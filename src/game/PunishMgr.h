#ifndef PUNISHMGR_H
#define PUNISHMGR_H

#include "Common.h"
#include "Policies/Singleton.h"

class Player;

class PunishMgr
{
    struct PunishAction
    {
        std::string what;
        std::string action;
        std::string arg;
    };

    struct PunishTrigger
    {
        uint32 value;
        std::string action;
        std::string arg;
    };

    struct ValueState
    {
        ValueState(): chanMuteTime(0), allMuteTime(0), banTime(0), badpoints(0) {}
        uint32 chanMuteTime;
        uint32 allMuteTime;
        int32 banTime;
        int32 badpoints;
    };

    typedef std::deque<PunishAction*> PunishActionList;
    typedef std::deque<PunishTrigger*> PunishTriggerList;
    typedef std::map<std::string, PunishActionList> PunishActionMap;
    typedef std::map<uint32, PunishTriggerList> PunishTriggerMap;
    typedef void (PunishMgr::*InternalHandler)(Player*, ValueState*, const char*);
    typedef std::map<std::string, InternalHandler> InternalHandlerMap;

public:
    PunishMgr();
    ~PunishMgr();
    void Load(void);
    bool Handle(uint32 accid, Player *who, Player *pSource, std::string playername, std::string what, std::string reason);
    uint32 GetMaxPoints(void) { return _maxpoints; }

private:
    void _Clear(void);
    bool _Valid(const char *s);
    void _SendPointsInfo(Player *who, uint32 pts);
    void _SendPunishResult(Player *pSource, std::string& name, ValueState& state, std::string& what);
    PunishActionMap _actions;
    PunishTriggerMap _triggers;
    InternalHandlerMap _handlers;
    uint32 _maxpoints;

    // handlers
    void _HandleChannelMute(Player *pl, ValueState *state, const char* arg);
    void _HandleMute(Player *pl, ValueState *state, const char* arg);
    void _HandleBan(Player *pl, ValueState *state, const char* arg);
    void _HandlePoints(Player *pl, ValueState *state, const char* arg);
    void _HandleNotify(Player *pl, ValueState *state, const char* arg);

};

#define sPunishMgr MaNGOS::Singleton<PunishMgr>::Instance()

#endif
