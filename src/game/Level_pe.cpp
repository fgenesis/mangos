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
#include "Language.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Weather.h"
#include "TargetedMovementGenerator.h"
#include "PointMovementGenerator.h"
#include "SkillDiscovery.h"
#include "SkillExtraItems.h"
#include "SystemConfig.h"
#include "PlayerDropMgr.h"
#include "VirtualPlayerMgr.h"
#include "AuctionHouseMgr.h"
#include "ChannelMgr.h"
#include "PunishMgr.h"


bool ChatHandler::UnlockMove(char* args)
{
    Player *player = getSelectedPlayer();
    if (!player)
        player = m_session->GetPlayer();
    WorldPacket data;
    data.Initialize( SMSG_FORCE_MOVE_UNROOT );
    data.appendPackGUID(player->GetGUID());
    player->GetSession()->SendPacket( &data );
    return true;
}

bool ChatHandler::LockMove(char* args)
{
    Player *player = getSelectedPlayer();
    if (!player)
        player = m_session->GetPlayer();
    WorldPacket data;
    data.Initialize( SMSG_FORCE_MOVE_ROOT );
    data.appendPackGUID(player->GetGUID());
    data << (uint32)2;
    player->GetSession()->SendPacket( &data );
    return true;
}

bool ChatHandler::HandleMyinfoCommand(char* args)
{
    uint32 guid = m_session->GetPlayer()->GetGUIDLow();
    if(m_session->GetPlayer()->IsMyinfoForbidden())
    {
        PSendSysMessage("You are not allowed to set your info.");
        CharacterDatabase.PExecute("UPDATE character_myinfo SET msg='<forbidden>' WHERE guid=%u",guid);
        return true;
    }

    std::string msg(args);

    if(!msg.length())
    {
        CharacterDatabase.PExecute("UPDATE character_myinfo SET msg='' WHERE guid=%u",guid);
        PSendSysMessage("Personal info message deleted.");
        return true;
    }

    if (msg.length()>60)
    {
        msg.resize(60);
        PSendSysMessage("Message was too long, truncated to 60 chars: '%s'",msg.c_str());
    }    

    CharacterDatabase.escape_string(msg);

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("INSERT IGNORE INTO character_myinfo (guid) VALUES (%u)",guid);
    CharacterDatabase.PExecute("UPDATE character_myinfo SET msg='%s' WHERE guid=%u", msg.c_str(), guid);
    CharacterDatabase.CommitTransaction();
    PSendSysMessage("Personal info message updated.");
    return true;
}

bool ChatHandler::HandleUnstuckCommand(char* args)
{
    Player *p=m_session->GetPlayer();
    float z=p->GetPositionZ();
    if(z < -4000 || z > 4000)
    {
        p->ResurrectPlayer(1,true);
        p->TeleportToHomebind();

        return true;
    }
    else
    {
        SendSysMessage("You are probably not stucked. If you really are, wait a few minutes and try again or contact a GM.");
    }
    return true;
}

