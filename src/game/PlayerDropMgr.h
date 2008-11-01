#ifndef _FG_PLAYERDROPMGR_H
#define _FG_PLAYERDROPMGR_H

#include "Common.h"

struct PlayerDropEntry
{
    uint32 guid;
    uint32 racemask;
    uint32 classmask;
    uint32 gender;
    uint32 item;
    float chance;
    uint32 mincount;
    uint32 maxcount;
    uint32 kminlvl;
    uint32 kmaxlvl;
    uint32 vminlvl;
    uint32 vmaxlvl;
    int32 lvldiff;
    int32 map;
};

typedef std::vector<PlayerDropEntry> PlayerDropStorage;

void LoadPlayerDrops(void);


extern PlayerDropStorage sPlayerDropStore;



#endif
