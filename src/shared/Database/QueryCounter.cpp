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

void QueryCounter::SetDatabase(Database *db)
{
    Guard g(m_mutex); // just to be safe
    m_pDatabase = db;
}

void QueryCounter::CountQuery(const Database *db, const char *qstr)
{
    if(!m_enable)
        return;

    Guard g(m_mutex);
    QueryInfo& qi = m_store[db->GetIdent()][qstr]; // auto-initialized if not present
    ++qi.count;
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

    m_pDatabase->BeginTransaction();
    m_pDatabase->PExecute("DELETE FROM query_counter WHERE starttime = "UI64FMTD, m_starttime);
    for (DBQueryCountStore::iterator dbi =  m_store.begin(); dbi != m_store.end(); ++dbi)
    {
        char dbIdent = dbi->first;
        QueryCountStore &qcs = dbi->second;
        for (QueryCountStore::iterator it = qcs.begin(); it != qcs.end(); ++it)
        {
            std::string esc(it->first);
            m_pDatabase->escape_string(esc);
            m_pDatabase->PExecute("INSERT INTO query_counter VALUES ("UI64FMTD", '%c', '%s', "UI64FMTD")",
                m_starttime, dbIdent, esc.c_str(), it->second.count);
        }
    }
    m_pDatabase->CommitTransaction();
}
