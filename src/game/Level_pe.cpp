#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "PlayerDump.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Opcodes.h"
#include "GameObject.h"
#include "Chat.h"
#include "Log.h"
#include "Guild.h"
#include "ObjectAccessor.h"
#include "MapManager.h"
#include "SpellAuras.h"
#include "ScriptCalls.h"
#include "Language.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Weather.h"
#include "TargetedMovementGenerator.h"
#include "PointMovementGenerator.h"
#include "SkillDiscovery.h"
#include "SkillExtraItems.h"
#include "SystemConfig.h"
#include "Config/ConfigEnv.h"
#include "PlayerDropMgr.h"
#include "VirtualPlayerMgr.h"


bool ChatHandler::ForceEmoteSleep(const char* args)
{
    Player *player = getSelectedPlayer();
    if (!player)
        player = m_session->GetPlayer();
    player->SetFlag(UNIT_FIELD_BYTES_1,PLAYER_STATE_SLEEP);
    return true;
}

bool ChatHandler::ForceEmoteKneel(const char* args)
{
    Player *player = getSelectedPlayer();
    if (!player)
        player = m_session->GetPlayer();
    player->SetFlag(UNIT_FIELD_BYTES_1,PLAYER_STATE_KNEEL);
    return true;
}

bool ChatHandler::UnlockMove(const char* args)
{
    Player *player = getSelectedPlayer();
    if (!player)
        player = m_session->GetPlayer();
    WorldPacket data;
    data.Initialize( SMSG_FORCE_MOVE_UNROOT );
    data.append(player->GetPackGUID());
    player->GetSession()->SendPacket( &data );
    return true;
}

bool ChatHandler::LockMove(const char* args)
{
    Player *player = getSelectedPlayer();
    if (!player)
        player = m_session->GetPlayer();
    WorldPacket data;
    data.Initialize( SMSG_FORCE_MOVE_ROOT );
    data.append(player->GetPackGUID());
    data << (uint32)2;
    player->GetSession()->SendPacket( &data );
    return true;
}

bool ChatHandler::HandleMyinfoCommand(const char* args)
{
    uint32 guid = m_session->GetPlayer()->GetGUIDLow();
    QueryResult *inf=CharacterDatabase.PQuery("SELECT `guid`,`forbidden`,`msg` FROM character_myinfo WHERE guid=%u",guid);
    if(!inf)
        CharacterDatabase.PExecute("INSERT INTO character_myinfo VALUES (%u,0,'')",guid);
    if(inf && inf->Fetch()[1].GetUInt16())
    {
        PSendSysMessage("You are not allowed to set your info.");
        CharacterDatabase.PExecute("UPDATE character_myinfo SET `msg`='<forbidden>' WHERE guid=%u",guid);
        delete inf;
        return true;
    }
    delete inf;
    std::string msg(args);

    if(!msg.length())
    {
        CharacterDatabase.PExecute("UPDATE character_myinfo SET `msg`='' WHERE guid=%u",guid);
        PSendSysMessage("Personal info message deleted.");
        return true;
    }
    std::string msg2=msg;
    CharacterDatabase.escape_string(msg);

    if (msg!=msg2)
    {
        PSendSysMessage("Your message was slightly modified due to possible SQL injection");
    }

    if (msg.length()>60)
    {
        msg.resize(60);
        PSendSysMessage("Message was too long, truncated to 60 chars: '%s'",msg.c_str());
    }    

    CharacterDatabase.PExecute("UPDATE character_myinfo SET `msg`='%s' WHERE guid=%u",msg.c_str(),guid);
    PSendSysMessage("Personal info message updated.");
    return true;
}

bool ChatHandler::HandleUnstuckCommand(const char* args)
{
    Player *p=m_session->GetPlayer();
    float z=p->GetPositionZ();
    if(z < -4000 || z > 4000)
    {

        p->ResurrectPlayer(1,true);
        p->TeleportTo(p->m_homebindMapId,p->m_homebindX,p->m_homebindY,p->m_homebindZ,0.0f);

        return true;
    }
    else
    {
        SendSysMessage("You are probably not stucked. If you really are, wait a few minutes and try again or contact a GM.");
    }
    return true;
}

