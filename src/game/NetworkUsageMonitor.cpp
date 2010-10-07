#include "NetworkUsageMonitor.h"
#include "Policies/SingletonImp.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "World.h"
#include "WorldPacket.h"


INSTANTIATE_SINGLETON_1( NetworkUsageMonitor );



NetworkUsageMonitor::NetworkUsageMonitor()
{
    for(uint32 i = 0; i < NUM_MSG_TYPES; i++)
    {
        _packetsOut[i] = 0;
        _bytesOut[i] = 0;
        _packetsIn[i] = 0;
        _bytesIn[i] = 0;
    }
    _totalPktOut = 0;
    _totalBytesOut = 0;
    _lastminPktOut = 0;
    _lastminBytesOut = 0;

    _totalPktIn = 0;
    _totalBytesIn = 0;
    _lastminPktIn = 0;
    _lastminBytesIn = 0;

    _lasttimeOut = _lasttimeIn = time(NULL);
}

NetworkUsageMonitor::~NetworkUsageMonitor()
{
}

void NetworkUsageMonitor::CountOutgoing(const WorldPacket *pkt, uint32 extra)
{
    if(!sWorld.getConfig(CONFIG_BOOL_NETMON_RECORD_OUTGOING))
        return;

    uint32 bytes = pkt->wpos() + extra;
    uint32 opcode = pkt->GetOpcode();
    ++_totalPktOut;
    
    _totalBytesOut += bytes;

    ++_packetsOut[opcode];
    _bytesOut[opcode] += bytes;

    time_t curtime = time(NULL);

    if((curtime - _lasttimeOut) < 60)
    {
        ++_lastminPktOut;
        _lastminBytesOut += bytes;
    }
    else
    {
        _lasttimeOut = curtime;
        _lastminPktOut = 1;
        _lastminBytesOut = bytes;
    }
}

void NetworkUsageMonitor::CountIncoming(const WorldPacket *pkt, uint32 extra)
{
    if(!sWorld.getConfig(CONFIG_BOOL_NETMON_RECORD_INCOMING))
        return;

    uint32 bytes = pkt->size() + extra;
    uint32 opcode = pkt->GetOpcode();
    ++_totalPktIn;

    _totalBytesIn += bytes;

    ++_packetsIn[opcode];
    _bytesIn[opcode] += bytes;

    time_t curtime = time(NULL);

    if((curtime - _lasttimeIn) < 60)
    {
        ++_lastminPktIn;
        _lastminBytesOut += bytes;
    }
    else
    {
        _lasttimeIn = curtime;
        _lastminPktOut = 1;
        _lastminBytesOut = bytes;
    }
}

void NetworkUsageMonitor::SaveData(void)
{
    uint64 starttime = uint64(sWorld.GetStartTime());

    if(sWorld.getConfig(CONFIG_BOOL_NETMON_SAVE_OUTGOING))
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM network_usage_out WHERE starttime = %u", starttime);
        for(uint32 i = 0; i < NUM_MSG_TYPES; i++)
        {
            if(!_packetsOut[i])
                continue;

            CharacterDatabase.PExecute("INSERT INTO network_usage_out VALUES ("UI64FMTD", %u, "UI64FMTD", "UI64FMTD")",
                starttime, i, _packetsOut[i], _bytesOut[i]);
        }

        CharacterDatabase.CommitTransaction();
    }

    if(sWorld.getConfig(CONFIG_BOOL_NETMON_SAVE_INCOMING))
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute("DELETE FROM network_usage_in WHERE starttime = %u", starttime);
        for(uint32 i = 0; i < NUM_MSG_TYPES; i++)
        {
            if(!_packetsIn[i])
                continue;

            CharacterDatabase.PExecute("INSERT INTO network_usage_in VALUES ("UI64FMTD", %u, "UI64FMTD", "UI64FMTD")",
                starttime, i, _packetsIn[i], _bytesIn[i]);
        }

        CharacterDatabase.CommitTransaction();
    }
}

