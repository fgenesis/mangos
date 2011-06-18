
#include "PlayerDropMgr.h"
#include "Log.h"
#include "ProgressBar.h"
#include "Database/DatabaseEnv.h"
#include "Policies/SingletonImp.h"
#include "ItemPrototype.h"
#include "ObjectMgr.h"


PlayerDropStorage sPlayerDropStore;

void LoadPlayerDrops(void)
{

    const char *tablename = "player_drop_template";
    sLog.outString("Loading player drop templates from %s - currently loaded: %u",tablename, sPlayerDropStore.size());
    QueryResult *result = WorldDatabase.PQuery("SELECT guid, racemask, classmask, gender, item, chance, mincount, maxcount, kminlvl, kmaxlvl, vminlvl, vmaxlvl, lvldiff, map FROM %s",tablename);
    if(!result)
    {
        sLog.outError("[FG] PlayerDropMgr: Can't load table %s, doesn't exist!",tablename);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    std::ostringstream ssFailed;

    PlayerDropEntry pd;
    uint32 count = 0;
    uint32 countfailed = 0;

    sPlayerDropStore.clear();

    do
    {
        Field *fields = result->Fetch();
        bar.step();

        pd.guid = fields[0].GetUInt32();
        pd.racemask = fields[1].GetUInt32();;
        pd.classmask = fields[2].GetUInt32();
        pd.gender = fields[3].GetUInt32();
        pd.item = fields[4].GetUInt32();
        pd.chance = fields[5].GetFloat();
        pd.mincount = fields[6].GetUInt32();
        pd.maxcount = fields[7].GetUInt32();
        pd.kminlvl = fields[8].GetUInt32();
        pd.kmaxlvl = fields[9].GetUInt32();
        pd.vminlvl = fields[10].GetUInt32();
        pd.vmaxlvl = fields[11].GetUInt32();
        pd.lvldiff = int32(fields[12].GetUInt32());
        pd.map = fields[13].GetUInt32();

        if(!pd.mincount)
            pd.mincount = 1;
        if(!pd.maxcount)
            pd.maxcount = 1;
        if(pd.maxcount < pd.mincount)
            std::swap(pd.mincount,pd.maxcount);
        if(pd.kmaxlvl && pd.kminlvl && pd.kmaxlvl < pd.kminlvl)
            std::swap(pd.kminlvl,pd.kmaxlvl);
        if(pd.vmaxlvl && pd.vminlvl && pd.vmaxlvl < pd.vminlvl)
            std::swap(pd.vminlvl,pd.vmaxlvl);
        if(pd.guid)
        {
            pd.classmask = -1;
            pd.racemask = -1;
        }

        const ItemPrototype *proto = pd.item ? sObjectMgr.GetItemPrototype(pd.item) : NULL; // fast check

        if(proto && pd.chance > 0 && pd.racemask && pd.classmask && pd.kminlvl <= 255 && pd.vminlvl <= 255 && pd.lvldiff < 255)
        {
            sPlayerDropStore.push_back(pd);
            count++;
        }
        else
        {
            countfailed++;
            char tmp[255];
            sprintf(tmp, "Skipped: guid=%u race=%u class=%u item=%u chance=%.3f lvldiff=%u\n",pd.guid,pd.racemask,pd.classmask,pd.item,pd.chance,pd.lvldiff);
            ssFailed << tmp;
        }


    } while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString( ">> [FG] Loaded %u player drop definitions", count );
    if(countfailed)
    {
        sLog.outError("Failed to load %u rows. List:", countfailed);
        sLog.outError("%s",ssFailed.str().c_str());
    }
}