bool ChatHandler::HandleTargetAndDeleteObjectCommand(const char *args)
{

    QueryResult *result;

    if(*args)
    {
        int32 id = atoi((char*)args);
        if(id)
            result = WorldDatabase.PQuery("SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - %f, 2) + POW(`position_y` - %f, 2) + POW(`position_z` - %f, 2)) as `order` FROM `gameobject` WHERE `map` = %i AND `id` = '%u' ORDER BY `order` ASC LIMIT 1", m_session->GetPlayer()->GetPositionX(), m_session->GetPlayer()->GetPositionY(), m_session->GetPlayer()->GetPositionZ(), m_session->GetPlayer()->GetMapId(),id);
        else
            result = WorldDatabase.PQuery(
            "SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - %f, 2) + POW(`position_y` - %f, 2) + POW(`position_z` - %f, 2)) as `order` "
            "FROM `gameobject`,`gameobject_template` WHERE `gameobject_template`.`entry` = `gameobject`.`id` AND `map` = %i AND `name` LIKE '%%%s%%' ORDER BY `order` ASC LIMIT 1",
            m_session->GetPlayer()->GetPositionX(), m_session->GetPlayer()->GetPositionY(), m_session->GetPlayer()->GetPositionZ(), m_session->GetPlayer()->GetMapId(),args);
    }
    else
        result = WorldDatabase.PQuery("SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - %f, 2) + POW(`position_y` - %f, 2) + POW(`position_z` - %f, 2)) as `order` FROM `gameobject` WHERE `map` = %i ORDER BY `order` ASC LIMIT 1", m_session->GetPlayer()->GetPositionX(), m_session->GetPlayer()->GetPositionY(), m_session->GetPlayer()->GetPositionZ(), m_session->GetPlayer()->GetMapId());

    if (!result)
    {
        SendSysMessage("Nothing found!");
        return true;
    }

    Field *fields = result->Fetch();
    uint32 guid = fields[0].GetUInt32();
    uint32 id = fields[1].GetUInt32();
    float x = fields[2].GetFloat();
    float y = fields[3].GetFloat();
    float z = fields[4].GetFloat();
    float o = fields[5].GetFloat();
    int mapid = fields[6].GetUInt16();
    delete result;

    const GameObjectInfo *goI = objmgr.GetGameObjectInfo(id);

    if (!goI)
    {
        PSendSysMessage(LANG_GAMEOBJECT_NOT_EXIST,id);
        return false;
    }

    GameObject* obj = ObjectAccessor::Instance().GetGameObject(*m_session->GetPlayer(), MAKE_NEW_GUID(guid, id, HIGHGUID_GAMEOBJECT));

    if(!obj)
    {
        PSendSysMessage("Game Object (GUID: %u) not found", GUID_LOPART(guid));
        return true;
    }

    uint64 owner_guid = obj->GetOwnerGUID();
    if(owner_guid)
    {
        Unit* owner = ObjectAccessor::Instance().GetUnit(*m_session->GetPlayer(),owner_guid);
        if(!owner && GUID_HIPART(owner_guid)!=HIGHGUID_PLAYER)
        {
            PSendSysMessage("Game Object (GUID: %u) have references in not found creature %u GO list, can't be deleted.", GUID_LOPART(owner_guid), obj->GetGUIDLow());
            return true;
        }

        owner->RemoveGameObject(obj,false);
    }

    obj->Delete();
    obj->DeleteFromDB();

    PSendSysMessage("Game Object (GUID: %u) [%s] removed", obj->GetGUIDLow(),goI->name);

    return true;
}

