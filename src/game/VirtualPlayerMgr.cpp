#include "VirtualPlayerMgr.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include "Formulas.h"
#include "World.h"
#include "Policies/SingletonImp.h"

INSTANTIATE_SINGLETON_1( VirtualPlayerMgr );

/* raus:
(channel list)
.info
www status

*/

VirtualPlayerMgr::VirtualPlayerMgr()
{
    _enabled = false;
    _last_update_time = time(NULL);
    _cur_hour = -1;
    _logincheck_timediff = 0;
    _history_maxonlinecount = 0;
}

VirtualPlayerMgr::~VirtualPlayerMgr()
{
}

bool VirtualPlayerMgr::NameExists(std::string n)
{
    if(!_enabled)
        return false;
    ACE_Guard<ACE_Thread_Mutex> g(_mutex);
    bool result = false;
    for(VPCharMap::iterator it = _chars.begin(); it != _chars.end(); it++)
    {
        if(!stricmp(it->second.name.c_str(), n.c_str()))
        {
            result = true;
            break;
        }
    }
    return result;
}

VirtualPlayer *VirtualPlayerMgr::GetChar(std::string n)
{
    if(!_enabled)
        return NULL;
    ACE_Guard<ACE_Thread_Mutex> g(_mutex);
    for(VPCharMap::iterator it = _chars.begin(); it != _chars.end(); it++)
    {
        if(!stricmp(it->second.name.c_str(), n.c_str()))
        {
            return &(it->second);
        }
    }
    return NULL;
}

bool VirtualPlayerMgr::IsOnline(std::string n)
{
    if(!_enabled)
        return false;
    ACE_Guard<ACE_Thread_Mutex> g(_mutex);
    bool result = false;
    for(VPCharMap::iterator it = _chars.begin(); it != _chars.end(); it++)
    {
        if(!stricmp(it->second.name.c_str(), n.c_str()))
        {
            return it->second.online;
        }
    }
    return false;
}

std::set<uint32> VirtualPlayerMgr::GetOnlineSet(void)
{
    ACE_Guard<ACE_Thread_Mutex> g(_mutex);
    std::set<uint32> cp = _onlineIDs; // copy set before return
    return cp;
}

VirtualPlayer *VirtualPlayerMgr::GetChar(uint32 id)
{
    if(!_enabled)
        return NULL;
    ACE_Guard<ACE_Thread_Mutex> g(_mutex);
    VPCharMap::iterator it = _chars.find(id);
    if(it != _chars.end())
        return &(it->second);
    return NULL;
}


void VirtualPlayerMgr::Load(void)
{
    if(!_enabled)
        return;
    uint32 zd,t,c,q,n,zp,cu;
    zp = _LoadZonePositions();
    zd = _LoadZoneData(); // must be loaded after positions
    t = _LoadTraits();
    c = _LoadChars();
    cu = _LoadCharsUpdate();
    q = _LoadQuests();
    n = _LoadNameParts();
    _LoadTimes();

    sLog.outString("VPM: Loaded %u zones, %u zonepos, %u traits, %u chars (%u updated), %u nameparts",zd,zp,t,c,cu,n);
    sLog.outString("VPM: %u quests indexed",q);

}

void VirtualPlayerMgr::_Reload(void)
{
    if(!_enabled)
        return;
    _LoadZonePositions();
    _LoadZoneData();
    _LoadTraits();
    _LoadQuests();
    _LoadNameParts();
    _LoadTimes();
    _LoadCharsUpdate();
}

uint32 VirtualPlayerMgr::_LoadCharsUpdate(void)
{
    uint32 count = 0;
    QueryResult *result = CharacterDatabase.PQuery("SELECT id,info,guild FROM vp_chars_update");
    uint32 id;
    std::string info, guild;
    VPCharMap::iterator it;
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            id = fields[0].GetUInt32();
            info = fields[1].GetCppString();
            guild = fields[2].GetCppString();
            it = _chars.find(id);
            if(it != _chars.end())
            {
                count++;
                if(!info.size() && !guild.size())
                {
                    it->second.info = "";
                    it->second.guild = "";
                }
                else
                {
                    if(info.size())
                        it->second.info = info;
                    if(guild.size())
                        it->second.guild = guild;
                }
                // data changed; save immediately
                if(it->second.online)
                    it->second.next_savetime = 0;
            }

        } while (result->NextRow());

        delete result;
    }
    CharacterDatabase.Execute("DELETE FROM vp_chars_update");
    return count;
}


uint32 VirtualPlayerMgr::_LoadTimes(void)
{
    _hourpop.clear();
    uint32 count = 0;
    QueryResult *result = CharacterDatabase.PQuery("SELECT hour,min,max FROM vp_times");
    VirtualPlayerHourPopulation he;
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            he.hour = fields[0].GetUInt32();
            he.min = fields[1].GetUInt32();
            he.max = fields[2].GetUInt32();
            _hourpop[he.hour] = he;
            count++;
        } while (result->NextRow());

        delete result;
    }
    return count;
}

