#include "QueryCounter.h"
#include "Policies/SingletonImp.h"
#include "Log.h"

INSTANTIATE_SINGLETON_1( QueryCounter );


QueryCounter::QueryCounter()
: m_pDatabase(NULL), m_starttime(0), m_enable(false)
{
}

QueryCounter::~QueryCounter()
{
}

void QueryCounter::SetDatabase(DatabaseType *db)
{
    Guard g(m_mutex); // just to be safe
    m_pDatabase = db;
}

void QueryCounter::CountQuery(const char *qstr, uint32 qtime /* = 0 */)
{
    if(!m_enable)
        return;

    Guard g(m_mutex);
    QueryInfo& qi = m_store[qstr]; // auto-initialized if not present
    ++qi.count;
    qi.totalTime += qtime;
}

void QueryCounter::SaveData(void)
{
    if (!m_enable)
        return;

    if (!m_pDatabase)
    {
        sLog.outError("QueryCounter: m_pDatabase == NULL!");
        return;
    }

    DEBUG_LOG("QueryCounter: Saving %u queries...", m_store.size());

    Guard g(m_mutex);

    //m_enable = false; // need to disable recording for this moment, otherwise PExecute() calling CountQuery() again would deadlock
    m_pDatabase->BeginTransaction();
    m_pDatabase->PExecute("DELETE FROM query_counter WHERE starttime = "UI64FMTD, m_starttime);
    for (QueryCountStore::iterator it = m_store.begin(); it != m_store.end(); ++it)
    {
        std::string esc(it->first);
        m_pDatabase->escape_string(esc);
        m_pDatabase->PExecute("INSERT INTO query_counter VALUES ("UI64FMTD", '%s', "UI64FMTD", "UI64FMTD")",
            m_starttime, esc.c_str(), it->second.count, it->second.totalTime);
    }
    m_pDatabase->CommitTransaction();
    //m_enable = true;
}