bool ChatHandler::HandleBCCommand(const char* args)
{
    WorldPacket data;

    if(!*args)
        return false;

    std::stringstream wmsg;
    wmsg << "|cffFF0000[" <<m_session->GetPlayer()->GetName() << "]:|cff80FF00 " << args;

    sWorld.SendWorldText(LANG_BROADCAST_MSG,wmsg.str().c_str());

    return true;
}

bool ChatHandler::HandleSendHomeCommand(const char* args)
{
    Player *self = m_session->GetPlayer();
    Unit *target = getSelectedUnit();
    if(target && target->GetTypeId() == TYPEID_PLAYER)
    {
        Player *ptarget = (Player*)target;
        if(ptarget->m_homebindMapId != 0, ptarget->m_homebindX != 0, ptarget->m_homebindY != 0)
        {
            ptarget->TeleportTo(ptarget->m_homebindMapId,ptarget->m_homebindX,ptarget->m_homebindY,ptarget->m_homebindZ,0);
        }
    }
    return true;
}

bool ChatHandler::HandleSuggestionCommand(const char* args)
{
    /*
    if(!*args)
        return false;

    sLog.outSuggestion("SUGGESTION [%s,%u]: %s",m_session->GetPlayerName(),m_session->GetAccountId(),args);
    PSendSysMessage("Suggestion noted down. Thx!");
*/
    return true;
}

bool ChatHandler::HandleAddEmoteCommand(const char* args)
{
    // play emote
    uint32 emote = atoi((char*)args);
    Creature* target = getSelectedCreature();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }
    target->SetUInt32Value(UNIT_NPC_EMOTESTATE,emote);

    bool exists = false;
    QueryResult *result = WorldDatabase.PQuery("SELECT guid FROM creature_addon WHERE guid=%u",target->GetGUIDLow());
    if(result)
    {
        delete result;
        exists = true;
    }

    if(exists)
    {
        WorldDatabase.PExecute("UPDATE creature_addon SET emote=%u WHERE guid=%u",emote,target->GetGUIDLow());
        SendSysMessage("Creature_addon emote updated");
    }
    else
    {
        WorldDatabase.PExecute("REPLACE INTO creature_addon (guid, emote) VALUES (%u,%u)",target->GetGUIDLow(),emote);
        SendSysMessage("Creature_addon emote assigned");
    }

    return true;
}

bool ChatHandler::HandleMinlootCommand(const char* args)
{
    Creature *target = getSelectedCreature();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }
    QueryResult *result = WorldDatabase.PQuery("SELECT minloot FROM creature_extended WHERE entry=%u",target->GetEntry());
    if(!result)
    {
        std::stringstream ss;
        ss << "No minloot assigned for " << target->GetName();
        SendSysMessage(ss.str().c_str());
        return true;
    }
    uint32 minloot = result->Fetch()[0].GetUInt32();
    std::stringstream ss;
    ss << "Minloot is " << minloot << " items for " << target->GetName();
    SendSysMessage(ss.str().c_str());
    delete result;
    return true;
}

bool ChatHandler::HandleSetExtMinlootCommand(const char* args)
{
    uint32 minloot = atoi((char*)args);
    Creature* target = getSelectedCreature();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    bool exists = false;
    QueryResult *result = WorldDatabase.PQuery("SELECT entry FROM creature_extended WHERE entry=%u",target->GetEntry());
    if(result)
    {
        delete result;
        exists = true;
    }

    if(exists)
    {
        WorldDatabase.PExecute("UPDATE creature_extended SET minloot=%u WHERE entry=%u",minloot,target->GetEntry());
        SendSysMessage("Minloot updated");
    }
    else
    {
        WorldDatabase.PExecute("REPLACE INTO creature_extended (entry, minloot) VALUES (%u,%u)",target->GetEntry(),minloot);
        SendSysMessage("Minloot assigned");
    }

    return true;
}

