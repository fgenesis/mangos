/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
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
#include "MapPersistentStateMgr.h"
#include "ObjectMgr.h"

// FG: req. for ACH
#include "World.h"
#include "Language.h"

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
    DEBUG_LOG( "WORLD: got MSG_MOVE_WORLDPORT_ACK." );
    HandleMoveWorldportAckOpcode();
}

void WorldSession::HandleMoveWorldportAckOpcode()
{
    // ignore unexpected far teleports
    if(!GetPlayer()->IsBeingTeleportedFar())
        return;

    if (_player->GetVehicleKit())
        _player->GetVehicleKit()->RemoveAllPassengers();

    // get start teleport coordinates (will used later in fail case)
    WorldLocation old_loc;
    GetPlayer()->GetPosition(old_loc);

    // get the teleport destination
    WorldLocation &loc = GetPlayer()->GetTeleportDest();

    // possible errors in the coordinate validity check (only cheating case possible)
    if (!MapManager::IsValidMapCoord(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation))
    {
        sLog.outError("WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far to a not valid location "
            "(map:%u, x:%f, y:%f, z:%f) We port him to his homebind instead..",
            GetPlayer()->GetGuidStr().c_str(), loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z);
        // stop teleportation else we would try this again and again in LogoutPlayer...
        GetPlayer()->SetSemaphoreTeleportFar(false);
        // and teleport the player to a valid place
        GetPlayer()->TeleportToHomebind();
        return;
    }

    // get the destination map entry, not the current one, this will fix homebind and reset greeting
    MapEntry const* mEntry = sMapStore.LookupEntry(loc.mapid);

    Map* map = NULL;

    // prevent crash at attempt landing to not existed battleground instance
    if(mEntry->IsBattleGroundOrArena())
    {
        if (GetPlayer()->GetBattleGroundId())
            map = sMapMgr.FindMap(loc.mapid, GetPlayer()->GetBattleGroundId());

        if (!map)
        {
            DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far to nonexisten battleground instance "
                " (map:%u, x:%f, y:%f, z:%f) Trying to port him to his previous place..",
                GetPlayer()->GetGuidStr().c_str(), loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z);

            GetPlayer()->SetSemaphoreTeleportFar(false);

            // Teleport to previous place, if cannot be ported back TP to homebind place
            if (!GetPlayer()->TeleportTo(old_loc))
            {
                DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s cannot be ported to his previous place, teleporting him to his homebind place...",
                    GetPlayer()->GetGuidStr().c_str());
                GetPlayer()->TeleportToHomebind();
            }
            return;
        }
    }

    InstanceTemplate const* mInstance = ObjectMgr::GetInstanceTemplate(loc.mapid);

    // reset instance validity, except if going to an instance inside an instance
    if (GetPlayer()->m_InstanceValid == false && !mInstance)
        GetPlayer()->m_InstanceValid = true;

    GetPlayer()->SetSemaphoreTeleportFar(false);

    // relocate the player to the teleport destination
    if (!map)
        map = sMapMgr.CreateMap(loc.mapid, GetPlayer());

    GetPlayer()->SetMap(map);
    GetPlayer()->Relocate(loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation);

    GetPlayer()->SendInitialPacketsBeforeAddToMap();
    // the CanEnter checks are done in TeleporTo but conditions may change
    // while the player is in transit, for example the map may get full
    if (!GetPlayer()->GetMap()->Add(GetPlayer()))
    {
        // if player wasn't added to map, reset his map pointer!
        GetPlayer()->ResetMap();

        DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s was teleported far but couldn't be added to map "
            " (map:%u, x:%f, y:%f, z:%f) Trying to port him to his previous place..",
            GetPlayer()->GetGuidStr().c_str(), loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z);

        // Teleport to previous place, if cannot be ported back TP to homebind place
        if (!GetPlayer()->TeleportTo(old_loc))
        {
            DETAIL_LOG("WorldSession::HandleMoveWorldportAckOpcode: %s cannot be ported to his previous place, teleporting him to his homebind place...",
                GetPlayer()->GetGuidStr().c_str());
            GetPlayer()->TeleportToHomebind();
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
            _player->SetBGTeam(TEAM_NONE);
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
            flight->Reset(*GetPlayer());
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
        }
    }

    if (mInstance)
    {
        Difficulty diff = GetPlayer()->GetDifficulty(mEntry->IsRaid());
        if(MapDifficulty const* mapDiff = GetMapDifficultyData(mEntry->MapID,diff))
        {
            if (mapDiff->resetTime)
            {
                if (time_t timeReset = sMapPersistentStateMgr.GetScheduler().GetResetTimeFor(mEntry->MapID,diff))
                {
                    uint32 timeleft = uint32(timeReset - time(NULL));
                    GetPlayer()->SendInstanceResetWarning(mEntry->MapID, diff, timeleft);
                }
            }
        }
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

void WorldSession::HandleMoveTeleportAckOpcode(WorldPacket& recv_data)
{
    DEBUG_LOG("MSG_MOVE_TELEPORT_ACK");

    ObjectGuid guid;

    recv_data >> guid.ReadAsPacked();

    uint32 counter, time;
    recv_data >> counter >> time;
    DEBUG_LOG("Guid: %s", guid.GetString().c_str());
    DEBUG_LOG("Counter %u, time %u", counter, time/IN_MILLISECONDS);

    Unit *mover = _player->GetMover();
    Player *plMover = mover->GetTypeId() == TYPEID_PLAYER ? (Player*)mover : NULL;

    if(!plMover || !plMover->IsBeingTeleportedNear())
        return;

    if(guid != plMover->GetObjectGuid())
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
    DEBUG_LOG("WORLD: Recvd %s (%u, 0x%X) opcode", LookupOpcodeName(opcode), opcode, opcode);
    recv_data.hexlike();

    Unit *mover = _player->GetMover();
    Player *plMover = mover && mover->GetTypeId() == TYPEID_PLAYER ? (Player*)mover : NULL;

    bool beeingTele = false;
    if(plMover)
    {
        beeingTele = plMover->IsBeingTeleported();
        plMover->m_anti_JustTeleported = beeingTele; // ACH related
    }

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode and WorldSession::HandleMoveTeleportAck
    if(/*plMover &&*/ beeingTele)
    {
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    /* extract packet */
    ObjectGuid guid;
    MovementInfo movementInfo;

    recv_data >> guid.ReadAsPacked();
    recv_data >> movementInfo;
    /*----------------*/

    if (!VerifyMovementInfo(movementInfo, guid))
        return;

    // fall damage generation (ignore in flight case that can be triggered also at lags in moment teleportation to another map).
    if (opcode == MSG_MOVE_FALL_LAND && plMover && !plMover->IsTaxiFlying())
    {
        //movement anticheat
        plMover->m_anti_JustJumped = 0;
        plMover->m_anti_JumpBaseZ = 0;
        //end movement anticheat
        plMover->HandleFall(movementInfo);
    }

    // ACH start
    uint32 alarm_level = 0;
    if(plMover && plMover == GetPlayer())
    {
        alarm_level = ACH_CheckMoveInfo(opcode, &movementInfo, plMover);

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

            if(plMover->m_anti_NotificationTime + (int32)sWorld.getConfig(CONFIG_UINT32_ACH_NOTIFY_INTERVAL) < curclock)
            {
                plMover->m_anti_NotificationCount = 0;
            }

            if( plMover->m_anti_AlarmCount >= sWorld.getConfig(CONFIG_UINT32_ACH_NOTIFY_IMPACT_3) && plMover->m_anti_NotificationCount == 2
             || plMover->m_anti_AlarmCount >= sWorld.getConfig(CONFIG_UINT32_ACH_NOTIFY_IMPACT_2) && plMover->m_anti_NotificationCount == 1
             || plMover->m_anti_AlarmCount >= sWorld.getConfig(CONFIG_UINT32_ACH_NOTIFY_IMPACT_1) && plMover->m_anti_NotificationCount == 0
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
    HandleMoverRelocation(movementInfo);

    if (plMover)
        plMover->UpdateFallInformationIfNeed(movementInfo, opcode);

    // after move info set
    if (opcode == MSG_MOVE_SET_WALK_MODE || opcode == MSG_MOVE_SET_RUN_MODE)
        mover->UpdateWalkMode(mover, false);

    WorldPacket data(opcode, recv_data.size());
    data << mover->GetPackGUID();             // write guid
    movementInfo.Write(data);                               // write data
    mover->SendMessageToSetExcept(&data, _player);
}

void WorldSession::HandleForceSpeedChangeAckOpcodes(WorldPacket &recv_data)
{
    uint32 opcode = recv_data.GetOpcode();
    DEBUG_LOG("WORLD: Recvd %s (%u, 0x%X) opcode", LookupOpcodeName(opcode), opcode, opcode);
    /* extract packet */
    ObjectGuid guid;
    MovementInfo movementInfo;
    float  newspeed;

    recv_data >> guid.ReadAsPacked();
    recv_data >> Unused<uint32>();                          // counter or moveEvent
    recv_data >> movementInfo;
    recv_data >> newspeed;

    // now can skip not our packet
    if(_player->GetObjectGuid() != guid)
    {
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }
    /*----------------*/

    // client ACK send one packet for mounted/run case and need skip all except last from its
    // in other cases anti-cheat check can be fail in false case
    UnitMoveType move_type;
    UnitMoveType force_move_type;

    static char const* move_type_name[MAX_MOVE_TYPE] = {  "Walk", "Run", "RunBack", "Swim", "SwimBack", "TurnRate", "Flight", "FlightBack", "PitchRate" };

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
            _player->SetSpeedRate(move_type,_player->GetSpeedRate(move_type),true);
        }
        else                                                // must be lesser - cheating
        {
            BASIC_LOG("Player %s from account id %u kicked for incorrect speed (must be %f instead %f)",
                _player->GetName(),_player->GetSession()->GetAccountId(),_player->GetSpeed(move_type), newspeed);
            _player->GetSession()->KickPlayer();
        }
    }
}

void WorldSession::HandleSetActiveMoverOpcode(WorldPacket &recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_SET_ACTIVE_MOVER");
    recv_data.hexlike();

    ObjectGuid guid;
    recv_data >> guid;

    if(_player->GetMover()->GetObjectGuid() != guid)
    {
        sLog.outError("HandleSetActiveMoverOpcode: incorrect mover guid: mover is %s and should be %s",
            _player->GetMover()->GetGuidStr().c_str(), guid.GetString().c_str());
        return;
    }
}

void WorldSession::HandleMoveNotActiveMoverOpcode(WorldPacket &recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_MOVE_NOT_ACTIVE_MOVER");
    recv_data.hexlike();

    ObjectGuid old_mover_guid;
    MovementInfo mi;

    recv_data >> old_mover_guid.ReadAsPacked();
    recv_data >> mi;

    if(_player->GetMover()->GetObjectGuid() == old_mover_guid)
    {
        sLog.outError("HandleMoveNotActiveMover: incorrect mover guid: mover is %s and should be %s instead of %s",
            _player->GetMover()->GetGuidStr().c_str(),
            _player->GetGuidStr().c_str(),
            old_mover_guid.GetString().c_str());
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    _player->m_movementInfo = mi;
}


void WorldSession::HandleDismissControlledVehicle(WorldPacket &recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_DISMISS_CONTROLLED_VEHICLE");
    recv_data.hexlike();

    ObjectGuid guid;
    MovementInfo mi;

    recv_data >> guid.ReadAsPacked();
    recv_data >> mi;

    ObjectGuid vehicleGUID = _player->GetVehicleGUID();

    if (vehicleGUID.IsEmpty())                              // something wrong here...
        return;

    _player->m_movementInfo = mi;

    if(Vehicle *vehicle = _player->GetMap()->GetVehicle(vehicleGUID))
    {
        if(vehicle->GetVehicleFlags() & VF_DESPAWN_AT_LEAVE)
            vehicle->Dismiss();
        else
            ((Unit*)_player)->ExitVehicle();
    }
}

void WorldSession::HandleRequestVehicleExit(WorldPacket &recv_data)
{
    sLog.outDebug("WORLD: Recvd CMSG_REQUEST_VEHICLE_EXIT");
    recv_data.hexlike();

    ((Unit*)GetPlayer())->ExitVehicle();
}

void WorldSession::HandleRequestVehiclePrevSeat(WorldPacket &recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_REQUEST_VEHICLE_PREV_SEAT");
    recv_data.hexlike();

    GetPlayer()->ChangeSeat(-1, false);
}

void WorldSession::HandleRequestVehicleNextSeat(WorldPacket &recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_REQUEST_VEHICLE_NEXT_SEAT");
    recv_data.hexlike();

    GetPlayer()->ChangeSeat(-1, true);
}

void WorldSession::HandleRequestVehicleSwitchSeat(WorldPacket &recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_REQUEST_VEHICLE_SWITCH_SEAT");
    recv_data.hexlike();

    ObjectGuid vehicleGUID = _player->GetVehicleGUID();

    if(vehicleGUID.IsEmpty())                                        // something wrong here...
        return;

    if(Vehicle *vehicle = _player->GetMap()->GetVehicle(vehicleGUID))
    {
        ObjectGuid guid;
        recv_data >> guid.ReadAsPacked();

        int8 seatId = 0;
        recv_data >> seatId;

        if(!guid.IsEmpty())
        {
            if(vehicleGUID != guid.GetRawValue())
            {
                if(Vehicle *veh = _player->GetMap()->GetVehicle(guid.GetRawValue()))
                {
                    if(!_player->IsWithinDistInMap(veh, 10))
                        return;

                    if(Vehicle *v = veh->FindFreeSeat(&seatId, false))
                    {
                        vehicle->RemovePassenger(_player);
                        ((Unit*)_player)->EnterVehicle(v, seatId, false);
                    }
                }
                return;
            }
        }
        if(Vehicle *v = vehicle->FindFreeSeat(&seatId, false))
        {
            vehicle->RemovePassenger(_player);
            ((Unit*)_player)->EnterVehicle(v, seatId, false);
        }
    }
}

void WorldSession::HandleChangeSeatsOnControlledVehicle(WorldPacket &recv_data)
{
    sLog.outDebug("WORLD: Recvd CMSG_CHANGE_SEATS_ON_CONTROLLED_VEHICLE");
    recv_data.hexlike();

    ObjectGuid vehicleGUID = _player->GetVehicleGUID();

    if(vehicleGUID.IsEmpty())                                        // something wrong here...
        return;

    if(recv_data.GetOpcode() == CMSG_REQUEST_VEHICLE_PREV_SEAT)
    {
        _player->ChangeSeat(-1, false);
        return;
    }
    else if(recv_data.GetOpcode() == CMSG_REQUEST_VEHICLE_NEXT_SEAT)
    {
        _player->ChangeSeat(-1, true);
        return;
    }

    ObjectGuid guid, guid2;
    recv_data >> guid.ReadAsPacked();

    MovementInfo mi;
    recv_data >> mi;
    _player->m_movementInfo = mi;

    recv_data >> guid2.ReadAsPacked(); //guid of vehicle or of vehicle in target seat

    int8 seatId;
    recv_data >> seatId;

    if(guid.GetRawValue() == guid2.GetRawValue())
        _player->ChangeSeat(seatId, false);
    else if(Vehicle *vehicle = _player->GetMap()->GetVehicle(guid2.GetRawValue()))
    {
        if(vehicle->HasEmptySeat(seatId))
        {
            ((Unit*)_player)->ExitVehicle();
            ((Unit*)_player)->EnterVehicle(vehicle, seatId);
        }
    }
}

void WorldSession::HandleEnterPlayerVehicle(WorldPacket &recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_PLAYER_VEHICLE_ENTER");
    recv_data.hexlike();

    ObjectGuid guid;
    recv_data >> guid;

    if (Player* pl = ObjectAccessor::FindPlayer(guid))
    {
        if (!pl->GetVehicleKit())
            return;
        if (!pl->IsInSameRaidWith(GetPlayer()))
            return;
        if (!pl->IsWithinDistInMap(GetPlayer(), INTERACTION_DISTANCE))
            return;
        if (pl->GetTransport())
            return;
        ((Unit*)GetPlayer())->EnterVehicle(pl->GetVehicleKit());
    }
}

void WorldSession::HandleEjectPasenger(WorldPacket &recv_data)
{
    DEBUG_LOG("WORLD: Recvd CMSG_EJECT_PASSENGER");
    recv_data.hexlike();

    if(recv_data.GetOpcode()==CMSG_EJECT_PASSENGER)
    {
        if (GetPlayer()->GetVehicleKit())
        {
            ObjectGuid guid;
            recv_data >> guid;
            if(Player* Pl = ObjectAccessor::FindPlayer(guid))
                ((Unit*)Pl)->ExitVehicle();
        }
    }
}

void WorldSession::HandleMountSpecialAnimOpcode(WorldPacket& /*recvdata*/)
{
    //DEBUG_LOG("WORLD: Recvd CMSG_MOUNTSPECIAL_ANIM");

    WorldPacket data(SMSG_MOUNTSPECIAL_ANIM, 8);
    data << GetPlayer()->GetObjectGuid();

    GetPlayer()->SendMessageToSet(&data, false);
}

void WorldSession::HandleMoveKnockBackAck( WorldPacket & recv_data )
{
    DEBUG_LOG("CMSG_MOVE_KNOCK_BACK_ACK");

    Unit *mover = _player->GetMover();
    Player *plMover = mover->GetTypeId() == TYPEID_PLAYER ? (Player*)mover : NULL;

    // ignore, waiting processing in WorldSession::HandleMoveWorldportAckOpcode and WorldSession::HandleMoveTeleportAck
    if(plMover && plMover->IsBeingTeleported())
    {
        recv_data.rpos(recv_data.wpos());                   // prevent warnings spam
        return;
    }

    ObjectGuid guid;
    MovementInfo movementInfo;

    recv_data >> guid.ReadAsPacked();
    recv_data >> Unused<uint32>();                          // knockback packets counter
    recv_data >> movementInfo;

    // ACH related
    _player->m_movementInfo = movementInfo;
    _player->m_anti_Last_HSpeed = movementInfo.jump.xyspeed;
    _player->m_anti_Last_VSpeed = movementInfo.jump.velocity < 3.2f ? movementInfo.jump.velocity - 1.0f : 3.2f;

    uint32 dt = (_player->m_anti_Last_VSpeed < 0) ? (int)(ceil(_player->m_anti_Last_VSpeed/-25)*1000) : (int)(ceil(_player->m_anti_Last_VSpeed/25)*1000);
    _player->m_anti_LastSpeedChangeTime = movementInfo.GetTime() + dt + 1000;
    // end ACH related

    if (!VerifyMovementInfo(movementInfo, guid))
        return;

    HandleMoverRelocation(movementInfo);

    WorldPacket data(MSG_MOVE_KNOCK_BACK, recv_data.size() + 15);
    data << mover->GetPackGUID();
    data << movementInfo;
    data << movementInfo.GetJumpInfo().sinAngle;
    data << movementInfo.GetJumpInfo().cosAngle;
    data << movementInfo.GetJumpInfo().xyspeed;
    data << movementInfo.GetJumpInfo().velocity;
    mover->SendMessageToSetExcept(&data, _player);
}

void WorldSession::HandleMoveHoverAck( WorldPacket& recv_data )
{
    DEBUG_LOG("CMSG_MOVE_HOVER_ACK");

    ObjectGuid guid;                                        // guid - unused
    MovementInfo movementInfo;

    recv_data >> guid.ReadAsPacked();
    recv_data >> Unused<uint32>();                          // unk1
    recv_data >> movementInfo;
    recv_data >> Unused<uint32>();                          // unk2
}

void WorldSession::HandleMoveWaterWalkAck(WorldPacket& recv_data)
{
    DEBUG_LOG("CMSG_MOVE_WATER_WALK_ACK");

    ObjectGuid guid;                                        // guid - unused
    MovementInfo movementInfo;

    recv_data >> guid.ReadAsPacked();
    recv_data >> Unused<uint32>();                          // unk1
    recv_data >> movementInfo;
    recv_data >> Unused<uint32>();                          // unk2
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

bool WorldSession::VerifyMovementInfo(MovementInfo const& movementInfo, ObjectGuid const& guid) const
{
    // ignore wrong guid (player attempt cheating own session for not own guid possible...)
    if (guid != _player->GetMover()->GetObjectGuid())
        return false;

    if (!MaNGOS::IsValidMapCoord(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o))
        return false;

    if (movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
    {
        // transports size limited
        // (also received at zeppelin/lift leave by some reason with t_* as absolute in continent coordinates, can be safely skipped)
        if( movementInfo.GetTransportPos()->x > 50 || movementInfo.GetTransportPos()->y > 50 || movementInfo.GetTransportPos()->z > 100 )
            return false;

        if( !MaNGOS::IsValidMapCoord(movementInfo.GetPos()->x + movementInfo.GetTransportPos()->x, movementInfo.GetPos()->y + movementInfo.GetTransportPos()->y,
            movementInfo.GetPos()->z + movementInfo.GetTransportPos()->z, movementInfo.GetPos()->o + movementInfo.GetTransportPos()->o) )
        {
            return false;
        }
    }

    return true;
}

void WorldSession::HandleMoverRelocation(MovementInfo& movementInfo)
{
    movementInfo.UpdateTime(WorldTimer::getMSTime());

    Unit *mover = _player->GetMover();

    if (Player *plMover = mover->GetTypeId() == TYPEID_PLAYER ? (Player*)mover : NULL)
    {
        if (movementInfo.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
        {
            if (!plMover->m_transport)
            {
                // elevators also cause the client to send MOVEFLAG_ONTRANSPORT - just unmount if the guid can be found in the transport list
                for (MapManager::TransportSet::const_iterator iter = sMapMgr.m_Transports.begin(); iter != sMapMgr.m_Transports.end(); ++iter)
                {
                    if ((*iter)->GetObjectGuid() == movementInfo.GetTransportGuid())
                    {
                        plMover->m_transport = (*iter);
                        (*iter)->AddPassenger(plMover);
                        break;
                    }
                }
            }
        }
        else if (plMover->m_transport)               // if we were on a transport, leave
        {
            plMover->m_transport->RemovePassenger(plMover);
            plMover->m_transport = NULL;
            movementInfo.ClearTransportData();
        }

        if (movementInfo.HasMovementFlag(MOVEFLAG_SWIMMING) != plMover->IsInWater())
        {
            // now client not include swimming flag in case jumping under water
            plMover->SetInWater( !plMover->IsInWater() || plMover->GetTerrain()->IsUnderWater(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z) );
        }

        plMover->SetPosition(movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o);
        plMover->m_movementInfo = movementInfo;

        if(movementInfo.GetPos()->z < -500.0f)
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
                    plMover->EnvironmentalDamage(DAMAGE_FALL_TO_VOID, plMover->GetMaxHealth());
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
        if (mover->IsInWorld() && mover->GetTypeId() == TYPEID_UNIT)
        {
            mover->GetMap()->CreatureRelocation((Creature*)mover, movementInfo.GetPos()->x, movementInfo.GetPos()->y, movementInfo.GetPos()->z, movementInfo.GetPos()->o);
            if(((Creature*)mover)->IsVehicle())
                ((Vehicle*)mover)->RellocatePassengers(mover->GetMap());
        }
    }
}

uint32 WorldSession::ACH_CheckMoveInfo(uint32 opcode, const MovementInfo* movementInfoPtr, Player *plMover)
{
    if(!plMover) // player to check
        return true; // this does not necessarily have to be an error, just return anything here, as long as it works

    const MovementInfo& movementInfo = *movementInfoPtr;

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

    uint32 cServerTime = WorldTimer::getMSTime();
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
    if (plMover->m_anti_DeltaServerTime < 90000 && plMover->m_anti_DeltaClientTime < 90000)
        plMover->m_anti_DeltaClientTime = plMover->m_anti_DeltaServerTime;

    int32 sync_time = plMover->m_anti_DeltaClientTime - plMover->m_anti_DeltaServerTime;

    //mistiming checks
    int32 gmd = sWorld.getConfig(CONFIG_UINT32_ACH_MISTIMING_DELTA);
    if (sync_time > gmd || sync_time < -gmd)
    {
        cClientTimeDelta = cServerTimeDelta;
        plMover->m_anti_MistimingCount++;

        /*sLog.outError("MA-%s, mistiming exception. #:%d, mistiming: %dms ",
        plMover->GetName(), plMover->m_anti_MistimingCount, sync_time);*/

        plMover->m_anti_TypeFlags |= ACH_TYPE_MISTIMING;

        alarm_level += (sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_MISTIMING_FLAT) +
            (plMover->m_anti_MistimingCount * sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_MISTIMING_MULTI)));

    }
    // end mistiming checks


    uint32 curDest = plMover->m_taxi.GetTaxiDestination(); //check taxi flight
    if ((plMover->GetTransport() == NULL) && !curDest)
    {
        UnitMoveType move_type;

        // calculating section ---------------------
        //current speed
        if (movementInfo.moveFlags & MOVEFLAG_FLYING) move_type = movementInfo.moveFlags & MOVEFLAG_BACKWARD ? MOVE_FLIGHT_BACK : MOVE_FLIGHT;
        else if (movementInfo.moveFlags & MOVEFLAG_SWIMMING) move_type = movementInfo.moveFlags & MOVEFLAG_BACKWARD ? MOVE_SWIM_BACK : MOVE_SWIM;
        else if (movementInfo.moveFlags & MOVEFLAG_WALK_MODE) move_type = MOVE_WALK;
        //hmm... in first time after login player has MOVE_SWIMBACK instead MOVE_WALKBACK
        else move_type = movementInfo.moveFlags & MOVEFLAG_BACKWARD ? MOVE_SWIM_BACK : MOVE_RUN;

        float current_speed = plMover->GetSpeed(move_type);
        // end current speed

        // movement distance
        float allowed_delta= 0;

        float delta_x = plMover->GetPositionX() - movementInfo.GetPos()->x;
        float delta_y = plMover->GetPositionY() - movementInfo.GetPos()->y;
        float delta_z = plMover->GetPositionZ() - movementInfo.GetPos()->z;
        float real_delta = delta_x * delta_x + delta_y * delta_y;
        float tg_z = -99999; //tangens
        // end movement distance

        if (cClientTimeDelta < 0)
            cClientTimeDelta = 0;
        float time_delta = (cClientTimeDelta < 1500) ? (float)cClientTimeDelta/1000 : 1.5f; //normalize time - 1.5 second allowed for heavy loaded server

        if (!(movementInfo.moveFlags & (MOVEFLAG_FLYING | MOVEFLAG_SWIMMING)))
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
        float JumpHeight = plMover->m_anti_JumpBaseZ - movementInfo.GetPos()->z;
        if ((plMover->m_anti_JumpBaseZ != 0)
            && !(movementInfo.moveFlags & (MOVEFLAG_SWIMMING | MOVEFLAG_FLYING | MOVEFLAG_ASCENDING))
            && (JumpHeight < plMover->m_anti_Last_VSpeed))
        {
            sLog.outError("MA-%s, GraviJump exception. JumpHeight = %f, Allowed Veritcal Speed = %f",
                plMover->GetName(), JumpHeight, plMover->m_anti_Last_VSpeed);

            plMover->m_anti_TypeFlags |= ACH_TYPE_GRAVITY;

            //plMover->GetSession()->ACH_HandleGravity()

            alarm_level += sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_GRAVITY);
        }

        //multi jump checks
        if (opcode == MSG_MOVE_JUMP && !plMover->IsInWater())
        {
            if (plMover->m_anti_JustJumped >= 1)
            {
                alarm_level += sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_MULTIJUMP);
                sLog.outError("MA-%s, MultiJump exception. Jumps = %u", plMover->GetName(), plMover->m_anti_JustJumped);
                plMover->m_anti_TypeFlags |= ACH_TYPE_MULTIJUMP;
                //plMover->GetSession()->ACH_HandleMultiJump()
            }
            else
            {
                plMover->m_anti_JustJumped += 1;
                plMover->m_anti_JumpBaseZ = movementInfo.GetPos()->z;
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

            alarm_level += (sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_SPEED_FLAT) + 
                (sqrt(real_delta - allowed_delta) * sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_SPEED_MULTI)));
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

            alarm_level += sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_MOUNTAIN);
        }
        //Fly hack checks
        if (((movementInfo.moveFlags & (MOVEFLAG_CAN_FLY | MOVEFLAG_FLYING | MOVEFLAG_ASCENDING)) != 0)
            && !plMover->isGameMaster()
            && !(plMover->HasAuraType(SPELL_AURA_FLY) || plMover->HasAuraType(SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED)))
        {
            sLog.outError("MA-%s, flight exception.",plMover->GetName());
            plMover->m_anti_TypeFlags |= ACH_TYPE_FLY;
            //plMover->GetSession()->ACH_HandleFly()

            alarm_level += sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_FLY);
        }
        //Water-Walk checks
        if (((movementInfo.moveFlags & MOVEFLAG_WATERWALKING) != 0)
            && !plMover->isGameMaster()
            && !(plMover->HasAuraType(SPELL_AURA_WATER_WALK) || plMover->HasAuraType(SPELL_AURA_GHOST) || plMover->HasAura(3714)))
        {
            sLog.outError("MA-%s, water-walk exception. [%X]{SPELL_AURA_WATER_WALK=[%X]}",
                plMover->GetName(), movementInfo.moveFlags, plMover->HasAuraType(SPELL_AURA_WATER_WALK));
            plMover->m_anti_TypeFlags |= ACH_TYPE_WATERWALK;
            //plMover->GetSession()->ACH_HandleWaterwalk()

            alarm_level += sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_WATERWALK);
        }
        //Teleport To Plane checks
        if (movementInfo.GetPos()->z < 0.0001f && movementInfo.GetPos()->z > -0.0001f
            && ((movementInfo.moveFlags & (MOVEFLAG_SWIMMING | MOVEFLAG_CAN_FLY | MOVEFLAG_FLYING | MOVEFLAG_ASCENDING)) == 0)
            && !plMover->isGameMaster())
        {
            // Prevent using TeleportToPlan.
            Map *map = plMover->GetMap();
            if (map){
                float plane_z = map->GetTerrain()->GetHeight(movementInfo.GetPos()->x, movementInfo.GetPos()->y, MAX_HEIGHT) - movementInfo.GetPos()->z;
                plane_z = (plane_z < -500.0f) ? 0 : plane_z; //check holes in heigth map
                if(plane_z > 0.1f || plane_z < -0.1f)
                {
                    plMover->m_anti_TeleToPlane_Count++;
                    alarm_level += (sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_TELEPORT_TO_PLANE_FLAT) +
                        (sWorld.getConfig(CONFIG_UINT32_ACH_IMPACT_TELEPORT_TO_PLANE_MULTI) * plMover->m_anti_TeleToPlane_Count));

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
    return alarm_level;
}