bool ChatHandler::HandleTargetAndDeleteObjectCommand(char *args)
{
    Player* pl = m_session->GetPlayer();
    QueryResult *result;

    uint32 id = args && *args ? atoi((char*)args) : 0;
    float distance = 100*100; // should be ok as max.

    if(id)
    {
        // copied from HandleGameObjectNearCommand
        result = WorldDatabase.PQuery("SELECT guid, id, position_x, position_y, position_z, map, "
        "(POW(position_x - '%f', 2) + POW(position_y - '%f', 2) + POW(position_z - '%f', 2)) AS order_ "
        "FROM gameobject WHERE map='%u' AND (POW(position_x - '%f', 2) + POW(position_y - '%f', 2) + POW(position_z - '%f', 2)) <= '%f' ORDER BY order_",
        pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(),
        pl->GetMapId(), pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(),distance*distance);
    }
    else
    {
        // copied from HandleGameObjectNearCommand, added id = '...' to WHERE part
        result = WorldDatabase.PQuery("SELECT guid, id, position_x, position_y, position_z, map, "
        "(POW(position_x - '%f', 2) + POW(position_y - '%f', 2) + POW(position_z - '%f', 2)) AS order_ "
        "FROM gameobject WHERE map='%u' AND (POW(position_x - '%f', 2) + POW(position_y - '%f', 2) + POW(position_z - '%f', 2)) <= '%f' AND id = '%u' ORDER BY order_",
        pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(),
        pl->GetMapId(), pl->GetPositionX(), pl->GetPositionY(), pl->GetPositionZ(),distance*distance, id);
    }

    if (!result)
    {
        SendSysMessage("Nothing found!");
        return true;
    }

    Field *fields = result->Fetch();
    uint32 guid32 = fields[0].GetUInt32();
    id = fields[1].GetUInt32();
    float x = fields[2].GetFloat();
    float y = fields[3].GetFloat();
    float z = fields[4].GetFloat();
    float o = fields[5].GetFloat();
    int mapid = fields[6].GetUInt16();
    delete result;

    const GameObjectInfo *goI = sObjectMgr.GetGameObjectInfo(id);

    if (!goI)
    {
        PSendSysMessage(LANG_GAMEOBJECT_NOT_EXIST,id);
        return false;
    }

    ObjectGuid guid(HIGHGUID_GAMEOBJECT, guid32, id);

    GameObject* obj = m_session->GetPlayer()->GetMap()->GetGameObject(guid);

    if(!obj)
    {
        PSendSysMessage("Game Object (GUID: %u) not found", guid32);
        return true;
    }

    ObjectGuid owner_guid = obj->GetOwnerGuid();
    if(!owner_guid.IsEmpty())
    {
        Unit* owner = ObjectAccessor::Instance().GetUnit(*m_session->GetPlayer(),owner_guid);
        if(!owner && !owner_guid.IsPlayer())
        {
            PSendSysMessage("Game Object (GUID: %u) have references in not found creature %u GO list, can't be deleted.", owner_guid.GetCounter(), obj->GetGUIDLow());
            return true;
        }

        owner->RemoveGameObject(obj,false);
    }

    obj->Delete();
    obj->DeleteFromDB();

    PSendSysMessage("Game Object (GUID: %u) [%s] removed", obj->GetGUIDLow(),goI->name);

    return true;
}

bool ChatHandler::HandleBCCommand(char* args)
{
    WorldPacket data;

    if(!*args)
        return false;

    std::stringstream wmsg;
    wmsg << "|cffFF0000[" <<m_session->GetPlayer()->GetName() << "]:|cff80FF00 " << args;

    sWorld.SendWorldText(LANG_BROADCAST_MSG,wmsg.str().c_str());

    return true;
}

bool ChatHandler::HandleSendHomeCommand(char* args)
{
    Player *self = m_session->GetPlayer();
    Unit *target = getSelectedUnit();
    if(target && target->GetTypeId() == TYPEID_PLAYER)
    {
        Player *ptarget = (Player*)target;
        ptarget->TeleportToHomebind();
    }
    return true;
}

bool ChatHandler::HandleSuggestionCommand(char* args)
{
    /*
    if(!*args)
        return false;

    sLog.outSuggestion("SUGGESTION [%s,%u]: %s",m_session->GetPlayerName(),m_session->GetAccountId(),args);
    PSendSysMessage("Suggestion noted down. Thx!");
*/
    return true;
}

bool ChatHandler::HandleAddEmoteCommand(char* args)
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

    WorldDatabase.PExecute("DELETE FROM creature_addon WHERE guid=%u", target->GetGUIDLow());
    WorldDatabase.PExecute("UPDATE creature_addon SET emote=%u WHERE guid=%u",emote,target->GetGUIDLow());
    SendSysMessage("Creature_addon emote updated");

    return true;
}

bool ChatHandler::HandleMinlootCommand(char* args)
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

bool ChatHandler::HandleSetExtMinlootCommand(char* args)
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

bool ChatHandler::HandleSetExtSpellmultiCommand(char* args)
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