bool ChatHandler::HandleSetExtSpellmultiCommand(const char* args)
{
    float multi = atof((char*)args);
    Creature* target = getSelectedCreature();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    bool exists = false;
    QueryResult *result = WorldDatabase.PQuery("SELECT entry FROM creature_extended WHERE entry=%u",target->GetEntry());
    if(result)
    {
        delete result;
        exists = true;
    }

    if(exists)
    {
        WorldDatabase.PExecute("UPDATE creature_extended SET SpellDmgMulti=%f WHERE entry=%u",multi,target->GetEntry());
        SendSysMessage("Spellmulti updated");
    }
    else
    {
        WorldDatabase.PExecute("REPLACE INTO creature_extended (entry, SpellDmgMulti) VALUES (%u,%f)",target->GetEntry(),multi);
        SendSysMessage("Spellmulti assigned");
    }

    return true;
}

bool ChatHandler::HandleSetExtXPMultiCommand(const char* args)
{
    float multi = atof((char*)args);
    Creature* target = getSelectedCreature();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    bool exists = false;
    QueryResult *result = WorldDatabase.PQuery("SELECT entry FROM creature_extended WHERE entry=%u",target->GetEntry());
    if(result)
    {
        delete result;
        exists = true;
    }

    if(exists)
    {
        WorldDatabase.PExecute("UPDATE creature_extended SET XPMulti=%f WHERE entry=%u",multi,target->GetEntry());
        SendSysMessage("XPMulti updated");
    }
    else
    {
        WorldDatabase.PExecute("REPLACE INTO creature_extended (entry, XPMulti) VALUES (%u,%f)",target->GetEntry(),multi);
        SendSysMessage("XPMulti assigned");
    }

    return true;
}

bool ChatHandler::HandleSetExtHonorCommand(const char* args)
{
    uint32 honor = atoi((char*)args);
    Creature* target = getSelectedCreature();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }

    bool exists = false;
    QueryResult *result = WorldDatabase.PQuery("SELECT entry FROM creature_extended WHERE entry=%u",target->GetEntry());
    if(result)
    {
        delete result;
        exists = true;
    }

    QueryResult *templ = WorldDatabase.PQuery("SELECT RacialLeader FROM creature_template WHERE entry=%u",target->GetEntry());
    if(!templ)
    {
        SendSysMessage("Some really big FUCK UP happened just now; Spawned NPC has no template?!");
        return true;
    }
    bool leader = templ->Fetch()[0].GetUInt32();
    if(!leader)
    {
        SendSysMessage("Warning: before assigned honor will work, RacialLeader must be set to 1 for this creature!");
    }
    delete templ;

    if(exists)
    {
        WorldDatabase.PExecute("UPDATE creature_extended SET honor=%u WHERE entry=%u",honor,target->GetEntry());
        SendSysMessage("Honor updated");
    }
    else
    {
        WorldDatabase.PExecute("REPLACE INTO creature_extended (entry, honor) VALUES (%u,%u)",target->GetEntry(),honor);
        SendSysMessage("Honor assigned");
    }

    return true;
}

bool ChatHandler::HandleReloadPECommand(const char *args)
{
    HandleReloadCreatureExtendedCommand("");
    HandleReloadPlayerDropTemplateCommand("");
    //objmgr.LoadAnticheatAccInfo();
    sVPlayerMgr.Reload();

    return true;
}

bool ChatHandler::HandleReloadCreatureExtendedCommand(const char *args)
{
    sCreatureExtendedStorage.Load();
    //SendGlobalSysMessage("DB table creature_extended reloaded.");
    return true;
}

bool ChatHandler::HandleReloadPlayerDropTemplateCommand(const char *args)
{
    LoadPlayerDrops();
    //SendGlobalSysMessage("DB table player_drop_template reloaded.");
    return true;
}

