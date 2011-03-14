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

    void CountQuery(const char *qstr, uint32 qtime = 0);
    void SaveData(void);
    void SetEnabled(bool b) { m_enable = b; }
    void SetDatabase(DatabaseType *db);
    void SetStartTime(uint64 t) { m_starttime = t; }

private:
    struct QueryInfo
    {
        QueryInfo(): count(0), totalTime(0) {}
        uint64 count;
        uint64 totalTime;
    };
    typedef std::map<std::string, QueryInfo> QueryCountStore;

    typedef ACE_Recursive_Thread_Mutex LockType;
    typedef ACE_Guard<LockType> Guard;

    DatabaseType *m_pDatabase;
    uint64 m_starttime;
    QueryCountStore m_store;
    LockType m_mutex;
    bool m_enable;
    
};



#define sQueryCounter MaNGOS::Singleton<QueryCounter>::Instance()

#endif