uint32 VirtualPlayerMgr::_LoadNameParts(void)
{
    _nameparts.clear();
    uint32 count = 0;
    QueryResult *result = WorldDatabase.PQuery("SELECT str,start,middle,end FROM vp_nameparts");
    VirtualPlayerNameEntry ne;
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            ne.str = fields[0].GetCppString();
            ne.start = fields[1].GetBool();
            ne.middle = fields[2].GetBool();
            ne.end = fields[3].GetBool();
            _nameparts.push_back(ne);
            count++;
        } while (result->NextRow());

        delete result;
    }
    return count;
}

uint32 VirtualPlayerMgr::_LoadZoneData(void)
{
    _zones.clear();
    uint32 count = 0;
    QueryResult *result = WorldDatabase.PQuery("SELECT zone,lvlmin,lvlmax FROM vp_zonedata");
    VirtualPlayerZoneDataEntry ze;
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            ze.zone = fields[0].GetUInt32();
            ze.lvlmin = fields[1].GetUInt32();
            ze.lvlmax = fields[2].GetUInt32();
            _zones[ze.zone] = ze;
            if(_zonepos.find(ze.zone) == _zonepos.end())
            {
                sLog.outError("VPM: Zone %u has no possible positions!",ze.zone);
            }
            count++;
        } while (result->NextRow());

        delete result;
    }
    return count;
}

uint32 VirtualPlayerMgr::_LoadTraits(void)
{
    _traits.clear();
    uint32 count = 0; //                                0     1       2             3             4
    QueryResult *result = WorldDatabase.PQuery("SELECT id, xp_kill, xp_quest, staytime_min, staytime_max, "
    //        5                6               7                   8                  9                 10
        "questtime_min, questtime_max, killtime_burst_min, killtime_burst_max, killtime_idle_min, killtime_idle_max, "
    //        11                 12             13              14              15
        "kills_burst_min, kills_burst_max, time_idle_min, time_idle_max, offline_time_min FROM vp_traits");
    VirtualPlayerTraits vt;
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            vt.id                  = fields[0].GetUInt32();
            vt.xp_kill             = fields[1].GetFloat();
            vt.xp_quest            = fields[2].GetFloat();
            vt.staytime_min        = fields[3].GetUInt32();
            vt.staytime_max        = fields[4].GetUInt32();
            vt.questtime_min       = fields[5].GetUInt32();
            vt.questtime_max       = fields[6].GetUInt32();
            vt.killtime_burst_min  = fields[7].GetUInt32();
            vt.killtime_burst_max  = fields[8].GetUInt32();
            vt.killtime_idle_min   = fields[9].GetUInt32();
            vt.killtime_idle_max   = fields[10].GetUInt32();
            vt.kills_burst_min     = fields[11].GetUInt32();
            vt.kills_burst_max     = fields[12].GetUInt32();
            vt.time_idle_min       = fields[13].GetUInt32();
            vt.time_idle_max       = fields[14].GetUInt32();
            vt.offline_time_min    = fields[15].GetUInt32();
            _traits[vt.id] = vt;
            count++;
        } while (result->NextRow());

        delete result;
    }
    return count;
}

uint32 VirtualPlayerMgr::_LoadChars(void)
{
    // NOT clearing chars; its dangerous operation!!
    QueryResult *result;
    result = CharacterDatabase.PQuery("SELECT MAX(id) FROM vp_chars");
    if(result)
    {
        Field *fields = result->Fetch();
        _maxCharID = fields[0].GetUInt32();
    }
    else
    {
        _maxCharID = 0;
        return 0;
    }
    DEBUG_LOG("VPM: MaxCharID=%u",_maxCharID);
    uint32 count = 0; //                       0      1        2     3    4      5    6    7      8      9          10              11      12     13     14     15
    result = CharacterDatabase.PQuery("SELECT id, traits_id, name, race, class, lvl, xp, honor, quests, info, planned_logout, last_logout, zone, gender, guild, guid FROM vp_chars");
    VirtualPlayer vp;
    if(result)
    {
        do
        {
            // DB fields
            Field *fields = result->Fetch();
            vp.id                  = fields[0].GetUInt32();
            vp.traits_id           = fields[1].GetFloat();
            vp.name                = fields[2].GetCppString();
            vp.race                = fields[3].GetUInt32();
            vp.class_              = fields[4].GetUInt32();
            vp.lvl                 = fields[5].GetUInt32();
            vp.xp                  = fields[6].GetUInt32();
            vp.honor               = fields[7].GetUInt32();
            vp.quests              = fields[8].GetUInt32();
            vp.info                = fields[9].GetCppString();
            vp.planned_logout      = fields[10].GetUInt32();
            vp.last_logout         = fields[11].GetUInt32();
            vp.zone                = fields[12].GetUInt32();
            vp.gender              = fields[13].GetBool();
            vp.guild               = fields[14].GetString();
            vp.guid                = MAKE_NEW_GUID(fields[15].GetUInt32(), 0, HIGHGUID_PLAYER);

            // find correct traits
            VPTraitsMap::iterator it = _traits.find(vp.traits_id);
            if(it != _traits.end())
            {
                vp.traits = it->second;
            }
            else
            {
                vp.traits_id = 1;
                it = _traits.find(vp.traits_id);
                if(it != _traits.end())
                {
                    vp.traits = it->second;
                    sLog.outError("Invalid traits %u for VChar %s (%u), fixed.",vp.traits,vp.name.c_str(),vp.id);
                }
                else
                {
                    sLog.outError("Traits %u not found; can't load %s (%u)",vp.name.c_str(),vp.id);
                    continue;
                }
            }

            _chars[vp.id] = vp;
            count++;
        } while (result->NextRow());

        delete result;
    }
    return count;
}

