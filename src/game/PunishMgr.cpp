#include "Policies/SingletonImp.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Util.h"
#include "SharedDefines.h"
#include "Language.h"
#include "ProgressBar.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include "Chat.h"
#include "PunishMgr.h"

INSTANTIATE_SINGLETON_1( PunishMgr );


PunishMgr::PunishMgr()
: _maxpoints(0)
{
    _handlers["points"]    = &PunishMgr::_HandlePoints;
    _handlers["chanmute"]  = &PunishMgr::_HandleChannelMute;
    _handlers["mute"]      = &PunishMgr::_HandleMute;
    _handlers["notify"]    = &PunishMgr::_HandleNotify;
    _handlers["ban"]       = &PunishMgr::_HandleBan;
}

PunishMgr::~PunishMgr()
{
    _Clear();
    _maxpoints = 0;
}

void PunishMgr::_Clear()
{
    for(PunishActionMap::iterator i = _actions.begin(); i != _actions.end(); i++)
        for(PunishActionList::iterator j = i->second.begin(); j != i->second.end(); j++)
            delete *j;
    for(PunishTriggerMap::iterator i = _triggers.begin(); i != _triggers.end(); i++)
        for(PunishTriggerList::iterator j = i->second.begin(); j != i->second.end(); j++)
            delete *j;
    _triggers.clear();
    _actions.clear();
}

bool PunishMgr::_Valid(const char *s)
{
    return _handlers.find(s) != _handlers.end();
}

void PunishMgr::Load(void)
{
    sLog.outString("Loading punish actions...");
    _Clear();
    std::string t;
    uint32 actionCount = 0, trigCount = 0;
    QueryResult *result = LoginDatabase.Query("SELECT what, action, arg FROM punish_action");
    if(result)
    {
        barGoLink bar(result->GetRowCount());

        do
        {
            Field *fields = result->Fetch();
            t = fields[1].GetCppString();
            if(!_Valid(t.c_str()))
            {
                sLog.outError("punish_action: Action '%s' unknown, skipped", t.c_str());
                continue;
            }
            PunishAction *act = new PunishAction;
            act->what = fields[0].GetCppString();
            act->action = t;
            act->arg = fields[2].GetCppString();
            _actions[act->what].push_back(act);
            bar.step();
            ++actionCount;
        }
        while(result->NextRow());

        delete result;
    }
    else
        sLog.outError("Could not load punish actions!");

    sLog.outString("Loading punish trigger values...");
    result = LoginDatabase.Query("SELECT value, action, arg FROM punish_value");
    if(result)
    {
        barGoLink bar(result->GetRowCount());

        do
        {
            Field *fields = result->Fetch();
            t = fields[1].GetCppString();
            
            if(!_Valid(t.c_str()))
            {
                sLog.outError("punish_value: Action '%s' unknown, skipped", t.c_str());
                continue;
            }
            PunishTrigger *trg = new PunishTrigger;
            trg->value = fields[0].GetUInt32();
            trg->action = t;
            trg->arg = fields[2].GetCppString();
            _triggers[trg->value].push_back(trg);
            _maxpoints = std::max(trg->value, _maxpoints);
            bar.step();
            ++trigCount;
        }
        while(result->NextRow());

        delete result;
    }
    else
        sLog.outError("Could not load punish triggers!");

    sLog.outString();
    sLog.outString(">> [FG] Punish system: Loaded %u actions, %u triggers. Max. %u points.", actionCount, trigCount, _maxpoints);
}

void PunishMgr::_SendPointsInfo(Player *who, uint32 pts)
{
    if(!who)
        return;

    float perc = ((float)pts / (float)GetMaxPoints()) * 100.0f;

    ChatHandler(who).PSendSysMessage(LANG_BADPOINTS_REACHED, pts, GetMaxPoints(), perc);

}