bool ChatHandler::HandlePDropCommand(const char* args)
{
    Player *target = getSelectedPlayer();
    uint32 mylvl = 0;
    if(!target || target->GetTypeId() != TYPEID_PLAYER)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        return true;
    }

    char *smylvl = strtok((char*)args, " ");
    if(smylvl)
    {
        mylvl = atoi(smylvl);
    }

    uint32 items = 0;

    for(PlayerDropStorage::iterator it = sPlayerDropStore.begin(); it != sPlayerDropStore.end(); it++)
    {
        if( 
            (it->guid == 0 || target->GetGUIDLow() == it->guid)
            && (target->getRaceMask() & it->racemask)
            && (target->getClassMask() & it->classmask)
            && (it->gender == 0 || it->gender > 2 || (target->getGender() == 0 && it->gender == 1) || (target->getGender() == 1 && it->gender == 2))
            && (target->getLevel() >= it->vminlvl)
            && (it->vmaxlvl == 0 || target->getLevel() <= it->vmaxlvl)
            )
        {
            if(mylvl == 0 || (
                (mylvl >= it->kminlvl)
                && (it->kmaxlvl == 0 || mylvl <= it->kmaxlvl)
                && (it->lvldiff <= int32(target->getLevel() - mylvl))
                ))
            {
                const ItemPrototype *proto = objmgr.GetItemPrototype(it->item);
                if(proto)
                {
                    items++;
                    std::stringstream ss;
                    ss << it->item << "- |cffffffff|Hitem:" << it->item << ":0:0:0|h[" << proto->Name1 << "]|h|r : " << it->chance << '%';
                    SendSysMessage(ss.str().c_str());
                }
            }
        }
    }
    std::stringstream ss;
    ss << "== Total items: " << items << " ==";
    SendSysMessage(ss.str().c_str());
    return true;
}

bool ChatHandler::HandleBindCreatureCommand(const char* args)
{
    Creature *target = getSelectedCreature();
    if(!target)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        return true;
    }
    uint32 r;
    if(*args)
        r = atoi((char*)args);
    else
        r = realmID;

    WorldDatabase.PQuery("REPLACE INTO creature_realmbind (guid, realmid) VALUES (%u, %u)",target->GetGUIDLow(),r);
    std::stringstream ss;
    ss << target->GetGUIDLow() << " [" << target->GetName() << "] bound to realmID " << r;
    if(r == realmID)
        ss << " (this realm)";
    SendSysMessage(ss.str().c_str());
    return true;
}