bool ChatHandler::HandleSetExtXPMultiCommand(char* args)
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

bool ChatHandler::HandleSetExtHonorCommand(char* args)
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

bool ChatHandler::HandleReloadPECommand(char *args)
{
    HandleReloadCreatureExtendedCommand("");
    HandleReloadPlayerDropTemplateCommand("");
    //sObjectMgr.LoadAnticheatAccInfo();
    sVPlayerMgr.Reload();
    sObjectMgr.LoadSpecialChannels();
    sObjectMgr.LoadAllowedGMAccounts();
    sPunishMgr.Load();

    return true;
}

bool ChatHandler::HandleReloadCreatureExtendedCommand(char *args)
{
    sObjectMgr.LoadCreaturesExtended();
    //SendGlobalSysMessage("DB table creature_extended reloaded.");
    return true;
}

bool ChatHandler::HandleReloadPlayerDropTemplateCommand(char *args)
{
    LoadPlayerDrops();
    //SendGlobalSysMessage("DB table player_drop_template reloaded.");
    return true;
}

bool ChatHandler::HandlePDropCommand(char* args)
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
                const ItemPrototype *proto = sObjectMgr.GetItemPrototype(it->item);
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

/*
bool ChatHandler::HandleAnticheatModeCommand(char* args)
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

bool ChatHandler::HandleSetXPMultiKillCommand(char *args)
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

bool ChatHandler::HandleSetXPMultiQuestCommand(char *args)
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

bool ChatHandler::HandleHelpmeCommand(char *args)
{
    // FG: this will always display the help text stored in the database
    return false;
}

bool ChatHandler::HandleGMTriggersCommand(char* args)
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

bool ChatHandler::HandleBanInfo2AccountCommand(char* args)
{
    return HandleBanInfoHelper(m_session->GetAccountId(), "");
}

bool ChatHandler::CharacterDizintegrateHelper(Player *pl, char* args)
{
    if(!pl)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    ObjectGuid guid = pl->GetObjectGuid();

    if(!stricmp(args,"all")) // remove all items except totems!!
    {
        if(sLog.IsOutCharDump())
        {
            std::string dump = PlayerDumpWriter().GetDump(guid.GetCounter());
            sLog.outCharDump(dump.c_str(),m_session->GetAccountId(),guid.GetCounter(),pl->GetName());
        }

        // in inventory
        for(int i = INVENTORY_SLOT_ITEM_START; i < INVENTORY_SLOT_ITEM_END; ++i)
            if (Item* pItem = pl->GetItemByPos( INVENTORY_SLOT_BAG_0, i ))
                if ( !pItem->IsBag() && !(pItem->GetProto()->Class == ITEM_CLASS_MISC && pItem->GetProto()->SubClass == ITEM_SUBCLASS_ARMOR_CLOTH && pItem->GetProto()->Flags & 32) )
                    pl->DestroyItem( INVENTORY_SLOT_BAG_0, i, true);

        // in inventory bags
        for(int i = INVENTORY_SLOT_BAG_START; i < INVENTORY_SLOT_BAG_END; ++i)
            if (Bag* pBag = (Bag*)pl->GetItemByPos( INVENTORY_SLOT_BAG_0, i ))
                for(uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    if (Item* pItem = pBag->GetItemByPos(j))
                        if ( !pItem->IsBag() && !(pItem->GetProto()->Class == ITEM_CLASS_MISC && pItem->GetProto()->SubClass == ITEM_SUBCLASS_ARMOR_CLOTH && pItem->GetProto()->Flags & 32) )
                            pl->DestroyItem( i, j, true);

        // in equipment and bag list
        for(int i = EQUIPMENT_SLOT_START; i < INVENTORY_SLOT_BAG_END; ++i)
            if (Item* pItem = pl->GetItemByPos( INVENTORY_SLOT_BAG_0, i ))
                if ( !pItem->IsBag() && !(pItem->GetProto()->Class == ITEM_CLASS_MISC && pItem->GetProto()->SubClass == ITEM_SUBCLASS_ARMOR_CLOTH && pItem->GetProto()->Flags & 32) )
                    pl->DestroyItem( INVENTORY_SLOT_BAG_0, i, true);

        // in bank
        for(int i = BANK_SLOT_ITEM_START; i < BANK_SLOT_ITEM_END; ++i)
            if (Item* pItem = pl->GetItemByPos( INVENTORY_SLOT_BAG_0, i ))
                if ( !pItem->IsBag() && !(pItem->GetProto()->Class == ITEM_CLASS_MISC && pItem->GetProto()->SubClass == ITEM_SUBCLASS_ARMOR_CLOTH && pItem->GetProto()->Flags & 32) )
                    pl->DestroyItem( INVENTORY_SLOT_BAG_0, i, true);

        // in bank bags
        // in inventory bags
        for(int i = BANK_SLOT_BAG_START; i < BANK_SLOT_BAG_END; ++i)
            if (Bag* pBag = (Bag*)pl->GetItemByPos( INVENTORY_SLOT_BAG_0, i ))
                for(uint32 j = 0; j < pBag->GetBagSize(); ++j)
                    if (Item* pItem = pBag->GetItemByPos(j))
                        if ( !pItem->IsBag() && !(pItem->GetProto()->Class == ITEM_CLASS_MISC && pItem->GetProto()->SubClass == ITEM_SUBCLASS_ARMOR_CLOTH && pItem->GetProto()->Flags & 32) )
                            pl->DestroyItem( i, j, true);

        std::string bannedby = m_session->GetPlayerName();
        std::string banreason = "[auto-message] DIZINTEGRATED!";

        LoginDatabase.escape_string(bannedby);
        LoginDatabase.escape_string(banreason);

        LoginDatabase.PExecute("INSERT INTO account_banned(id,bandate,unbandate,bannedby,banreason,active) VALUES (%u,UNIX_TIMESTAMP(),UNIX_TIMESTAMP()+1,'%s','%s',0)", pl->GetSession()->GetAccountId(), bannedby.c_str(), banreason.c_str() );


        SendSysMessage("All items removed");
    }
    else
    {
        return false;
    }

    return true;
}

bool ChatHandler::HandleCharacterDizintegrateSelectedCommand(char *args)
{
    return CharacterDizintegrateHelper(getSelectedPlayer(), args);
}

bool ChatHandler::HandleCharacterDizintegrateNameCommand(char *args)
{
    char* cname = strtok (args, " ");
    if (!cname || !*cname)
        return false;

    char* rest = strtok (NULL," ");
    std::string pname = cname;
    if(!normalizePlayerName(pname))
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    return CharacterDizintegrateHelper(sObjectMgr.GetPlayer(pname.c_str()), rest);
}

bool ChatHandler::HandleGMHaxCommand(char* args)
{
    if(!*args)
    {
        if(m_session->GetPlayer()->isGMHax())
            m_session->SendNotification(LANG_GM_HAX_ON);
        else
            m_session->SendNotification(LANG_GM_HAX_OFF);
        return true;
    }

    std::string argstr(args);

    if (argstr == "on")
    {
        m_session->GetPlayer()->SetGMHax(true);
        m_session->SendNotification(LANG_GM_HAX_ON);
        return true;
    }

    if (argstr == "off")
    {
        m_session->GetPlayer()->SetGMHax(false);
        m_session->SendNotification(LANG_GM_HAX_OFF);
        return true;
    }

    SendSysMessage(LANG_USE_BOL);
    SetSentErrorMessage(true);
    return false;
}

bool ChatHandler::HandleBanAutoCommand(char *args)
{
    if (!*args)
        return false;

    char* cname = strtok ((char*)args, " ");
    if (!cname)
        return false;

    std::string name = cname;

    char* creason = strtok (NULL,"");
    std::string banreason = creason ? creason : "";


    QueryResult *result;
    Field *fields;

    result = CharacterDatabase.PQuery("SELECT account FROM characters WHERE name = '%s'",name.c_str());
    if(!result)
    {
        PSendSysMessage(LANG_BAN_NOTFOUND,"character",name.c_str());
        return true;
    }
    fields = result->Fetch();
    uint32 accId = fields->GetUInt32();
    delete result;

    std::string oldreason;
    uint32 bandate, unbandate;
    int32 bandiff;
    uint32 max_banid = 0;
    uint32 counted_extra_bans = 0;
    result = LoginDatabase.PQuery("SELECT bandate,unbandate,banreason FROM account_banned WHERE id=%u", accId);
    if(result)
    {
        do
        {
            fields = result->Fetch();
            bandate = fields[0].GetUInt32();
            unbandate = fields[1].GetUInt32();
            oldreason = fields[2].GetCppString();
            bandiff = unbandate - bandate;
            if(oldreason.size() > 10 && oldreason.substr(0,10) == "[AutoBan #")
            {
                uint32 banid = atoi(oldreason.c_str() + 10);
                max_banid = std::max(max_banid, banid);
            }
            else if(bandiff >= (int32)sWorld.getConfig(CONFIG_UINT32_AUTOBAN_MIN_COUNTED_BANTIME) || bandiff == 0)
            {
                ++counted_extra_bans;
            }
        }
        while (result->NextRow());
        delete result;
    }
    max_banid = std::max(max_banid, counted_extra_bans);
    std::string duration = sWorld.GetAutoBanTime(max_banid);
    std::stringstream reason;
    reason << "[AutoBan #" << (max_banid + 1) << "; " << duration << "] " << banreason;
    uint32 duration_secs = TimeStringToSecs(duration);

    switch(sWorld.BanAccount(BAN_CHARACTER, name, duration_secs, reason.str().c_str(), m_session ? m_session->GetPlayerName() : ""))
    {
    case BAN_SUCCESS:
        if(atoi(duration.c_str())>0)
            PSendSysMessage(LANG_BAN_YOUBANNED,name.c_str(),secsToTimeString(TimeStringToSecs(duration),true).c_str(),reason.str().c_str());
        else
            PSendSysMessage(LANG_BAN_YOUPERMBANNED,name.c_str(),reason.str().c_str());
        break;
    case BAN_SYNTAX_ERROR:
        return false;
    case BAN_NOTFOUND:
        PSendSysMessage(LANG_BAN_NOTFOUND,"character",name.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}

bool ChatHandler::HandleCharacterAutodumpCommand(char *args)
{
    char* cname = strtok (args, " ");
    if (!cname || !*cname)
        return false;
    uint64 guid64 = 0;
    uint32 accid = 0;
    Player *target = sObjectMgr.GetPlayer(cname);
    std::string normalName;
    if(target)
    {
        // player online
        guid64 = target->GetGUID();
        accid = target->GetSession()->GetAccountId();
        normalName = target->GetName();
    }
    else
    {
        // player offline, ask DB
        guid64 = sObjectMgr.GetPlayerGUIDByName(cname);
        accid = sObjectMgr.GetPlayerAccountIdByGUID(guid64);
        if(guid64 && accid)
        {
            normalName = cname;
            normalizePlayerName(normalName);
        }
        else // nothing found or wrong data
        {
            PSendSysMessage(LANG_NO_PLAYER, cname);
            SetSentErrorMessage(true);
            return false;
        }
    }

    ObjectGuid guid;
    guid.Set(guid64);

    std::string dump = PlayerDumpWriter().GetDump(guid.GetCounter());
    std::string outfile;
    bool result = sLog.outCharDumpExtra(dump.c_str(),accid,guid.GetCounter(),normalName.c_str(),&outfile);
    if(!result)
    {
        PSendSysMessage(LANG_FILE_OPEN_FAIL, outfile.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage("Dump of '%s' saved to: %s", normalName.c_str(), outfile.c_str());
    return true;
}

//mute player for some times, in global channels only
bool ChatHandler::HandleChannelMuteCommand(char* args)
{
    char* nameStr = ExtractOptNotLastArg(&args);

    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&nameStr, &target, &target_guid, &target_name))
        return false;

    uint32 notspeaktime;
    if (!ExtractUInt32(&args, notspeaktime))
        return false;

    uint32 account_id = target ? target->GetSession()->GetAccountId() : sObjectMgr.GetPlayerAccountIdByGUID(target_guid);

    // find only player from same account if any
    if (!target)
    {
        if (WorldSession* session = sWorld.FindSession(account_id))
            target = session->GetPlayer();
    }

    // FG: limit mutetime if set in conf
    if(uint32 maxtime = sWorld.getConfig(CONFIG_UINT32_MAX_CHANNEL_MUTETIME))
    {
        notspeaktime = std::min(maxtime,notspeaktime);
    }

    // must have strong lesser security level
    if (HasLowerSecurity(target, target_guid, true))
        return false;

    time_t mutetime = time(NULL) + notspeaktime*60;

    if (target)
        target->GetSession()->m_channelMuteTime = mutetime;

    LoginDatabase.PExecute("UPDATE account SET chanmutetime = " UI64FMTD " WHERE id = '%u'", uint64(mutetime), account_id);

    if (target)
        ChatHandler(target).PSendSysMessage(LANG_YOUR_GLOBAL_CHANNELS_CHAT_DISABLED, notspeaktime);

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_YOU_DISABLE_CHANNEL_CHAT, nameLink.c_str(), notspeaktime);
    return true;
}

//unmute player, in global channels only
bool ChatHandler::HandleChannelUnmuteCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
        return false;

    uint32 account_id = target ? target->GetSession()->GetAccountId() : sObjectMgr.GetPlayerAccountIdByGUID(target_guid);

    // find only player from same account if any
    if (!target)
    {
        if (WorldSession* session = sWorld.FindSession(account_id))
            target = session->GetPlayer();
    }

    if (target)
    {
        if (target->CanSpeak())
        {
            SendSysMessage(LANG_CHAT_ALREADY_ENABLED);
            SetSentErrorMessage(true);
            return false;
        }

        target->GetSession()->m_channelMuteTime = 0;
    }

    LoginDatabase.PExecute("UPDATE account SET chanmutetime = '0' WHERE id = '%u'", account_id);

    if (target)
        ChatHandler(target).PSendSysMessage(LANG_YOUR_GLOBAL_CHANNELS_CHAT_ENABLED);

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_YOU_ENABLE_CHANNEL_CHAT, nameLink.c_str());
    return true;
}

bool ChatHandler::HandlePunishCommand(char *args)
{
    Player *plr = NULL;
    std::string pname;
    uint64 plr_guid = 0;
    uint32 accid = 0;
    if(!ExtractPlayerTarget(&args, &plr, NULL, &pname))
        return false;

    if(plr)
        accid = plr->GetSession()->GetAccountId();
    else
    {
        std::string pname_esc(pname);
        CharacterDatabase.escape_string(pname_esc);
        QueryResult *result = CharacterDatabase.PQuery("SELECT account FROM characters WHERE name = '%s'", pname_esc.c_str());
        if(!result)
        {
            SendSysMessage(LANG_PLAYER_NOT_FOUND);
            SetSentErrorMessage(true);
            return false;
        }
        Field *fields = result->Fetch();
        accid = fields[0].GetUInt32();
        delete result;
    }

    // extra check, not sure if really necessary, but we better be on the safe side!
    if(!accid)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // find only player from same account if any
    if (!plr)
    {
        if (WorldSession* session = sWorld.FindSession(accid))
        {
            plr = session->GetPlayer();
            pname = plr->GetName();
        }
    }

    char *what = ExtractArg(&args);
    char *reason = ExtractArg(&args);

    std::string strWhat(what ? what : "");

    bool handled = sPunishMgr.Handle(accid, plr, m_session->GetPlayer(), pname, strWhat, reason ? reason : GetMangosString(LANG_NO_REASON_GIVEN));

    if(!handled)
    {
        PSendSysMessage(LANG_NO_RECORD_IN_DB, strWhat.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    return true;
}