uint32 VirtualPlayerMgr::_LoadZonePositions(void)
{
    _zonepos.clear();
    uint32 count = 0;
    QueryResult *result = WorldDatabase.PQuery("SELECT zone,x,y,z,map FROM vp_zonepositions");
    VirtualPlayerZonePosition zp;
    if(result)
    {
        do
        {
            Field *fields = result->Fetch();
            zp.zone = fields[0].GetUInt32();
            zp.x = fields[1].GetFloat();
            zp.y = fields[2].GetFloat();
            zp.z = fields[3].GetFloat();
            zp.map = fields[4].GetUInt32();
            _zonepos[zp.zone].push_back(zp);
            count++;
        } while (result->NextRow());

        delete result;
    }
    return count;
}


uint32 VirtualPlayerMgr::_LoadQuests(void)
{
    _quests.clear();
    uint32 count = 0;
    ObjectMgr::QuestMap const& qmap = sObjectMgr.GetQuestTemplates();
    uint32 qlvl;
    for(ObjectMgr::QuestMap::const_iterator qi = qmap.begin(); qi != qmap.end(); qi++)
    {
        Quest& q = *(qi->second);
        qlvl = q.GetQuestLevel();
        if(!qlvl)
        {
            qlvl = q.GetMinLevel();
        }
        _quests[qlvl].push_back(q.GetQuestId());
        count++;
    }

    for(VPQuestMap::iterator it = _quests.begin(); it != _quests.end(); it++)
    {
        DEBUG_LOG("VPM: Level %u: %u Quests",it->first,it->second.size());
    }

    return count;
}

std::string VirtualPlayerMgr::_GenerateName(uint32 s) // syllables
{
    std::string name;
    std::deque<std::string> avail_start, avail_middle, avail_end;
    // TODO: more efficient if loaded once and definitely on db load...!
    for(VPNameList::iterator it = _nameparts.begin(); it != _nameparts.end(); it++)
    {
        if(it->start)
            avail_start.push_back(it->str);
        if(it->middle)
            avail_middle.push_back(it->str);
        if(it->end)
            avail_end.push_back(it->str);
    }
    while(!name.size())
    {
        for(uint32 i = 0; i < s; i++)
        {
            if(!i) // first syllable
                name += avail_start[ urand(0, avail_start.size() - 1) ];
            else if(i == s - 1) // last syllable
                name += avail_end[ urand(0, avail_end.size() - 1) ];
            else // middle syllable
                name += avail_middle[ urand(0, avail_middle.size() - 1) ];
        }
        if(name.size() > 12)
        {
            DEBUG_LOG("VPM: NameGen: '%s' is too long (%u)!",name.c_str(),name.length());
            name = "";
        }
        if(!normalizePlayerName(name))
        {
            DEBUG_LOG("VPM: NameGen: '%s' cant be normalized!",name.c_str());
            name = "";
        }
        if(sObjectMgr.GetPlayerGUIDByName(name))
        {
            DEBUG_LOG("VPM: NameGen: '%s' already exists as real character",name.c_str());
            name = "";
        }
        bool name_exist = false;
        for(VPCharMap::iterator cit = _chars.begin(); cit != _chars.end(); cit++)
        {
            if(cit->second.name == name)
            {
                name_exist = true;
                break;
            }
        }
        if(name_exist)
        {
            DEBUG_LOG("VPM: NameGen: '%s' already exists as virtual char",name.c_str());
            name = "";
        }
    }

    return name;
}

void VirtualPlayerMgr::_Save(VirtualPlayer *vp)
{
    if(!vp->online)
    {
        sLog.outError("VPM: _Save called for offline vchar %s (%u)",vp->name.c_str(),vp->id);
        return;
    }
    time_t curtime = time(NULL);
    
    if(vp->planned_logout < curtime)
    {
        vp->last_logout = curtime;
        vp->planned_logout = 0;
        _SetOffline(vp);
    }

    _SaveToDB(vp);
    DEBUG_LOG("VPM: %s (%u) saved, is now *%s*",vp->name.c_str(),vp->id,(vp->online ? "online" : "offline") );
}

