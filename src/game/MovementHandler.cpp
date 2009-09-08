/*
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Log.h"
#include "Corpse.h"
#include "Player.h"
#include "Vehicle.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "Transports.h"
#include "BattleGround.h"
#include "WaypointMovementGenerator.h"
#include "InstanceSaveMgr.h"
#include "ObjectMgr.h"

// FG: req. for ACH
#include "World.h"
#include "Language.h"
#include "Chat.h"

enum AnticheatTypeFlags
{
    ACH_TYPE_NOTHING = 0x0000,
    ACH_TYPE_SPEED = 0x0001,
    ACH_TYPE_TELEPORT = 0x0002,
    ACH_TYPE_MOUNTAIN = 0x0004,
    ACH_TYPE_GRAVITY = 0x0008,
    ACH_TYPE_MULTIJUMP = 0x0010,
    ACH_TYPE_FLY = 0x0020,
    ACH_TYPE_PLANE = 0x0040,
    ACH_TYPE_WATERWALK = 0x0080,
    ACH_TYPE_MISTIMING = 0x0100
};

void WorldSession::HandleMoveWorldportAckOpcode( WorldPacket & /*recv_data*/ )
{
    sLog.outDebug( "WORLD: got MSG_MOVE_WORLDPORT_ACK." );
    HandleMoveWorldportAckOpcode();
}

void WorldSession::HandleMoveWorldportAckOpcode()
{
    // ignore unexpected far teleports
    if(!GetPlayer()->IsBeingTeleportedFar())
        return;

    // get the teleport destination
    WorldLocation &loc = GetPlayer()->GetTeleportDest();

    // possible errors in the coordinate validity check
    if(!MapManager::IsValidMapCoord(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation))
    {
        LogoutPlayer(false);
        return;
    }

    // get the destination map entry, not the current one, this will fix homebind and reset greeting
    MapEntry const* mEntry = sMapStore.LookupEntry(loc.mapid);
    InstanceTemplate const* mInstance = objmgr.GetInstanceTemplate(loc.mapid);

    // reset instance validity, except if going to an instance inside an instance
    if(GetPlayer()->m_InstanceValid == false && !mInstance)
        GetPlayer()->m_InstanceValid = true;

    GetPlayer()->SetSemaphoreTeleportFar(false);

    // relocate the player to the teleport destination
    GetPlayer()->SetMap(MapManager::Instance().CreateMap(loc.mapid, GetPlayer()));    GetPlayer()->Relocate(loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation);
    GetPlayer()->Relocate(loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation);

    GetPlayer()->SendInitialPacketsBeforeAddToMap();
    // the CanEnter checks are done in TeleporTo but conditions may change
    // while the player is in transit, for example the map may get full
    if(!GetPlayer()->GetMap()->Add(GetPlayer()))
    {
        //if player wasn't added to map, reset his map pointer!
        GetPlayer()->ResetMap();

        sLog.outDebug("WORLD: teleport of player %s (%d) to location %d, %f, %f, %f, %f failed", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation);
        // teleport the player home
        if(!GetPlayer()->TeleportTo(GetPlayer()->m_homebindMapId, GetPlayer()->m_homebindX, GetPlayer()->m_homebindY, GetPlayer()->m_homebindZ, GetPlayer()->GetOrientation()))
        {
            // the player must always be able to teleport home
            sLog.outError("WORLD: failed to teleport player %s (%d) to homebind location %d, %f, %f, %f, %f!", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow(), GetPlayer()->m_homebindMapId, GetPlayer()->m_homebindX, GetPlayer()->m_homebindY, GetPlayer()->m_homebindZ, GetPlayer()->GetOrientation());
            assert(false);
        }
        return;
    }

    // battleground state prepare (in case join to BG), at relogin/tele player not invited
    // only add to bg group and object, if the player was invited (else he entered through command)
    if(_player->InBattleGround())
    {
        // cleanup setting if outdated
        if(!mEntry->IsBattleGroundOrArena())
        {
            // We're not in BG
            _player->SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
            // reset destination bg team
            _player->SetBGTeam(0);
        }
        // join to bg case
        else if(BattleGround *bg = _player->GetBattleGround())
        {
            if(_player->IsInvitedForBattleGroundInstance(_player->GetBattleGroundId()))
                bg->AddPlayer(_player);
        }
    }

    GetPlayer()->SendInitialPacketsAfterAddToMap();

    // flight fast teleport case
    if(GetPlayer()->GetMotionMaster()->GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE)
    {
        if(!_player->InBattleGround())
        {
            // short preparations to continue flight
            FlightPathMovementGenerator* flight = (FlightPathMovementGenerator*)(GetPlayer()->GetMotionMaster()->top());
            flight->Initialize(*GetPlayer());
            return;
        }

        // battleground state prepare, stop flight
        GetPlayer()->GetMotionMaster()->MovementExpired();
        GetPlayer()->m_taxi.ClearTaxiDestinations();
    }

    // resurrect character at enter into instance where his corpse exist after add to map
    Corpse *corpse = GetPlayer()->GetCorpse();
    if (corpse && corpse->GetType() != CORPSE_BONES && corpse->GetMapId() == GetPlayer()->GetMapId())
    {
        if( mEntry->IsDungeon() )
        {
            GetPlayer()->ResurrectPlayer(0.5f);
            GetPlayer()->SpawnCorpseBones();
            GetPlayer()->SaveToDB();
        }
    }

    if((mEntry->IsRaid() || (mEntry->IsNonRaidDungeon() && mEntry->SupportsHeroicMode() && GetPlayer()->IsHeroic())) && mInstance)
    {
        uint32 timeleft = sInstanceSaveManager.GetResetTimeFor(GetPlayer()->GetMapId()) - time(NULL);
        GetPlayer()->SendInstanceResetWarning(GetPlayer()->GetMapId(), GetPlayer()->GetDifficulty(), timeleft);
    }

    // mount allow check
    if(!mEntry->IsMountAllowed())
        _player->RemoveSpellsCausingAura(SPELL_AURA_MOUNTED);

    // honorless target
    if(GetPlayer()->pvpInfo.inHostileArea)
        GetPlayer()->CastSpell(GetPlayer(), 2479, true);

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    //lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();
}