bool ChatHandler::HandleBindObjectCommand(const char *args)
{
    QueryResult *result;

    result = WorldDatabase.PQuery("SELECT `guid`, `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, (POW(`position_x` - %f, 2) + POW(`position_y` - %f, 2) + POW(`position_z` - %f, 2)) as `order` FROM `gameobject` WHERE `map` = %i ORDER BY `order` ASC LIMIT 1", m_session->GetPlayer()->GetPositionX(), m_session->GetPlayer()->GetPositionY(), m_session->GetPlayer()->GetPositionZ(), m_session->GetPlayer()->GetMapId());

    if (!result)
    {
        SendSysMessage("Nothing found!");
        return true;
    }

    Field *fields = result->Fetch();
    uint32 guid = fields[0].GetUInt32();
    uint32 id = fields[1].GetUInt32();
    float x = fields[2].GetFloat();
    float y = fields[3].GetFloat();
    float z = fields[4].GetFloat();
    float o = fields[5].GetFloat();
    int mapid = fields[6].GetUInt16();
    delete result;

    const GameObjectInfo *goI = objmgr.GetGameObjectInfo(id);

    if (!goI)
    {
        PSendSysMessage(LANG_GAMEOBJECT_NOT_EXIST,id);
        return false;
    }

    GameObject* obj = ObjectAccessor::Instance().GetGameObject(*m_session->GetPlayer(), MAKE_NEW_GUID(guid, id, HIGHGUID_GAMEOBJECT));

    if(!obj)
    {
        PSendSysMessage("Game Object (GUID: %u) not found", guid);
        return true;
    }


    uint32 r;
    if(*args)
        r = atoi((char*)args);
    else
        r = realmID;

    WorldDatabase.PQuery("REPLACE INTO gameobject_realmbind (guid, realmid) VALUES (%u, %u)",obj->GetGUIDLow(),r);
    std::stringstream ss;
    ss << obj->GetGUIDLow() << " [" << obj->GetName() << "] bound to realmID " << r;
    if(r == realmID)
        ss << " (this realm)";
    SendSysMessage(ss.str().c_str());
    
    return true;
}
/*
bool ChatHandler::HandleAnticheatModeCommand(const char* args)
{
    std::stringstream ss;
    if(!args || !strlen(args))
    {

        ss << "Current anticheat mode: " << sWorld.GetAnticheatMode();
        SendSysMessage(ss.str().c_str());
        return true;
    }
    uint32 mode = atoi(args);
    sWorld.SetAnticheatMode(mode);
    ss << "Anticheat mode set to: " << mode;
    SendSysMessage(ss.str().c_str());
    return true;
}

bool ChatHandler::HandleAnticheatPlayerCommand(const char *args)
{
    Player *p = getSelectedPlayer();
    int32 mode = -1;
    Tokens tok = StrSplit(args, " ");

    if(tok.size() == 1)
    {
        mode = atoi(tok[0].c_str());
    }
    else if(tok.size() > 1)
    {
        std::string pname = tok[0];
        normalizePlayerName(pname);
        p = ObjectAccessor::Instance().FindPlayerByName(tok[0].c_str());
        mode = atoi(tok[1].c_str());
    }

    if(p && mode >= 0)
    {
        p->GetSession()->anticheat_mode = mode;
        loginDatabase.PExecute("INSERT IGNORE INTO anticheat_acc_info (id) VALUES (%u)",p->GetSession()->GetAccountId());
        loginDatabase.PExecute("UPDATE anticheat_acc_info SET mode=%u WHERE id=%u",mode,p->GetSession()->GetAccountId());
        std::stringstream ss;
        ss << "Player " << p->GetName() << " acc assigned anticheat mode " << mode;
        SendSysMessage(ss.str().c_str());
    }
    else
    {
        SendSysMessage("Select player, or give correct player name (must be online), or give mode number!");
    }
    return true;
}

bool ChatHandler::HandleAnticheatOptionCommand(const char *args)
{
    if(!args || !strlen(args))
        return false;

    Tokens tok = StrSplit(args, " ");
    if(tok.size() < 2)
        return false;

    std::string opt = tok[0];
    std::string val = tok[1];

    if(opt == "kick")
        sWorld.SetAlarmKickMvAnticheat(atoi(val.c_str()));
    else if(opt == "alarmcount")
        sWorld.SetAlarmCountMvAnticheat(atoi(val.c_str()));
    else if(opt == "alarmtime")
        sWorld.SetAlarmTimeMvAnticheat(atoi(val.c_str()));
    else if(opt == "handlefall")
        sWorld.SetAnticheatHandleFall(atoi(val.c_str()));
    else if(opt == "prevent")
        sWorld.SetAnticheatPrevent(atoi(val.c_str()));
    else
        return false;

    std::stringstream ss;
    ss << "Anticheat option '" << opt << "' set to '" << val << "'";
    SendSysMessage(ss.str().c_str());
    return true;
}
*/
bool ChatHandler::HandleAHExpireCommand(const char* args)
{
    if (args == NULL)
        return false;

    char* ahMapIdStr = strtok((char*) args, " ");
    char* playerGuidStr = strtok(NULL, " ");

    if ((ahMapIdStr == NULL) || (playerGuidStr == NULL))
        return false;

    uint32 ahMapID = (uint32) strtoul(ahMapIdStr, NULL, 0);
    uint32 playerGUID = (uint32) strtoul(playerGuidStr, NULL, 0);

    AuctionHouseObject* auctionHouse = objmgr.GetAuctionsMap(ahMapID);

    if (auctionHouse == NULL)
        return false;

    AuctionHouseObject::AuctionEntryMap::iterator itr;
    itr = auctionHouse->GetAuctionsBegin();

    while (itr != auctionHouse->GetAuctionsEnd())
    {
        if (itr->second->owner == playerGUID)
            itr->second->time = sWorld.GetGameTime();

        ++itr;
    }

    return true;
}