void VirtualPlayerMgr::_SetOffline(VirtualPlayer *vp)
{
    vp->online = false;
    _onlineIDs.erase(vp->id);
    CharacterDatabase.PExecute("DELETE FROM vp_online WHERE id=%u",vp->id);
    CharacterDatabase.PExecute("UPDATE characters SET online=0 WHERE guid=%u",GUID_LOPART(vp->guid));
}

void VirtualPlayerMgr::_SetOnline(VirtualPlayer *vp)
{
    CharacterDatabase.PExecute("UPDATE characters SET online=1 WHERE guid=%u",GUID_LOPART(vp->guid));
    CharacterDatabase.PExecute("INSERT IGNORE INTO vp_online VALUES (%u)",vp->id);
    _onlineIDs.insert(vp->id);
    vp->online = true;
}

void VirtualPlayerMgr::_SaveToDB(VirtualPlayer *vp)
{
    // update req. db values
    VPZonePosMap::iterator zpmi = _zonepos.find(vp->zone);
    if(zpmi != _zonepos.end())
    {
        VirtualPlayerZonePosition zp = zpmi->second[ urand(0, zpmi->second.size() - 1) ];
        vp->px = zp.x;
        vp->py = zp.y;
        vp->pz = zp.z;
        vp->map = zp.map;
    }
    uint32 loguid = GUID_LOPART(vp->guid);

    std::stringstream ss;
    ss << "REPLACE INTO vp_chars ";
    //      0        1       2     3     4     5    6    7      8       9         10             11       12     13    14 15 16 17     18    19
    ss << "(id, traits_id, name, race, class, lvl, xp, honor, quests, info, planned_logout, last_logout, zone, gender, x, y, z, map, guild, guid) VALUES (";
    ss << vp->id << ", ";
    ss << vp->traits_id << ", ";
    ss << "'" << vp->name << "', ";
    ss << uint32(vp->race) << ", ";
    ss << uint32(vp->class_) << ", ";
    ss << uint32(vp->lvl) << ", ";
    ss << vp->xp << ", ";
    ss << vp->honor << ", ";
    ss << vp->quests << ", ";
    ss << "'" << vp->info << "', ";
    ss << vp->planned_logout << ", ";
    ss << vp->last_logout << ", ";
    ss << vp->zone << ", ";
    ss << uint32(vp->gender) << ", ";
    ss << vp->px << ", ";
    ss << vp->py << ", ";
    ss << vp->pz << ", ";
    ss << vp->map << ", ";
    ss << "'" << vp->guild << "', ";
    ss << loguid << ")";
    CharacterDatabase.Execute(ss.str().c_str());

    vp->_Export();
}

void VirtualPlayer::_Export(void)
{
    uint32 data[PLAYER_END];
    for(uint32 i = 0; i < PLAYER_END; i++)
        data[i] = 0;
    data[OBJECT_FIELD_GUID] = *((uint32*)&guid);
    data[OBJECT_FIELD_GUID+1] = *((uint32*)&guid + 1);
    data[UNIT_FIELD_LEVEL] = lvl;
    data[PLAYER_XP] = xp;
    data[PLAYER_NEXT_LEVEL_XP] = lvlup_xp;
    data[PLAYER_FIELD_HONOR_CURRENCY] = honor;
    data[UNIT_FIELD_BYTES_0] = ( race ) | ( class_ << 8 ) | ( gender << 16 );
    data[PLAYER_BYTES_3] = gender; // TODO: not 100% correct, should be byte value
    std::ostringstream ss, ss_del, ss2;
    uint32 loguid = GUID_LOPART(guid);
    ss_del << "DELETE FROM characters WHERE account=1 AND guid=" << loguid;
    
    // from Player.cpp::SaveToDB()
    ss << "INSERT INTO characters (guid,account,name,race,class,"
        "map, position_x, position_y, position_z, data, "
        "online, zone) VALUES (";
    ss << loguid << ", ";
    ss << "1, "; // acc is 1 to track exported virtual characters
    ss << "'" << name << "', ";
    ss << uint32(race) << ", ";
    ss << uint32(class_) << ", ";
    ss << map << ", ";
    ss << px << ", ";
    ss << py << ", ";
    ss << pz << ", ";
    ss << "'";
    for(uint32 i = 0; i < PLAYER_END; i++)
    {
        ss << data[i] << " ";
    }
    ss << "', ";
    ss << uint32(online) << ", ";
    ss << zone << ")";

    // myinfo
    ss2 << "REPLACE INTO character_myinfo (guid,msg) VALUES (";
    ss2 << guid << ", ";
    ss2 << "'" << info << "'";
    ss2 << ")";

    std::string out1 = ss_del.str();
    std::string out2 = ss.str();
    std::string out3 = ss2.str();

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.Execute(out1.c_str());
    CharacterDatabase.Execute(out2.c_str());
    CharacterDatabase.Execute(out3.c_str());
    CharacterDatabase.CommitTransaction();
}