// note that who can be NULL in case player is offline
// return false ONLY if supplied input is incorrect
bool PunishMgr::Handle(uint32 accid, Player *who, Player *pSource, std::string playername, std::string what, std::string reason)
{
    PunishActionMap::iterator act = _actions.find(what);
    if(act == _actions.end())
        return false;

    ValueState state;
    int32 oldpoints;
    state.badpoints = oldpoints = (int32)WorldSession::GetBadPointsFromDB(accid);

    // accumulate all channel mutes, normal mutes, and ban times before doing something
    for(PunishActionList::iterator it = act->second.begin(); it != act->second.end(); it++)
    {
        InternalHandlerMap::iterator hnd = _handlers.find((*it)->action);
        if(hnd == _handlers.end())
        {
            sLog.outError("PunishMgr::Handle() [action] invalid action '%s'", (*it)->action.c_str());
            continue;
        }
        InternalHandler handler = hnd->second;
        (*this.*handler)(who, &state, (*it)->arg.c_str());
    }

    // points cant go negative
    if(state.badpoints < 0)
        state.badpoints = 0;

    // accumulate more channel mutes, normal mutes, and ban times before doing something
    // if multiple notifications would be sent, send only the one with the highest amount of points
    // the map is sorted, so we iterate until the points get too high
    PunishTrigger *usedNotify = NULL;
    uint32 curpoints = (uint32)state.badpoints; // save, in case the state is changed
    for(PunishTriggerMap::iterator trig = _triggers.begin(); trig != _triggers.end() && trig->first <= curpoints; trig++)
    {
        PunishTriggerList& ptlist = trig->second;
        for(PunishTriggerList::iterator it = ptlist.begin(); it != ptlist.end(); it++)
        {
            PunishTrigger *pt = *it;

            // special case
            if(pt->action == "notify")
            {
                usedNotify = pt;
                continue;
            }
            InternalHandlerMap::iterator hnd = _handlers.find((*it)->action);
            if(hnd == _handlers.end())
            {
                sLog.outError("PunishMgr::Handle() [value] invalid action '%s'", pt->action.c_str());
                continue;
            }
            
            InternalHandler handler = hnd->second;
            (*this.*handler)(who, &state, pt->arg.c_str());
        }
    }

    // points cant go negative - just in case someone added a point drop as action
    if(state.badpoints < 0)
        state.badpoints = 0;

    // save new points
    LoginDatabase.PExecute("INSERT IGNORE INTO account_badpoints (id) VALUES (%u)", accid);
    uint64 curtime = (uint64)time(NULL);
    LoginDatabase.PExecute("UPDATE account_badpoints SET curpts=%u, lasttime="UI64FMTD" WHERE id=%u", state.badpoints, curtime, accid);
    // do not touch maxpts, realmd will take care of that


    // -- perform actions --

    // first, send cached notification
    if(usedNotify)
    {
        InternalHandlerMap::iterator hnd = _handlers.find(usedNotify->action);
        MANGOS_ASSERT(hnd != _handlers.end());
        InternalHandler handler = hnd->second;
        (*this.*handler)(who, &state, usedNotify->arg.c_str());
    }

    // notify if points gained
    if(oldpoints < state.badpoints)
        _SendPointsInfo(who, state.badpoints);

    std::stringstream finalReason;
    finalReason << "[Auto-Punish for '" << what << "': " << state.badpoints << " bad points]. Reason: '" << reason << "'";

    // channel mute
    if(state.chanMuteTime)
    {
        time_t mutetime = time(NULL) + state.chanMuteTime;
        LoginDatabase.PExecute("UPDATE account SET chanmutetime = " UI64FMTD " WHERE id = '%u'", uint64(mutetime), accid);

        if (who)
        {
            who->GetSession()->m_channelMuteTime = mutetime;
            ChatHandler(who).PSendSysMessage(LANG_YOUR_GLOBAL_CHANNELS_CHAT_DISABLED, state.chanMuteTime / 60);
        }
    }

    // global mute
    if(state.allMuteTime)
    {
        time_t mutetime = time(NULL) + state.allMuteTime;
        LoginDatabase.PExecute("UPDATE account SET mutetime = " UI64FMTD " WHERE id = '%u'", uint64(mutetime), accid);

        if (who)
        {
            who->GetSession()->m_channelMuteTime = mutetime;
            ChatHandler(who).PSendSysMessage(LANG_YOUR_GLOBAL_CHANNELS_CHAT_DISABLED, state.allMuteTime / 60);
        }
    }

    // banning
    if(state.banTime)
    {
        BanReturn banret = sWorld.BanAccount(BAN_CHARACTER, playername, state.banTime < 0 ? 0 : state.banTime , finalReason.str(), pSource ? pSource->GetSession()->GetPlayerName() : "");
        if(pSource)
        {
            switch(banret)
            {
                case BAN_SUCCESS:
                    if (state.banTime > 0)
                        ChatHandler(pSource).PSendSysMessage(LANG_BAN_YOUBANNED, playername.c_str(), secsToTimeString(state.banTime,true).c_str(), finalReason.str().c_str());
                    else
                        ChatHandler(pSource).PSendSysMessage(LANG_BAN_YOUPERMBANNED, playername.c_str(), finalReason.str().c_str());
                    break;

                case BAN_SYNTAX_ERROR: // we should not reach this point...
                    ChatHandler(pSource).SendSysMessage("PunishMgr internal error - ban failed, must be wrong values in database");
                    return true;

                case BAN_NOTFOUND: // we should not reach this point...
                    ChatHandler(pSource).PSendSysMessage(LANG_BAN_NOTFOUND,"character", playername.c_str());
                    return true;
            }
        }
    }


    return true;
}

void PunishMgr::_HandleChannelMute(Player *pl, ValueState *state, const char *arg)
{
    int32 mins = atoi(arg);
    if(mins <= 0)
        return;

    uint32 notspeaktime = 60 * mins;

    state->chanMuteTime = std::max(notspeaktime, state->chanMuteTime);
}


void PunishMgr::_HandleMute(Player *pl, ValueState *state, const char *arg)
{
    int32 mins = atoi(arg);
    if(mins <= 0)
        return;

    uint32 notspeaktime = 60 * mins;

    state->allMuteTime = std::max(notspeaktime, state->allMuteTime);
}

void PunishMgr::_HandlePoints(Player *pl, ValueState *state, const char *arg)
{
    int32 pts = atoi(arg);
    state->badpoints += pts;
}

void PunishMgr::_HandleNotify(Player *pl, ValueState *state, const char *arg)
{
    if(!pl)
        return;

    ChatHandler(pl).SendSysMessage(arg);
    pl->GetSession()->SendNotification(arg);
}

void PunishMgr::_HandleBan(Player *pl, ValueState *state, const char *arg)
{
    std::string cp(arg); // copy original arg, because the ChatHandler functions modify their input!
    char *ptr = (char*)cp.c_str();

    char* duration = ChatHandler::ExtractArg(&ptr);                     // time string
    if(!duration)
    {
        sLog.outError("PunishMgr::_HandleBan(): Invalid arg '%s'", arg);
        return;
    }

    int32 duration_secs;

    // use atoi first, because TimeStringToSecs("-1") returns 0 !
    duration_secs = atoi(duration);
    if(duration_secs >= 0)
        duration_secs = TimeStringToSecs(duration);

    // if already perm banned, do nothing anymore
    if(duration_secs >= 0 && state->banTime >= 0)
    {
        if(duration_secs > state->banTime)
            state->banTime = duration_secs;
    }
    else
        state->banTime = -1;
}
