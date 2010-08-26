#ifndef VIRTUALPLAYERMGR_H
#define VIRTUALPLAYERMGR_H

#include "Common.h"
#include "Policies/Singleton.h"
#include "ObjectGuid.h"

struct VirtualPlayerTraits
{
    uint32 id;
    float xp_kill; // perc
    float xp_quest; // perc
    uint32 staytime_min; // secs
    uint32 staytime_max; // secs
    uint32 questtime_min; // secs
    uint32 questtime_max; // secs
    uint32 killtime_burst_min; // secs
    uint32 killtime_burst_max; // secs
    uint32 killtime_idle_min; // secs
    uint32 killtime_idle_max; // secs
    uint32 kills_burst_min;
    uint32 kills_burst_max;
    uint32 time_idle_min; // secs
    uint32 time_idle_max; // secs
    uint32 offline_time_min;
};

struct VirtualPlayerNameEntry
{
    std::string str;
    bool start;
    bool middle;
    bool end;
};

struct VirtualPlayerZoneDataEntry
{
    uint32 zone;
    uint32 lvlmin;
    uint32 lvlmax;
};

struct VirtualPlayerHourPopulation
{
    VirtualPlayerHourPopulation()
    {
        hour = min = max = 0;
    }
    uint32 hour;
    uint32 min;
    uint32 max;
};

struct VirtualPlayerZonePosition
{
    uint32 zone;
    float x,y,z;
    uint32 map;
};

class VirtualPlayer
{
public:
    VirtualPlayer()
    {
        id = 0;
        next_savetime = 0;
        planned_logout = 0;
        last_logout = 0;
        online = false;
        px = py = pz = 0;
        map = 0;
    }
    void Init(void); // must be called after load
    void AddXP(uint32);
    void HandleKill(uint32);
    void FinishQuest(uint32);
    void _UpdateKillTimers(void);
    void _CalcZoneTime(VirtualPlayerZoneDataEntry);
    void _Export(void);

    // in DB
    uint32 id;
    uint32 traits_id;
    std::string name;
    uint8 race;
    uint8 class_;
    uint8 lvl;
    uint32 xp;
    uint32 honor;
    uint32 quests;
    std::string info;
    time_t planned_logout; // unixtime, because stored in db
    time_t last_logout; // unixtime, because stored in db
    uint32 zone;
    bool gender;
    std::string guild;
    ObjectGuid guid;

    // calculated when loaded
    uint32 lvlup_xp;
    VirtualPlayerTraits traits;

    // dynamic
    bool online : 1;
    time_t next_savetime; // difftime
    time_t next_killtime; // difftime
    time_t next_bursttime; // difftime
    time_t next_questtime; // difftime
    time_t next_zonechange; // difftime
    bool killburst;
    uint32 left_burst_kills;

    // in DB, but not needed to load
    float px,py,pz;
    uint32 map;


};

typedef std::map<uint32,VirtualPlayerZoneDataEntry> VPZoneMap;
typedef std::map<uint32,VirtualPlayerTraits> VPTraitsMap;
typedef std::map<uint32,VirtualPlayer> VPCharMap;
typedef std::map<uint32,std::deque<uint32> > VPQuestMap;
typedef std::map<uint32,VirtualPlayerHourPopulation> VPHourPopMap;
typedef std::deque<VirtualPlayerNameEntry> VPNameList;
typedef std::deque<VirtualPlayerZonePosition> VPZonePosList;
typedef std::map<uint32,VPZonePosList> VPZonePosMap;

class VirtualPlayerMgr
{
public:
    VirtualPlayerMgr();
    ~VirtualPlayerMgr();
    void Load(void);
    void Reload(void)
    {
        if(!_enabled)
            return;
        _must_reload = true;
    }
    void Update(void);
    bool NameExists(std::string); // guarded
    VirtualPlayer *GetChar(uint32); // guarded
    VirtualPlayer *GetChar(std::string); // guarded
    std::set<uint32> GetOnlineSet(void); // guarded
    bool IsOnline(std::string); // guarded
    void ClearOnlineBots(void);
    uint32 GetOnlineCount(void)
    {
        ACE_Guard<ACE_Thread_Mutex> g(_mutex); // probably not needed, but better to be safe...
        return _onlineIDs.size();
    }
    inline uint32 GetHistoryMaxOnlineCount(void) { return _history_maxonlinecount; }
    inline uint32 GetMaxLevel(void) { return _max_level; }
    inline void SetMaxLevel(uint32 l) { _max_level = l; }
    inline void SetOnlineSpread(float s) { _min_max_spread = s; }
    inline void SetOnlineSpreadUpdateTime(uint32 t) { _spread_update_time = t; _spread_update_difftime = t; }
    inline void SetEnabled(bool b) { _enabled = b; }
    inline bool IsEnabled(void) { return _enabled; }
    inline void SetHourOffset(int32 h) { _hour_offset = h; }
    inline void SetLoginCheckInterval(uint32 i) { _logincheck_interval = i; }

private:
    void _Reload(void);
    uint32 _LoadTraits(void);
    uint32 _LoadChars(void);
    uint32 _LoadZoneData(void);
    uint32 _LoadQuests(void);
    uint32 _LoadTimes(void);
    uint32 _LoadNameParts(void);
    uint32 _LoadZonePositions(void);
    uint32 _LoadCharsUpdate(void);
    //uint32 _CleanupZonePositions(void);
    //uint32 _InsertMoreZonePositions(void);
    void _Save(VirtualPlayer*);
    void _SetOffline(VirtualPlayer*);
    void _SetOnline(VirtualPlayer*);
    void _SaveToDB(VirtualPlayer*);
    VirtualPlayer *_GenerateNewChar(void);
    std::string _GenerateName(uint32);
    VPCharMap _chars;
    VPZoneMap _zones;
    VPTraitsMap _traits;
    VPQuestMap _quests;
    VPHourPopMap _hourpop;
    VPNameList _nameparts;
    VPZonePosMap _zonepos;
    uint32 _maxCharID;
    time_t _last_update_time; // unixtime
    uint32 _spread_update_difftime; // difftime
    uint32 _logincheck_timediff; // difftime
    ACE_Thread_Mutex _mutex;
    std::set<uint32> _onlineIDs;
    uint32 _history_maxonlinecount;
    bool _must_reload;


    // settings;
    uint32 _min_online; // if under this value, start to login/create bots
    uint32 _max_online; // if over this value, start to logout bots
    uint32 _spread_update_time; // shift time
    float _min_max_spread; // 0.3 == ranging from -30% to +30%
    uint32 _max_level;
    int32 _hour_offset;
    uint32 _logincheck_interval;
    bool _enabled;

    // related
    uint32 _act_min_online;
    uint32 _act_max_online;
    int32 _cur_hour;
};


#define sVPlayerMgr MaNGOS::Singleton<VirtualPlayerMgr>::Instance()

#endif