void VirtualPlayer::Init(void)
{
    uint32 curtime = time(NULL);
    lvlup_xp = sObjectMgr.GetXPForLevel(lvl);
    next_savetime = urand(0, sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE) / 1000);
    next_questtime = urand(traits.questtime_min, traits.questtime_max);
    if(lvl < 23)
        next_questtime *= 0.6;
    next_killtime = urand(traits.killtime_idle_min, traits.killtime_idle_max);
    if(!zone)
        next_zonechange = 0;
    else
        next_zonechange = urand(360, 1000); // hack, we cant access _GetZoneTime() properly (no access to _zones)
    killburst = roll_chance_i(40);
    left_burst_kills = 0; // if its burst we need to re-init this value, else it would be != 0 and stay undefined
    _UpdateKillTimers();

    if(!planned_logout || planned_logout <= curtime)
    {
        planned_logout = curtime + urand(traits.staytime_min, traits.staytime_max);
    }
    DEBUG_LOG("VP: %s (%u) initialized, playtime=%u, burst=%u, nkill=%u, nq=%u, nsave=%u",
        name.c_str(),id,planned_logout-curtime,killburst,next_killtime,next_questtime,next_savetime);
}

void VirtualPlayer::HandleKill(uint32 mlvl)
{
    DEBUG_LOG("VP: %s: HandleKill for mlvl=%u (self=%u) [%s] left_burst_kills=%u",name.c_str(),mlvl,lvl, killburst ? "BURST" : "idle", left_burst_kills);
    // switch to idle mode if necessary
    if(killburst)
    {
        left_burst_kills--;
        if(!left_burst_kills)
        {
            killburst = false;
            DEBUG_LOG("VP: %s switched to idle mode",name.c_str());
        }
    }

    // calc XP
    uint32 newxp = MaNGOS::XP::BaseGain(lvl,mlvl, lvl < 60 ? CONTENT_1_60 : CONTENT_61_70);
    if(roll_chance_i(8))
    {
        newxp *= 2; // 8% chance for elite kill
    }
    newxp *= sWorld.getConfig(CONFIG_FLOAT_RATE_XP_KILL);
    newxp *= traits.xp_kill;
    if(newxp)
        AddXP(newxp);

    _UpdateKillTimers();
}

void VirtualPlayer::FinishQuest(uint32 questId)
{
    const Quest *q = sObjectMgr.GetQuestTemplate(questId);
    if(!q)
        return;

    if(lvl >= sVPlayerMgr.GetMaxLevel())
        return;

    quests++;

    float fullxp = 0;
    uint32 endxp = 0;
    uint32 RewMoneyMaxLevel = q->GetRewMoneyMaxLevel();
    if(RewMoneyMaxLevel)
    {
        uint32 pLevel = lvl;
        uint32 qLevel = q->GetQuestLevel();
        float fullxp = 0;
        if (qLevel >= 65)
            fullxp = RewMoneyMaxLevel / 6.0f;
        else if (qLevel == 64)
            fullxp = RewMoneyMaxLevel / 4.8f;
        else if (qLevel == 63)
            fullxp = RewMoneyMaxLevel / 3.6f;
        else if (qLevel == 62)
            fullxp = RewMoneyMaxLevel / 2.4f;
        else if (qLevel == 61)
            fullxp = RewMoneyMaxLevel / 1.2f;
        else if (qLevel > 0 && qLevel <= 60)
            fullxp = RewMoneyMaxLevel / 0.6f;

        if( pLevel <= qLevel +  5 )
            endxp = (uint32)fullxp;
        else if( pLevel == qLevel +  6 )
             endxp = (uint32)(fullxp * 0.8f);
        else if( pLevel == qLevel +  7 )
             endxp = (uint32)(fullxp * 0.6f);
        else if( pLevel == qLevel +  8 )
             endxp = (uint32)(fullxp * 0.4f);
        else if( pLevel == qLevel +  9 )
             endxp = (uint32)(fullxp * 0.2f);
        else
             endxp = (uint32)(fullxp * 0.1f);
    }
    DEBUG_LOG("VP: %s: quest %u returned %u XP",name.c_str(),questId, endxp);
    endxp *= sWorld.getConfig(CONFIG_FLOAT_RATE_XP_QUEST);
    endxp *= traits.xp_quest;
    AddXP(endxp);
}

void VirtualPlayer::AddXP(uint32 addxp)
{
    if(lvl >= sVPlayerMgr.GetMaxLevel())
    {
        xp = 0;
        return;
    }
    DEBUG_LOG("VP: %s: Adding %u XP (%.3f perc. of levelup xp done)", name.c_str(), addxp, (float(xp) / float(lvlup_xp)) * 100.0f );
    uint32 newxp = xp + addxp;
    while(newxp >= lvlup_xp && lvl < sVPlayerMgr.GetMaxLevel())
    {
        lvl += 1;
        if(lvl > sVPlayerMgr.GetMaxLevel())
            lvl = sVPlayerMgr.GetMaxLevel();
        newxp -= lvlup_xp;
        lvlup_xp = sObjectMgr.GetXPForLevel(lvl);
        DEBUG_LOG("VP: %s: Leveled to %u",name.c_str(),lvl);
    }
    xp = newxp;
}