bool ChatHandler::HandleAHDeleteCommand(const char* args)
{
    if (args == NULL)
        return false;

    char* ahMapIdStr = strtok((char*) args, " ");
    char* playerGuidStr = strtok(NULL, " ");

    if ((ahMapIdStr == NULL) || (playerGuidStr == NULL))
        return false;

    uint32 ahMapID = (uint32) strtoul(ahMapIdStr, NULL, 0);
    uint32 playerGUID = (uint32) strtoul(playerGuidStr, NULL, 0);

    AuctionHouseObject* auctionHouse = objmgr.GetAuctionsMap(ahMapID);

    if (auctionHouse == NULL)
        return false;

    AuctionHouseObject::AuctionEntryMap::iterator itr;
    itr = auctionHouse->GetAuctionsBegin();

    while (itr != auctionHouse->GetAuctionsEnd())
    {
        AuctionHouseObject::AuctionEntryMap::iterator tmp = itr;
        ++itr;

        if (tmp->second->owner != playerGUID)
            continue;

        Item* item = objmgr.GetAItem(tmp->second->item_guidlow);
        if (item != NULL)
        {
            objmgr.RemoveAItem(tmp->second->item_guidlow);
            item->DeleteFromDB();
            delete item;
        }
        else
        {
            sLog.outDebug("ahdelete: "
                "clearing auction for non-existant item_guidlow (%d)",
                tmp->second->item_guidlow);
        }

        CharacterDatabase.PExecute("DELETE FROM `auctionhouse` WHERE `id` = '%u'",
            tmp->second->Id);
        auctionHouse->RemoveAuction(tmp->second->Id);
        delete tmp->second;
    }

    return true;
}

bool ChatHandler::HandleSetXPMultiKillCommand(const char *args)
{
    if(!args || !strlen(args))
        return false;
    float m = atof(args);
    m_session->SetXPMultiKill(m);
    char buf[50];
    sprintf(buf, "Your kill XP multiplier has been set to: %.2f", m_session->GetXPMultiKill());
    SendSysMessage(buf);
    return true;
}

bool ChatHandler::HandleSetXPMultiQuestCommand(const char *args)
{
    if(!args || !strlen(args))
        return false;
    float m = atof(args);
    m_session->SetXPMultiQuest(m);
    char buf[50];
    sprintf(buf, "Your quest XP multiplier has been set to: %.2f", m_session->GetXPMultiQuest());
    SendSysMessage(buf);
    return true;
}

bool ChatHandler::HandleHelpmeCommand(const char *args)
{
    // FG: this will always display the help text stored in the database
    return false;
}

bool ChatHandler::HandleGMTriggersCommand(const char* args)
{
    if(!*args)
    {
        if(m_session->GetPlayer()->isGMTriggers())
            m_session->SendNotification(LANG_GM_TRIGGERS_ON);
        else
            m_session->SendNotification(LANG_GM_TRIGGERS_OFF);
        return true;
    }

    std::string argstr = (char*)args;

    if (argstr == "on")
    {
        m_session->GetPlayer()->SetGMTriggers(true);
        m_session->SendNotification(LANG_GM_TRIGGERS_ON);
        return true;
    }

    if (argstr == "off")
    {
        m_session->GetPlayer()->SetGMTriggers(false);
        m_session->SendNotification(LANG_GM_TRIGGERS_OFF);
        return true;
    }

    SendSysMessage(LANG_USE_BOL);
    SetSentErrorMessage(true);
    return false;
}