void WorldSession::HandleMoveTeleportAck(WorldPacket& recv_data)
{
    sLog.outDebug("MSG_MOVE_TELEPORT_ACK");
    uint64 guid;
    uint32 flags, time;

    recv_data >> guid;
    recv_data >> flags >> time;
    DEBUG_LOG("Guid " UI64FMTD, guid);
    DEBUG_LOG("Flags %u, time %u", flags, time/IN_MILISECONDS);

    Unit *mover = _player->m_mover;
    Player *plMover = mover->GetTypeId() == TYPEID_PLAYER ? (Player*)mover : NULL;

    if(!plMover || !plMover->IsBeingTeleportedNear())
        return;

    if(guid != plMover->GetGUID())
        return;

    plMover->SetSemaphoreTeleportNear(false);

    uint32 old_zone = plMover->GetZoneId();

    WorldLocation const& dest = plMover->GetTeleportDest();

    plMover->SetPosition(dest.coord_x, dest.coord_y, dest.coord_z, dest.orientation, true);

    uint32 newzone, newarea;
    plMover->GetZoneAndAreaId(newzone, newarea);
    plMover->UpdateZone(newzone, newarea);

    // new zone
    if(old_zone != newzone)
    {
        // honorless target
        if(plMover->pvpInfo.inHostileArea)
            plMover->CastSpell(plMover, 2479, true);
    }

    // resummon pet
    GetPlayer()->ResummonPetTemporaryUnSummonedIfAny();

    //lets process all delayed operations on successful teleport
    GetPlayer()->ProcessDelayedOperations();
}