VirtualPlayer *VirtualPlayerMgr::_GenerateNewChar(void)
{
    _maxCharID++;
    VirtualPlayer vp;
    vp.id = _maxCharID;
    vp.name = _GenerateName( urand(2,4) );
    vp.guid = sObjectMgr.GenerateLowGuid(HIGHGUID_PLAYER);
    vp.xp = 0;
    vp.zone = 0;
    vp.quests = 0;
    vp.honor = 0;
    vp.last_logout = 0;
    if(roll_chance_i(30))
    {
        vp.lvl = 1;
    }
    else
    {
        if(roll_chance_i(60))
        {
            vp.lvl = urand(1,10);
        }
        else
        {
            vp.lvl = urand(1,15);
        }
    }
    if(vp.lvl == 1)
    {
        vp.quests = 0;
        vp.honor = 0;
    }
    else if(vp.lvl < 4)
    {
        vp.quests = urand(1,8);
    }
    else if(vp.lvl < 12)
    {
        vp.quests = urand(4,25);
    }
    else if(vp.lvl < 30)
    {
        vp.quests = urand(25,60);
        vp.honor = urand(0, 250);
    }
    else
    {
        vp.quests = urand(60,250);
        vp.honor = urand(0, 1100);
    }
    std::deque<uint32> avail_traits;
    for(VPTraitsMap::iterator it = _traits.begin(); it != _traits.end(); it++)
    {
        avail_traits.push_back(it->first);
    }
    vp.traits_id = avail_traits[ urand(0, avail_traits.size() - 1) ];
    vp.traits = _traits[vp.traits_id];
    vp.gender = roll_chance_i(57); // sux but in avg more male chars are created i think :/
    do 
    {
        do 
        {
        vp.race = urand(1,11);
        } while(vp.race == 9); // invalid races!
        do 
        {
            vp.class_ = urand(1,11);
        } while(vp.class_ == 6 || vp.class_ == 10); // invalid classes!
    } while( !sObjectMgr.GetPlayerInfo(vp.race, vp.class_) );
    vp.Init();
    _chars[vp.id] = vp;
    DEBUG_LOG("VPM: Generated new char %s (%u), level %u, traits %u, class %u, race %u", vp.name.c_str(), vp.id, vp.lvl, vp.traits_id, vp.class_, vp.race);
    VirtualPlayer *vpr = &_chars[vp.id];
    _SaveToDB(vpr);
    return vpr; // return storage pointer!!!
}


void VirtualPlayer::_UpdateKillTimers(void)
{
    if(killburst)
    {
        // we are in a burst
        if(!left_burst_kills)
        {
            left_burst_kills = urand(traits.kills_burst_min, traits.kills_burst_max);
        }
        next_bursttime = 0;
        next_killtime = urand(traits.killtime_burst_min, traits.killtime_burst_max);
    }
    else
    {
        // we are idle
        if(!next_bursttime)
        {
            next_bursttime = urand(traits.killtime_idle_min, traits.killtime_idle_max);
        }
        left_burst_kills = 0;
        next_killtime = urand(traits.killtime_idle_min, traits.killtime_idle_max);
    }
}

void VirtualPlayer::_CalcZoneTime(VirtualPlayerZoneDataEntry ze)
{
    if(lvl > ze.lvlmax || lvl < ze.lvlmin) // change zone fast if too low lvl
        next_zonechange = urand(90,360);
    else
        next_zonechange = urand(300, 2000);
}

