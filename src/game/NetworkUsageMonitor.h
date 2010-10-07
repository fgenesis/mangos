#ifndef NETWORKUSAGEMONITOR_H
#define NETWORKUSAGEMONITOR_H

#include "Common.h"
#include "Policies/Singleton.h"
#include "ObjectGuid.h"
#include "Opcodes.h"

class WorldPacket;


class NetworkUsageMonitor
{
public:
    NetworkUsageMonitor();
    ~NetworkUsageMonitor();

    void CountOutgoing(const WorldPacket *pkt, uint32 extra);
    void CountIncoming(const WorldPacket *pkt, uint32 extra);
    void SaveData(void);

    inline uint64 GetLastMinBytesOut(void) { return _lastminBytesOut; }
    inline uint64 GetLastMinPacketsOut(void) { return _lastminPktOut; }
    inline uint64 GetTotalBytesOut(void) { return _totalBytesOut; }
    inline uint64 GetTotalPacketsOut(void) { return _totalPktOut; }

    inline uint64 GetLastMinBytesIn(void) { return _lastminBytesIn; }
    inline uint64 GetLastMinPacketsIn(void) { return _lastminPktIn; }
    inline uint64 GetTotalBytesIn(void) { return _totalBytesIn; }
    inline uint64 GetTotalPacketsIn(void) { return _totalPktIn; }

private:
    time_t _lasttimeOut;
    time_t _lasttimeIn;

    uint64 _totalPktOut;
    uint64 _totalBytesOut;
    uint64 _lastminPktOut;
    uint64 _lastminBytesOut;

    uint64 _totalPktIn;
    uint64 _totalBytesIn;
    uint64 _lastminPktIn;
    uint64 _lastminBytesIn;

    uint64 _packetsOut[NUM_MSG_TYPES];
    uint64 _bytesOut[NUM_MSG_TYPES];

    uint64 _packetsIn[NUM_MSG_TYPES];
    uint64 _bytesIn[NUM_MSG_TYPES];
};



#define sNetMon MaNGOS::Singleton<NetworkUsageMonitor>::Instance()

#endif