void WorldSession::HandleMovementOpcodes( WorldPacket & recv_data )
{
    uint32 opcode = recv_data.GetOpcode();
    sLog.outDebug("WORLD: Recvd %s (%u, 0x%X) opcode", LookupOpcodeName(opcode), opcode, opcode);

    Unit *mover = _player->m_mover;
    Player *plMover = mover->GetTypeId()==TYPEID_PLAYER ? (Player*)mover : NULL;

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode and WorldSession::HandleMoveTeleportAck
    if(plMover && plMover->IsBeingTeleported())
    {
        plMover->m_anti_JustTeleported = 1; // ACH related
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    /* extract packet */
    MovementInfo movementInfo;
    ReadMovementInfo(recv_data, &movementInfo);
    /*----------------*/

    if(recv_data.size() != recv_data.rpos())
    {
        sLog.outError("MovementHandler: player %s (guid %d, account %u) sent a packet (opcode %u) that is " SIZEFMTD " bytes larger than it should be. Kicked as cheater.", _player->GetName(), _player->GetGUIDLow(), _player->GetSession()->GetAccountId(), recv_data.GetOpcode(), recv_data.size() - recv_data.rpos());
        KickPlayer();
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    if (!MaNGOS::IsValidMapCoord(movementInfo.x, movementInfo.y, movementInfo.z, movementInfo.o))
    {
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    /* handle special cases */
    if (movementInfo.HasMovementFlag(MOVEMENTFLAG_ONTRANSPORT))
    {
        // transports size limited
        // (also received at zeppelin leave by some reason with t_* as absolute in continent coordinates, can be safely skipped)
        if( movementInfo.t_x > 50 || movementInfo.t_y > 50 || movementInfo.t_z > 50 )
        {
            recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
            return;
        }

        if( !MaNGOS::IsValidMapCoord(movementInfo.x+movementInfo.t_x, movementInfo.y + movementInfo.t_y,
            movementInfo.z + movementInfo.t_z, movementInfo.o + movementInfo.t_o) )
        {
            recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
            return;
        }

        // if we boarded a transport, add us to it
        if (plMover && plMover->m_anti_TransportGUID == 0 && movementInfo.t_guid && !plMover->m_transport)
        {
            // elevators also cause the client to send MOVEMENTFLAG_ONTRANSPORT - just unmount if the guid can be found in the transport list
            for (MapManager::TransportSet::const_iterator iter = MapManager::Instance().m_Transports.begin(); iter != MapManager::Instance().m_Transports.end(); ++iter)
            {
                if ((*iter)->GetGUID() == movementInfo.t_guid)
                {
                    plMover->m_transport = (*iter);
                    (*iter)->AddPassenger(plMover);
                    break;
                }
            }
        }
        //movement anticheat;
        if(movementInfo.t_guid)
        {
            //Correct finding GO guid in DB (thanks to GriffonHeart)
            GameObject *obj = HashMapHolder<GameObject>::Find(movementInfo.t_guid);
            if(obj)
                plMover->m_anti_TransportGUID = obj->GetDBTableGUIDLow();
            else
                plMover->m_anti_TransportGUID = GUID_LOPART(movementInfo.t_guid);
        }
        // end movement anticheat
    }
    else if (plMover && (plMover->m_anti_TransportGUID || plMover->m_transport))               // if we were on a transport, leave
    {
        if(plMover->m_transport)
        {
            plMover->m_transport->RemovePassenger(plMover);
            plMover->m_transport = NULL;
        }
        movementInfo.t_x = 0.0f;
        movementInfo.t_y = 0.0f;
        movementInfo.t_z = 0.0f;
        movementInfo.t_o = 0.0f;
        movementInfo.t_time = 0;
        movementInfo.t_seat = -1;
        plMover->m_anti_TransportGUID = 0;
    }

    // fall damage generation (ignore in flight case that can be triggered also at lags in moment teleportation to another map).
    if (opcode == MSG_MOVE_FALL_LAND && plMover && !plMover->isInFlight())
    {
        //movement anticheat
        plMover->m_anti_JustJumped = 0;
        plMover->m_anti_JumpBaseZ = 0;
        //end movement anticheat
        plMover->HandleFall(movementInfo);
    }

    if (plMover && (movementInfo.HasMovementFlag(MOVEMENTFLAG_SWIMMING) != plMover->IsInWater()))
    {
        // now client not include swimming flag in case jumping under water
        plMover->SetInWater( !plMover->IsInWater() || plMover->GetBaseMap()->IsUnderWater(movementInfo.x, movementInfo.y, movementInfo.z) );
    }

    /*----------------------*/

    // ACH start
    uint32 alarm_level = 0;
    if(plMover && plMover == GetPlayer())
    {
        alarm_level = ACH_CheckMoveInfo(opcode, movementInfo, plMover);

        if (alarm_level)
        {
            plMover->m_anti_AlarmCount += alarm_level;
            /*
            WorldPacket data;
            plMover->SetUnitMovementFlags(0);
            plMover->BuildTeleportAckMsg(&data, plMover->GetPositionX(), plMover->GetPositionY(), plMover->GetPositionZ(), plMover->GetOrientation());
            plMover->GetSession()->SendPacket(&data);
            plMover->BuildHeartBeatMsg(&data);
            plMover->SendMessageToSet(&data, true);
            return;
            */
            // ... we should process packets normally, dont make them suspicious
            
            clock_t curclock = clock();

            if(plMover->m_anti_NotificationTime + sWorld.getConfig(CONFIG_ACH_NOTIFY_INTERVAL) < curclock)
            {
                plMover->m_anti_NotificationCount = 0;
            }

            if( plMover->m_anti_AlarmCount >= sWorld.getConfig(CONFIG_ACH_NOTIFY_IMPACT_3) && plMover->m_anti_NotificationCount == 2
             || plMover->m_anti_AlarmCount >= sWorld.getConfig(CONFIG_ACH_NOTIFY_IMPACT_2) && plMover->m_anti_NotificationCount == 1
             || plMover->m_anti_AlarmCount >= sWorld.getConfig(CONFIG_ACH_NOTIFY_IMPACT_1) && plMover->m_anti_NotificationCount == 0
                )
            {
                std::stringstream nameLink, cheatTypes;
                nameLink << "|cffffffff|Hplayer:" << plMover->GetName() << "|h[" <<  plMover->GetName() << "]|h|r";
                if(plMover->m_anti_TypeFlags & ACH_TYPE_SPEED) cheatTypes << "speed ";
                if(plMover->m_anti_TypeFlags & ACH_TYPE_TELEPORT) cheatTypes << "teleport ";
                if(plMover->m_anti_TypeFlags & ACH_TYPE_MOUNTAIN) cheatTypes << "mountain ";
                if(plMover->m_anti_TypeFlags & ACH_TYPE_GRAVITY) cheatTypes << "gravity ";
                if(plMover->m_anti_TypeFlags & ACH_TYPE_MULTIJUMP) cheatTypes << "multijump ";
                if(plMover->m_anti_TypeFlags & ACH_TYPE_FLY) cheatTypes << "fly ";
                if(plMover->m_anti_TypeFlags & ACH_TYPE_PLANE) cheatTypes << "plane ";
                if(plMover->m_anti_TypeFlags & ACH_TYPE_WATERWALK) cheatTypes << "waterwalk ";
                if(plMover->m_anti_TypeFlags & ACH_TYPE_MISTIMING) cheatTypes << "mistiming ";
                sWorld.SendHaxNotification(LANG_GM_HAX_MESSAGE, plMover->GetSession()->GetAccountId(), nameLink.str().c_str(), plMover->m_anti_AlarmCount, cheatTypes.str().c_str());
                plMover->m_anti_NotificationCount++;
                plMover->m_anti_NotificationTime = curclock;
            }
        }

        if (alarm_level == 0 && plMover->m_anti_AlarmCount > 0)
        {
            sLog.outError("MA-%s produced total alarm level %u",plMover->GetName(),plMover->m_anti_AlarmCount);
            plMover->m_anti_AlarmCount = 0;
            plMover->m_anti_TypeFlags = ACH_TYPE_NOTHING;
        }
    }
    // ACH end

    /* process position-change */
    
    recv_data.put<uint32>(6, getMSTime());                  // fix time, offset flags(4) + unk(2)
    WorldPacket data(recv_data.GetOpcode(), (mover->GetPackGUID().size()+recv_data.size()));
    data.append(mover->GetPackGUID());                      // use mover guid
    data.append(recv_data.contents(), recv_data.size());
    GetPlayer()->SendMessageToSet(&data, false);

    if(plMover)                                             // nothing is charmed, or player charmed
    {
        plMover->SetPosition(movementInfo.x, movementInfo.y, movementInfo.z, movementInfo.o);
        plMover->m_movementInfo = movementInfo;
        plMover->UpdateFallInformationIfNeed(movementInfo, recv_data.GetOpcode());

        if(plMover->isMovingOrTurning())
            plMover->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);

        if(movementInfo.z < -500.0f)
        {
            if(plMover->InBattleGround()
                && plMover->GetBattleGround()
                && plMover->GetBattleGround()->HandlePlayerUnderMap(_player))
            {
                // do nothing, the handle already did if returned true
            }
            else
            {
                // NOTE: this is actually called many times while falling
                // even after the player has been teleported away
                // TODO: discard movement packets after the player is rooted
                if(plMover->isAlive())
                {
                    plMover->EnvironmentalDamage(DAMAGE_FALL_TO_VOID, GetPlayer()->GetMaxHealth());
                    // pl can be alive if GM/etc
                    if(!plMover->isAlive())
                    {
                        // change the death state to CORPSE to prevent the death timer from
                        // starting in the next player update
                        plMover->KillPlayer();
                        plMover->BuildPlayerRepop();
                    }
                }

                // cancel the death timer here if started
                plMover->RepopAtGraveyard();
            }
        }
    }
    else                                                    // creature charmed
    {
        if(mover->IsInWorld())
            mover->GetMap()->CreatureRelocation((Creature*)mover, movementInfo.x, movementInfo.y, movementInfo.z, movementInfo.o);
    }
}

void WorldSession::HandleForceSpeedChangeAck(WorldPacket &recv_data)
{
    sLog.outDebug("WORLD: Recvd %s (%u, 0x%X) opcode", LookupOpcodeName(recv_data.GetOpcode()), recv_data.GetOpcode(), recv_data.GetOpcode());

    /* extract packet */
    uint64 guid;
    uint32 unk1;
    float  newspeed;

    recv_data >> guid;

    // now can skip not our packet
    if(_player->GetGUID() != guid)
    {
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    // continue parse packet

    recv_data >> unk1;                                      // counter or moveEvent

    MovementInfo movementInfo;
    ReadMovementInfo(recv_data, &movementInfo);

    recv_data >> newspeed;
    /*----------------*/

    // client ACK send one packet for mounted/run case and need skip all except last from its
    // in other cases anti-cheat check can be fail in false case
    UnitMoveType move_type;
    UnitMoveType force_move_type;

    static char const* move_type_name[MAX_MOVE_TYPE] = {  "Walk", "Run", "RunBack", "Swim", "SwimBack", "TurnRate", "Flight", "FlightBack", "PitchRate" };

    uint16 opcode = recv_data.GetOpcode();
    switch(opcode)
    {
        case CMSG_FORCE_WALK_SPEED_CHANGE_ACK:          move_type = MOVE_WALK;          force_move_type = MOVE_WALK;        break;
        case CMSG_FORCE_RUN_SPEED_CHANGE_ACK:           move_type = MOVE_RUN;           force_move_type = MOVE_RUN;         break;
        case CMSG_FORCE_RUN_BACK_SPEED_CHANGE_ACK:      move_type = MOVE_RUN_BACK;      force_move_type = MOVE_RUN_BACK;    break;
        case CMSG_FORCE_SWIM_SPEED_CHANGE_ACK:          move_type = MOVE_SWIM;          force_move_type = MOVE_SWIM;        break;
        case CMSG_FORCE_SWIM_BACK_SPEED_CHANGE_ACK:     move_type = MOVE_SWIM_BACK;     force_move_type = MOVE_SWIM_BACK;   break;
        case CMSG_FORCE_TURN_RATE_CHANGE_ACK:           move_type = MOVE_TURN_RATE;     force_move_type = MOVE_TURN_RATE;   break;
        case CMSG_FORCE_FLIGHT_SPEED_CHANGE_ACK:        move_type = MOVE_FLIGHT;        force_move_type = MOVE_FLIGHT;      break;
        case CMSG_FORCE_FLIGHT_BACK_SPEED_CHANGE_ACK:   move_type = MOVE_FLIGHT_BACK;   force_move_type = MOVE_FLIGHT_BACK; break;
        case CMSG_FORCE_PITCH_RATE_CHANGE_ACK:          move_type = MOVE_PITCH_RATE;    force_move_type = MOVE_PITCH_RATE;  break;
        default:
            sLog.outError("WorldSession::HandleForceSpeedChangeAck: Unknown move type opcode: %u", opcode);
            return;
    }

    // skip all forced speed changes except last and unexpected
    // in run/mounted case used one ACK and it must be skipped.m_forced_speed_changes[MOVE_RUN} store both.
    if(_player->m_forced_speed_changes[force_move_type] > 0)
    {
        --_player->m_forced_speed_changes[force_move_type];
        if(_player->m_forced_speed_changes[force_move_type] > 0)
            return;
    }

    if (!_player->GetTransport() && fabs(_player->GetSpeed(move_type) - newspeed) > 0.01f)
    {
        if(_player->GetSpeed(move_type) > newspeed)         // must be greater - just correct
        {
            sLog.outError("%sSpeedChange player %s is NOT correct (must be %f instead %f), force set to correct value",
                move_type_name[move_type], _player->GetName(), _player->GetSpeed(move_type), newspeed);
            _player->SetSpeed(move_type,_player->GetSpeedRate(move_type),true);
        }
        else                                                // must be lesser - cheating
        {
            sLog.outBasic("Player %s from account id %u kicked for incorrect speed (must be %f instead %f)",
                _player->GetName(),_player->GetSession()->GetAccountId(),_player->GetSpeed(move_type), newspeed);
            _player->GetSession()->KickPlayer();
        }
    }
}

void WorldSession::HandleSetActiveMoverOpcode(WorldPacket &recv_data)
{
    sLog.outDebug("WORLD: Recvd CMSG_SET_ACTIVE_MOVER");
    recv_data.hexlike();

    uint64 guid;
    recv_data >> guid;

    if(_player->m_mover->GetGUID() != guid)
    {
        sLog.outError("HandleSetActiveMoverOpcode: incorrect mover guid: mover is " I64FMT " and should be " I64FMT, _player->m_mover->GetGUID(), guid);
        return;
    }
}

void WorldSession::HandleMoveNotActiveMover(WorldPacket &recv_data)
{
    sLog.outDebug("WORLD: Recvd CMSG_MOVE_NOT_ACTIVE_MOVER");
    recv_data.hexlike();

    uint64 old_mover_guid;
    recv_data >> old_mover_guid;

    if(_player->m_mover->GetGUID() == old_mover_guid)
    {
        sLog.outError("HandleMoveNotActiveMover: incorrect mover guid: mover is " I64FMT " and should be " I64FMT " instead of " UI64FMTD, _player->m_mover->GetGUID(), _player->GetGUID(), old_mover_guid);
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    MovementInfo mi;
    ReadMovementInfo(recv_data, &mi);
    _player->m_movementInfo = mi;
}

void WorldSession::HandleDismissControlledVehicle(WorldPacket &recv_data)
{
    sLog.outDebug("WORLD: Recvd CMSG_DISMISS_CONTROLLED_VEHICLE");
    recv_data.hexlike();

    uint64 vehicleGUID = _player->GetCharmGUID();

    if(!vehicleGUID)                                        // something wrong here...
    {
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    MovementInfo mi;
    ReadMovementInfo(recv_data, &mi);
    _player->m_movementInfo = mi;

    // using charm guid, because we don't have vehicle guid...
    if(Vehicle *vehicle = ObjectAccessor::GetVehicle(vehicleGUID))
    {
        // Aura::HandleAuraControlVehicle will call Player::ExitVehicle
        vehicle->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE);
    }
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket& /*recvdata*/)
{
    //sLog.outDebug("WORLD: Recvd CMSG_MOUNTSPECIAL_ANIM");

    WorldPacket data(SMSG_MOUNTSPECIAL_ANIM, 8);
    data << uint64(GetPlayer()->GetGUID());

    GetPlayer()->SendMessageToSet(&data, false);
}

void WorldSession::HandleMoveKnockBackAck( WorldPacket & recv_data )
{
    sLog.outDebug("CMSG_MOVE_KNOCK_BACK_ACK");

    recv_data.read_skip<uint64>();                          // guid
    recv_data.read_skip<uint32>();                          // unk

    MovementInfo movementInfo;
    ReadMovementInfo(recv_data, &movementInfo);

    // ACH related
    _player->m_movementInfo = movementInfo;
    _player->m_anti_Last_HSpeed = movementInfo.j_xyspeed;
    _player->m_anti_Last_VSpeed = movementInfo.j_unk < 3.2f ? movementInfo.j_unk - 1.0f : 3.2f;

    uint32 dt = (_player->m_anti_Last_VSpeed < 0) ? (int)(ceil(_player->m_anti_Last_VSpeed/-25)*1000) : (int)(ceil(_player->m_anti_Last_VSpeed/25)*1000);
    _player->m_anti_LastSpeedChangeTime = movementInfo.time + dt + 1000;
}

void WorldSession::HandleMoveHoverAck( WorldPacket& recv_data )
{
    sLog.outDebug("CMSG_MOVE_HOVER_ACK");

    recv_data.read_skip<uint64>();                          // guid
    recv_data.read_skip<uint32>();                          // unk

    MovementInfo movementInfo;
    ReadMovementInfo(recv_data, &movementInfo);

    recv_data.read_skip<uint32>();                          // unk2
}

void WorldSession::HandleMoveWaterWalkAck(WorldPacket& recv_data)
{
    sLog.outDebug("CMSG_MOVE_WATER_WALK_ACK");

    recv_data.read_skip<uint64>();                          // guid
    recv_data.read_skip<uint32>();                          // unk

    MovementInfo movementInfo;
    ReadMovementInfo(recv_data, &movementInfo);

    recv_data.read_skip<uint32>();                          // unk2
}

void WorldSession::HandleSummonResponseOpcode(WorldPacket& recv_data)
{
    if(!_player->isAlive() || _player->isInCombat() )
        return;

    uint64 summoner_guid;
    bool agree;
    recv_data >> summoner_guid;
    recv_data >> agree;

    _player->SummonIfPossible(agree);
}


uint32 WorldSession::ACH_CheckMoveInfo(uint32 opcode, MovementInfo& movementInfo, Player *plMover)
{
    if(!plMover) // player to check
        return true; // this does not necessarily have to be an error, just return anything here, as long as it works

    uint32 alarm_level = 0; // "impact" generated, > 0 for suspicious packets

    //calc time deltas
    int32 cClientTimeDelta = 1500;
    if (plMover->m_anti_LastClientTime)
    {
        cClientTimeDelta = movementInfo.time - plMover->m_anti_LastClientTime;
        plMover->m_anti_DeltaClientTime += cClientTimeDelta;
        plMover->m_anti_LastClientTime = movementInfo.time;
    }
    else
    {
        plMover->m_anti_LastClientTime = movementInfo.time;
    }

    uint32 cServerTime=getMSTime();
    uint32 cServerTimeDelta = 1500;
    if (plMover->m_anti_LastServerTime != 0)
    {
        cServerTimeDelta = cServerTime - plMover->m_anti_LastServerTime;
        plMover->m_anti_DeltaServerTime += cServerTimeDelta;
        plMover->m_anti_LastServerTime = cServerTime;
    }
    else
    {
        plMover->m_anti_LastServerTime = cServerTime;
    }

    //resync times on client login (first 90 sec for heavy areas)
    if (plMover->m_anti_DeltaServerTime < 90000 && plMover->m_anti_DeltaClientTime < 0000)
        plMover->m_anti_DeltaClientTime = plMover->m_anti_DeltaServerTime;

    int32 sync_time = plMover->m_anti_DeltaClientTime - plMover->m_anti_DeltaServerTime;

    //mistiming checks
    int32 gmd = sWorld.getConfig(CONFIG_ACH_MISTIMING_DELTA);
    if (sync_time > gmd || sync_time < -gmd)
    {
        cClientTimeDelta = cServerTimeDelta;
        plMover->m_anti_MistimingCount++;

        /*sLog.outError("MA-%s, mistiming exception. #:%d, mistiming: %dms ",
            plMover->GetName(), plMover->m_anti_MistimingCount, sync_time);*/

        plMover->m_anti_TypeFlags |= ACH_TYPE_MISTIMING;

        alarm_level += (sWorld.getConfig(CONFIG_ACH_IMPACT_MISTIMING_FLAT) +
            (plMover->m_anti_MistimingCount * sWorld.getConfig(CONFIG_ACH_IMPACT_MISTIMING_MULTI)));

    }
    // end mistiming checks


    uint32 curDest = plMover->m_taxi.GetTaxiDestination(); //check taxi flight
    if ((plMover->m_anti_TransportGUID == 0) && !curDest)
    {
        UnitMoveType move_type;

        // calculating section ---------------------
        //current speed
        if (movementInfo.flags & MOVEMENTFLAG_FLYING) move_type = movementInfo.flags & MOVEMENTFLAG_BACKWARD ? MOVE_FLIGHT_BACK : MOVE_FLIGHT;
        else if (movementInfo.flags & MOVEMENTFLAG_SWIMMING) move_type = movementInfo.flags & MOVEMENTFLAG_BACKWARD ? MOVE_SWIM_BACK : MOVE_SWIM;
        else if (movementInfo.flags & MOVEMENTFLAG_WALK_MODE) move_type = MOVE_WALK;
        //hmm... in first time after login player has MOVE_SWIMBACK instead MOVE_WALKBACK
        else move_type = movementInfo.flags & MOVEMENTFLAG_BACKWARD ? MOVE_SWIM_BACK : MOVE_RUN;

        float current_speed = plMover->GetSpeed(move_type);
        // end current speed

        // movement distance
        float allowed_delta= 0;

        float delta_x = plMover->GetPositionX() - movementInfo.x;
        float delta_y = plMover->GetPositionY() - movementInfo.y;
        float delta_z = plMover->GetPositionZ() - movementInfo.z;
        float real_delta = delta_x * delta_x + delta_y * delta_y;
        float tg_z = -99999; //tangens
        // end movement distance

        if (cClientTimeDelta < 0)
            cClientTimeDelta = 0;
        float time_delta = (cClientTimeDelta < 1500) ? (float)cClientTimeDelta/1000 : 1.5f; //normalize time - 1.5 second allowed for heavy loaded server

        if (!(movementInfo.flags & (MOVEMENTFLAG_FLYING | MOVEMENTFLAG_SWIMMING)))
            tg_z = (real_delta !=0) ? (delta_z*delta_z / real_delta) : -99999;

        if (current_speed < plMover->m_anti_Last_HSpeed)
        {
            allowed_delta = plMover->m_anti_Last_HSpeed;
            if (plMover->m_anti_LastSpeedChangeTime == 0 )
                plMover->m_anti_LastSpeedChangeTime = movementInfo.time + (uint32)floor(((plMover->m_anti_Last_HSpeed / current_speed) * 1500)) + 100; //100ms above for random fluctuating =)))
        }
        else
        {
            allowed_delta = current_speed;
        }
        allowed_delta = allowed_delta * time_delta;
        allowed_delta = allowed_delta * allowed_delta + 2;
        if (tg_z > 2.2)
            allowed_delta = allowed_delta + (delta_z*delta_z)/2.37; // mountain fall allowed speed

        if (movementInfo.time>plMover->m_anti_LastSpeedChangeTime)
        {
            plMover->m_anti_Last_HSpeed = current_speed; // store current speed
            plMover->m_anti_Last_VSpeed = -2.3f;
            if (plMover->m_anti_LastSpeedChangeTime != 0)
                plMover->m_anti_LastSpeedChangeTime = 0;
        }
        // end calculating section ---------------------

        //AntiGrav
        float JumpHeight = plMover->m_anti_JumpBaseZ - movementInfo.z;
        if ((plMover->m_anti_JumpBaseZ != 0)
            && !(movementInfo.flags & (MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_FLYING2))
            && (JumpHeight < plMover->m_anti_Last_VSpeed))
        {
            sLog.outError("MA-%s, GraviJump exception. JumpHeight = %f, Allowed Veritcal Speed = %f",
                plMover->GetName(), JumpHeight, plMover->m_anti_Last_VSpeed);

            plMover->m_anti_TypeFlags |= ACH_TYPE_GRAVITY;

            //plMover->GetSession()->ACH_HandleGravity()

            alarm_level += sWorld.getConfig(CONFIG_ACH_IMPACT_GRAVITY);
        }

        //multi jump checks
        if (opcode == MSG_MOVE_JUMP && !plMover->IsInWater())
        {
            if (plMover->m_anti_JustJumped >= 1)
            {
                alarm_level += sWorld.getConfig(CONFIG_ACH_IMPACT_MULTIJUMP);
                sLog.outError("MA-%s, MultiJump exception. Jumps = %u", plMover->GetName(), plMover->m_anti_JustJumped);
                plMover->m_anti_TypeFlags |= ACH_TYPE_MULTIJUMP;
                //plMover->GetSession()->ACH_HandleMultiJump()
            }
            else
            {
                plMover->m_anti_JustJumped += 1;
                plMover->m_anti_JumpBaseZ = movementInfo.z;
            }
        }
        else if (plMover->IsInWater())
        {
            plMover->m_anti_JustJumped = 0;
        }

        //speed hack checks
        if ((real_delta > allowed_delta)) // && (delta_z < 0))
        {
            sLog.outError("MA-%s, speed exception | cDelta=%f aDelta=%f | cSpeed=%f lSpeed=%f deltaTime=%f",
                plMover->GetName(), real_delta, allowed_delta, current_speed, plMover->m_anti_Last_HSpeed,time_delta);
            plMover->m_anti_TypeFlags |= ACH_TYPE_SPEED;
            //plMover->GetSession()->ACH_HandleSpeed()

            alarm_level += (sWorld.getConfig(CONFIG_ACH_IMPACT_SPEED_FLAT) + 
                (sqrt(real_delta - allowed_delta) * sWorld.getConfig(CONFIG_ACH_IMPACT_SPEED_MULTI)));
        }
        //teleport hack checks
        if ((real_delta>4900.0f) && !(real_delta < allowed_delta))
        {
            sLog.outError("MA-%s, is teleport exception | cDelta=%f aDelta=%f | cSpeed=%f lSpeed=%f deltaToime=%f",
                plMover->GetName(),real_delta, allowed_delta, current_speed, plMover->m_anti_Last_HSpeed,time_delta);
            plMover->m_anti_TypeFlags |= ACH_TYPE_TELEPORT;
            //plMover->GetSession()->ACH_HandleTeleport()
            // TODO: add more code if necessary. most of it should be covered above anyway
            //check_passed = false;
        }

        //mountian hack checks // 1.56f (delta_z < GetPlayer()->m_anti_Last_VSpeed))
        if ((delta_z < plMover->m_anti_Last_VSpeed) && (plMover->m_anti_JustJumped == 0) && (tg_z > 2.37f))
        {
            sLog.outError("MA-%s, mountain exception | tg_z=%f", plMover->GetName(),tg_z);
            plMover->m_anti_TypeFlags |= ACH_TYPE_MOUNTAIN;
            //plMover->GetSession()->ACH_HandleMountain()

            alarm_level += sWorld.getConfig(CONFIG_ACH_IMPACT_MOUNTAIN);
        }
        //Fly hack checks
        if (((movementInfo.flags & (MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_FLYING2)) != 0)
            && !plMover->isGameMaster()
            && !(plMover->HasAuraType(SPELL_AURA_FLY) || plMover->HasAuraType(SPELL_AURA_MOD_INCREASE_FLIGHT_SPEED)))
        {
            sLog.outError("MA-%s, flight exception.",plMover->GetName());
            plMover->m_anti_TypeFlags |= ACH_TYPE_FLY;
            //plMover->GetSession()->ACH_HandleFly()

            alarm_level += sWorld.getConfig(CONFIG_ACH_IMPACT_FLY);
        }
        //Water-Walk checks
        if (((movementInfo.flags & MOVEMENTFLAG_WATERWALKING) != 0)
            && !plMover->isGameMaster()
            && !(plMover->HasAuraType(SPELL_AURA_WATER_WALK) || plMover->HasAuraType(SPELL_AURA_GHOST) || plMover->HasAura(3714)))
        {
            sLog.outError("MA-%s, water-walk exception. [%X]{SPELL_AURA_WATER_WALK=[%X]}",
                plMover->GetName(), movementInfo.flags, plMover->HasAuraType(SPELL_AURA_WATER_WALK));
            plMover->m_anti_TypeFlags |= ACH_TYPE_WATERWALK;
            //plMover->GetSession()->ACH_HandleWaterwalk()

            alarm_level += sWorld.getConfig(CONFIG_ACH_IMPACT_WATERWALK);
        }
        //Teleport To Plane checks
        if (movementInfo.z < 0.0001f && movementInfo.z > -0.0001f
            && ((movementInfo.flags & (MOVEMENTFLAG_SWIMMING | MOVEMENTFLAG_CAN_FLY | MOVEMENTFLAG_FLYING | MOVEMENTFLAG_FLYING2)) == 0)
            && !plMover->isGameMaster())
        {
            // Prevent using TeleportToPlan.
            Map *map = plMover->GetMap();
            if (map){
                float plane_z = map->GetHeight(movementInfo.x, movementInfo.y, MAX_HEIGHT) - movementInfo.z;
                plane_z = (plane_z < -500.0f) ? 0 : plane_z; //check holes in heigth map
                if(plane_z > 0.1f || plane_z < -0.1f)
                {
                    plMover->m_anti_TeleToPlane_Count++;
                    alarm_level += (sWorld.getConfig(CONFIG_ACH_IMPACT_TELEPORT_TO_PLANE_FLAT) +
                        (sWorld.getConfig(CONFIG_ACH_IMPACT_TELEPORT_TO_PLANE_MULTI) * plMover->m_anti_TeleToPlane_Count));

                    sLog.outDebug("MA-%s, teleport to plane exception. plane_z: %f count=%u",
                        plMover->GetName(), plane_z, plMover->m_anti_TeleToPlane_Count);

                    plMover->m_anti_TypeFlags |= ACH_TYPE_PLANE;

                    //plMover->GetSession()->ACH_HandlePlaneTeleport()

                }
            }
        }
        else
        {
            if (plMover->m_anti_TeleToPlane_Count != 0)
                plMover->m_anti_TeleToPlane_Count = 0;
        }
    }
    else if (movementInfo.flags & MOVEMENTFLAG_ONTRANSPORT)
    {
        //antiwrap checks
        if (plMover->m_transport)
        {
            float trans_rad = movementInfo.t_x*movementInfo.t_x + movementInfo.t_y*movementInfo.t_y + movementInfo.t_z*movementInfo.t_z;
            if (trans_rad > 3600.0f)
            {
                //check_passed = false;

                sLog.outError("MA-%s, leave transport.", plMover->GetName());
            }
        }
        else
        {
            if (GameObjectData const* go_data = objmgr.GetGOData(plMover->m_anti_TransportGUID))
            {
                float delta_gox = go_data->posX - movementInfo.x;
                float delta_goy = go_data->posY - movementInfo.y;
                float delta_goz = go_data->posZ - movementInfo.z;
                int mapid = go_data->mapid;

                sLog.outDebug("MA-%s, transport movement. GO xyzo: %f,%f,%f",
                    plMover->GetName(), go_data->posX,go_data->posY,go_data->posZ);

                if (plMover->GetMapId() != mapid)
                {
                    //check_passed = false;
                }
                else if (mapid !=369)
                {
                    float delta_go = delta_gox*delta_gox + delta_goy*delta_goy;
                    if (delta_go > 3600.0f)
                    {
                        //check_passed = false;

                        sLog.outError("MA-%s, leave transport. GO xyzo: %f,%f,%f",
                            plMover->GetName(), go_data->posX,go_data->posY,go_data->posZ);

                    }
                }

            }
            else
            {

                sLog.outDebug("MA-%s, undefined transport.", plMover->GetName());

                //check_passed = false;
            }
        }

        /*if (!check_passed)
        {
            if (plMover->m_transport)
            {
                plMover->m_transport->RemovePassenger(plMover);
                plMover->m_transport = NULL;
            }
            movementInfo.t_x = 0.0f;
            movementInfo.t_y = 0.0f;
            movementInfo.t_z = 0.0f;
            movementInfo.t_o = 0.0f;
            movementInfo.t_time = 0;
            plMover->m_anti_TransportGUID = 0;
        }*/
    }
    return alarm_level;
}