void VirtualPlayerMgr::Update(void)
{
    if(!_enabled)
        return;

    if(_must_reload)
    {
        _must_reload = false;
        _Reload();
        _spread_update_difftime = 0;
    }

    time_t curtime = time(NULL);

    tm* atm = localtime(&curtime);
    if(atm->tm_hour != _cur_hour)
    {
        _cur_hour = atm->tm_hour;
        int32 real_hour = _cur_hour + _hour_offset;
        if(real_hour < 0)
            real_hour += 24;
        if(real_hour > 24)
            real_hour -= 24;
        VirtualPlayerHourPopulation pop = _hourpop[real_hour];
        _min_online = pop.min;
        _max_online = pop.max;
        sLog.outString("VPM: Hour changed, now %u h, min=%u max=%u",real_hour,_min_online,_max_online);
        _spread_update_difftime = 0; // must update spread now
    }

    uint32 difftime = curtime - _last_update_time;
    _last_update_time = curtime;
    if(!difftime)
        return;

    if(_onlineIDs.size() > _history_maxonlinecount)
        _history_maxonlinecount = _onlineIDs.size();

    // add some variance to min/max online count values
    if(_spread_update_difftime < difftime)
    {
        _spread_update_difftime = _spread_update_time;
        _act_min_online = _min_online + ((((float(rand() % 2000) / 1000.0f) - 1.0f) * _min_max_spread) * _min_online);
        _act_max_online = _max_online + ((((float(rand() % 2000) / 1000.0f) - 1.0f) * _min_max_spread) * _max_online);
        DEBUG_LOG("VPM: Min/Max spread updated. Min: %u -> %u, Max %u -> %u",_min_online,_act_min_online,_max_online,_act_max_online);
    }
    else _spread_update_difftime -= difftime;


    uint32 real_online = sWorld.GetActiveSessionCount();
    uint32 bots_online = GetOnlineCount();
    uint32 all_online = real_online + bots_online;
    float login_chance = 0;

    if(_logincheck_timediff < difftime)
    {
        _logincheck_timediff = _logincheck_interval;
        float online_desired_perc = ((float)all_online / _act_max_online); // % of how near we are at min ppl amount. > 1 is good!

        if(all_online < _act_max_online)
            login_chance = pow(1.0f - online_desired_perc, 0.65f) * (0.8f / online_desired_perc) * 30.0f;

        if(all_online <  _act_min_online)
        {
            login_chance += 14.0f;
        }

        if(roll_chance_f(login_chance))
        {
            sLog.outDetail("VPM: Logging in! (%u/%u (min) online, perc=%.4f, chance=%.3f)", all_online,_act_min_online,online_desired_perc,login_chance);
            bool cant_login = false;
            bool force_create = roll_chance_f(1.9f);
            bool high_level_login = roll_chance_i(20);

            if(!force_create)
            {
                // 2 loops:
                // loop 0 tries to find players that should still be online
                // loop 1 adds
                for(uint32 loopId = 0; loopId < 2; loopId++)
                {
                    // store all available (not online) chars to pick one later
                    std::deque<uint32> avail_chars;
                    for(VPCharMap::iterator it = _chars.begin(); it != _chars.end(); it++)
                    {
                        if(!it->second.online)
                        {
                            if(high_level_login || it->second.lvl < _max_level)
                            {
                                if(loopId == 0)
                                {
                                    if(it->second.planned_logout && (curtime + 120 < it->second.planned_logout) ) // no sense to logout shortly after login
                                    {
                                        avail_chars.push_back(it->first);
                                        DEBUG_LOG("VPM: Login: %s should still be online...",it->second.name.c_str());
                                    }
                                }
                                if(loopId == 1)
                                {
                                    if(it->second.last_logout + it->second.traits.offline_time_min < curtime)
                                        avail_chars.push_back(it->first);
                                }
                            }
                        }
                    }
                    if(avail_chars.size())
                    {
                        DEBUG_LOG("VPM: Picking 1 of %u avail. chars (loop %u)",avail_chars.size(), loopId);
                        VirtualPlayer *vp = &_chars[ avail_chars[ urand(0, avail_chars.size() - 1) ] ];
                        vp->Init();
                        _SetOnline(vp);
                        break;

                    }
                    else if(loopId > 0) //
                    {
                        cant_login = true;
                        DEBUG_LOG("VPM: No more avail. chars or login error!");
                        break;
                    }
                }
            }
            
            if(force_create || cant_login)
            {
                VirtualPlayer *vp = _GenerateNewChar(); // calls vp->Init()
                _SetOnline(vp);        
            }
        }
    }
    else _logincheck_timediff -= difftime;

    for(VPCharMap::iterator vpit = _chars.begin(); vpit != _chars.end(); vpit++)
    {
        VirtualPlayer *vp = &(vpit->second);
        if(vp->online)
        {
            // select correct zone data for current char's zone
            VirtualPlayerZoneDataEntry ze;

            // handle zone changes
            if(vp->next_zonechange < difftime)
            {
                std::deque<uint32> avail_zones;

                // special rules for low-level chars below
                if(vp->lvl < 14)
                {
                    switch(vp->race)
                    {
                    case 1: // Human
                        avail_zones.push_back(12); // elwynn forest
                        avail_zones.push_back(40); // westfall
                        break;
                    case 2: // Orc
                    case 8: // Troll
                        avail_zones.push_back(14); // durotar
                        avail_zones.push_back(17); // barrens
                        break;
                    case 3: // Dwarf
                    case 7: // Gnome
                        avail_zones.push_back(1); // dun morogh
                        avail_zones.push_back(38); // loch modan
                        break;
                    case 4: // Night elf
                        avail_zones.push_back(141); // teldrassil
                        avail_zones.push_back(148); // darkshore
                        break;
                    case 5: // Undead
                        avail_zones.push_back(85); // tirisfal glades
                        avail_zones.push_back(130); // silverpine forest
                        break;
                    case 6: // Tauren
                        avail_zones.push_back(215); // mulgore
                        avail_zones.push_back(17); // barrens
                        break;
                    case 10: // BloodElf
                        avail_zones.push_back(3430); // eversong woods
                        avail_zones.push_back(3433); // ghostlands
                        break;
                    case 11: // Draenei
                        avail_zones.push_back(3524); // azuremyst isle
                        avail_zones.push_back(148); // darkshore
                        break;
                    }
                }
                else
                {
                    for(VPZoneMap::iterator it = _zones.begin(); it != _zones.end(); it++)
                        if(vp->lvl >= it->second.lvlmin)
                        {
                            bool avail = true;
                            const AreaTableEntry *ate = GetAreaEntryByAreaID(it->first);
                            if(ate)
                            {
                                uint32 team;
                                switch(vp->race)
                                {
                                case 1:
                                case 3:
                                case 7:
                                case 4:
                                case 11:
                                    team = 2;
                                    break;
                                default:
                                    team = 4;
                                }
                                // do not go to hostile zones as low lvl
                                if(ate->team && vp->lvl < 26 && ate->team != team)
                                {
                                    avail = false;
                                    DEBUG_LOG("VPM: -- Zone skipped; zoneteam=%u, playerteam=%u (race=%u)",ate->team,team,vp->race);
                                }
                            }
                            if(avail)
                                avail_zones.push_back(it->first);
                        }
                }

                if(avail_zones.size())
                {
                    uint32 newzone = avail_zones[ urand(0, avail_zones.size() - 1) ];
                    DEBUG_LOG("VPM: %s (%u) zoning from %u to %u",vp->name.c_str(), vp->id, vp->zone, newzone);
                    vp->zone = newzone;
                }
                else
                {
                    sLog.outError("VPM: %s (%u): No zone available to go to, using failsafe values",vp->name.c_str(),vp->id);
                    vp->zone = 0;
                }

                // workaround for wrongly selected zone or fucked up DB data
                VirtualPlayerZoneDataEntry ze_tmp;
                ze_tmp.zone = 0;
                ze_tmp.lvlmin = vp->lvl;
                ze_tmp.lvlmax = vp->lvl + 3;
                _zones[0] = ze_tmp;

                vp->_CalcZoneTime(_zones[vp->zone]);
            }
            else vp->next_zonechange -= difftime;

            
            // apply new zone settings or select data for current zone to be used below
            VPZoneMap::iterator zmit = _zones.find(vp->zone);
            if(zmit != _zones.end())
            {
                ze = zmit->second;
            }
            else
            {
                sLog.outError("VPM: %s (%u) is in bad zone %u! Using failsafe values! >.>",vp->name.c_str(), vp->id, vp->zone);
                ze.zone = 0;
                ze.lvlmin = vp->lvl;
                ze.lvlmax = vp->lvl + 3;
                vp->zone = 0;
            }



            // handle quest finish/timer
            if(vp->next_questtime < difftime)
            {
                uint32 qlvl = urand(ze.lvlmin,ze.lvlmax);
                uint32 questId = 0;
                while(!questId && qlvl)
                {
                    VPQuestMap::iterator qit = _quests.find(qlvl);
                    if(qit != _quests.end())
                    {
                        std::deque<uint32>& qs = qit->second;
                        if(qs.size())
                            questId = qs[ urand(0, qs.size() - 1) ];
                    }
                    else
                        qlvl--;
                }
                if(questId)
                    vp->FinishQuest(questId);
                else
                    sLog.outError("VPM: Could not detect good quest for %s (%u)", vp->name.c_str(), vp->id);

                vp->next_questtime = urand(vp->traits.questtime_min, vp->traits.questtime_max);
                if(vp->lvl < 23)
                    vp->next_questtime *= 0.6;
            }
            else vp->next_questtime -= difftime;


            // handle mob kills. burst -> idle is already handled in child funcs
            if(vp->next_killtime < difftime)
            {
                vp->HandleKill( urand(ze.lvlmin, ze.lvlmax) );
            }
            else vp->next_killtime -= difftime;


            // handle idle -> burst mode switch
            if(!vp->killburst)
            {
                if(vp->next_bursttime < difftime)
                {
                    vp->killburst = true;
                    DEBUG_LOG("VPM: %s switched to burst mode", vp->name.c_str());
                    vp->_UpdateKillTimers();
                }
                else vp->next_bursttime -= difftime;
            }


            // handle position change inside zone, save and logout
            if(vp->next_savetime < difftime)
            {
                // position change
                _Save(vp);
                vp->next_savetime = sWorld.getConfig(CONFIG_UINT32_INTERVAL_SAVE) / 1000;
            }
            else vp->next_savetime -= difftime;

        }
    }
}

void VirtualPlayerMgr::ClearOnlineBots(void)
{
    if(!_enabled)
        return;
    DEBUG_LOG("Clearing online bots...");
    CharacterDatabase.Execute("DELETE FROM vp_online");
    CharacterDatabase.Execute("DELETE FROM characters WHERE account=1 AND guid NOT IN(SELECT guid FROM vp_chars)");
    CharacterDatabase.Execute("UPDATE characters SET online=0 WHERE account=1");
}

