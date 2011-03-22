#ifndef QUERYCOUNTER_H
#define QUERYCOUNTER_H

#include "Common.h"
#include "Policies/Singleton.h"
#include <ace/Thread_Mutex.h>
#include <ace/RW_Thread_Mutex.h>
#include "Database/DatabaseEnv.h"


class QueryCounter
{
public:
    QueryCounter();
    ~QueryCounter();

    void CountQuery(const Database *db, const char *qstr);
    void SaveData(void);
    void SetEnabled(bool b) { m_enable = b; }
    void SetDatabase(Database *db);
    void SetStartTime(uint64 t) { m_starttime = t; }

private:
    typedef std::pair<char, std::string> QueryIndex;
    struct QueryInfo // wrapper for auto-init with 0
    {
        QueryInfo(): count(0) {}
        uint64 count;
    };
    // a bit overcomplicated maybe, but better then indexing with std::pair<ident, query>
    typedef std::map<std::string, QueryInfo> QueryCountStore;
    typedef std::map<char, QueryCountStore> DBQueryCountStore;

    typedef ACE_Recursive_Thread_Mutex LockType;
    typedef ACE_Guard<LockType> Guard;

    Database *m_pDatabase;
    uint64 m_starttime;
    DBQueryCountStore m_store;
    LockType m_mutex;
    bool m_enable;
    
};



#define sQueryCounter MaNGOS::Singleton<QueryCounter>::Instance()

#endif
