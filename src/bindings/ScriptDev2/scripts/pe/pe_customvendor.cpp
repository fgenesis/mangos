#include "precompiled.h"
#include "ObjectMgr.h"

extern DatabaseType SD2Database;

enum CustomVendorBuyPhase
{
    CV_BUYPHASE_NULL,
    CV_BUYPHASE_LIST,
    CV_BUYPHASE_CONFIRM
};

enum CustomVendorBuyResult
{
    CV_BUYRESULT_NULL, // silently close gossip
    CV_BUYRESULT_NOT_ENOUGH_MONEY, // no money dialog
    CV_BUYRESULT_MISSING_ITEMS, // missing items dialog
    CV_BUYRESULT_ERROR, // could not add (bags full etc)
    CV_BUYRESULT_OK // give item, remove other items, close gossip
};

struct CustomVendorTemplate
{
    uint32 entry;
    uint32 item;
    uint32 count;
    uint32 cost_id;
    std::string fmt;
};

struct CustomCostTemplate 
{
    uint32 entry;
    int32 itemOrGroup[5];
    uint16 count[5];
    uint32 money;
};

struct ItemGroup
{
    std::string GetName(void) { return (name.empty() ? "[ERROR] Unnamed ItemGroup" : name); }
    uint32 groupId;
    std::deque<uint32> items;
    std::string name;
};

typedef std::list<CustomVendorTemplate> CustomVendorTemplateList;
typedef std::map<uint32, CustomVendorTemplateList> CustomVendorTemplateListMap;
typedef std::map<uint32, CustomCostTemplate> CustomCostTemplateMap;
typedef std::map<uint32, ItemGroup > ItemGroupMap;


CustomVendorTemplateListMap vendorMap;
CustomCostTemplateMap costMap;
ItemGroupMap itemgroups;

void LoadCustomVendorData(void)
{
    outstring_log("SD2: Loading Custom vendor tables...");
    vendorMap.clear();
    costMap.clear();
    itemgroups.clear();

    QueryResult *result = SD2Database.Query("SELECT entry,item,count,cost_id,fmt FROM pe_sd2_customvendor"); 
    if(!result)
    {
        error_log("SD2: CustomVendor: Table is empty! Can't load.");
        return;
    }
    do
    {
        bool has_perc = false;
        Field *fields = result->Fetch();
        CustomVendorTemplate cvt;
        cvt.entry = fields[0].GetUInt32();
        cvt.item = fields[1].GetUInt32();
        cvt.count = fields[2].GetUInt32();
        cvt.cost_id = fields[3].GetUInt32();
        cvt.fmt = fields[4].GetCppString();
        if(!GetItemStore(cvt.item))
        {
            error_log("SD2: CustomVendor: Item %u does not exist!", cvt.item);
            continue;
        }
        for(uint32 i = 0; i < cvt.fmt.length(); ++i)
        {
            if(cvt.fmt[i] == '%')
            {
                if(has_perc)
                {
                    error_log("SD2: CustomVendor: fmt has more then 1 format specifier, skipped");
                    continue;
                }
                has_perc = true;
            }
        }
        vendorMap[cvt.entry].push_back(cvt);
    }
    while (result->NextRow());
    delete result;


    result = SD2Database.Query("SELECT entry,itemOrGroup0,itemOrGroup1,itemOrGroup2,itemOrGroup3,itemOrGroup4,count0,count1,count2,count3,count4,money FROM pe_sd2_customcost"); 
    if(!result)
    {
        error_log("SD2: CustomCost: Table is empty! Can't load.");
        vendorMap.clear();
        return;
    }
    do
    {
        Field *fields = result->Fetch();
        CustomCostTemplate cct;
        cct.entry = fields[0].GetUInt32();

        for(uint8 i = 0; i < 5; ++i)
            cct.itemOrGroup[i] = fields[i + 1].GetUInt32();

        for(uint8 i = 0; i < 5; ++i)
            cct.count[i] = fields[i + 6].GetUInt16();

        cct.money = fields[11].GetUInt32();
        costMap[cct.entry] = cct;
    }
    while (result->NextRow());
    delete result;


    result = SD2Database.Query("SELECT groupId,item FROM pe_sd2_itemgroup"); 
    if(!result)
    {
        error_log("SD2: ItemGroups: Table is empty! Not too bad, loading was successful.");
        return;
    }
    else
    {
        do
        {
            Field *fields = result->Fetch();
            uint32 group, item;
            group = fields[0].GetUInt32();
            item = fields[1].GetUInt32();

            if(!GetItemStore(item))
            {
                error_log("SD2: ItemGroups: Item %u does not exist!", item);
                continue;
            }

            itemgroups[group].items.push_back(item);
        }
        while (result->NextRow());
        delete result;
    }

    result = SD2Database.Query("SELECT groupId,name FROM pe_sd2_itemgroupname"); 
    if(!result)
    {
        error_log("SD2: ItemGroupNames: Table is empty! Not too bad, loading was successful.");
        return;
    }
    else
    {
        do
        {
            Field *fields = result->Fetch();
            uint32 group;
            group = fields[0].GetUInt32();

            ItemGroupMap::iterator it = itemgroups.find(group);
            if(it == itemgroups.end())
            {
                error_log("SD2: ItemGroupNames: Group %u not found, cant add name!", group);
                continue;
            }

            itemgroups[group].name = fields[1].GetCppString();
        }
        while (result->NextRow());
        delete result;
    }

    outstring_log("SD2: CustomVendor script DB loading successful!");
}

// from table script_texts
#define ERROR_STRING1 -1990050

// from table npc_text
#define TXT_LIST 100000
#define TXT_NOT_ENOUGH_MONEY 100001
#define TXT_MISSING_ITEMS 100002
#define TXT_CAN_BUY 100003


void ShitHappened(Creature *pCreature, Player *pPlayer, bool closegossip, bool reload)
{
    if(closegossip)
        pPlayer->CLOSE_GOSSIP_MENU();
    error_log("SD2: CustomVendor: NPC %u caused error!", pCreature->GetEntry());
    pCreature->Yell(ERROR_STRING1, 0, pPlayer ? pPlayer->GetGUID() : 0);
    pCreature->DealDamage(pCreature, pCreature->GetMaxHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
    if(reload)
        LoadCustomVendorData();
}

struct MANGOS_DLL_DECL pe_customvendorAI : public ScriptedAI
{
    std::map<uint64, uint8> buyPhaseMap;
    //*** HANDLED FUNCTION ***
    //This is the constructor, called only once when the creature is first created
    pe_customvendorAI(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
        buyPhaseMap.clear();
    }

    //*** HANDLED FUNCTION ***
    //This is called whenever the core decides we need to evade
    void Reset()
    {
    }

    //*** HANDLED FUNCTION ***
    //Attack Start is called whenever someone hits us.
    void Aggro(Unit* pWho)
    {
    }

    void SetBuyPhase(uint64 guid, uint8 phase)
    {
        buyPhaseMap.erase(guid);
        if(phase != CV_BUYPHASE_NULL)
            buyPhaseMap[guid] = phase;
    }

    uint8 GetBuyPhase(uint64 guid)
    {
        std::map<uint64, uint8>::iterator it = buyPhaseMap.find(guid);
        return (it == buyPhaseMap.end() ? CV_BUYPHASE_NULL : it->second);
    }
};

//This is the GetAI method used by all scripts that involve AI
//It is called every time a new creature using this script is created
CreatureAI* GetAI_pe_customvendor(Creature* pCreature)
{
    return new pe_customvendorAI(pCreature);
}

bool GossipHello_pe_customvendor(Player* pPlayer, Creature* pCreature); // pre-declare, we need it for a hack below

//This function is called when the player clicks an option on the gossip menu
bool GossipSelect_pe_customvendor(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    uint8 result = CV_BUYRESULT_NULL;
    if (uiAction == GOSSIP_ACTION_INFO_DEF + 1) // reload action
    {
        if( ((pe_customvendorAI*)pCreature->AI())->GetBuyPhase(pPlayer->GetGUID()) == CV_BUYPHASE_CONFIRM )
        {
            GossipHello_pe_customvendor(pPlayer, pCreature);
            return true;
        }
        else
        {
            outstring_log("SD2: CustomVendor: %s Requested table reload", pPlayer->GetName());
            LoadCustomVendorData();
            pPlayer->CLOSE_GOSSIP_MENU();
            ((pe_customvendorAI*)pCreature->AI())->SetBuyPhase(pPlayer->GetGUID(), CV_BUYPHASE_NULL);
        }
    }
    else // item requested
    {
        // after the buy this is set to CV_BUYPHASE_NULL again --> buy process complete -> we can exit here
        if(((pe_customvendorAI*)pCreature->AI())->GetBuyPhase(pPlayer->GetGUID()) == CV_BUYPHASE_NULL)
        {
            pPlayer->CLOSE_GOSSIP_MENU();
            return true;
        }

        uint32 ItemID = uiAction - GOSSIP_ACTION_INFO_DEF;
        detail_log("SD2: CustomVendor: %s Requested item %u", pPlayer->GetName(), ItemID);

        // check if item can be bought
        CustomVendorTemplateListMap::iterator it = vendorMap.find(pCreature->GetEntry());
        if(it == vendorMap.end()) // vendor not in table, error
        {
            ShitHappened(pCreature, pPlayer, true, false);
            return true;
        }

        const ItemPrototype *proto = GetItemStore(ItemID); // the item the player should receive
        
        CustomVendorTemplateList& vendorlist = it->second;
        for(CustomVendorTemplateList::iterator entries = vendorlist.begin(); entries != vendorlist.end(); entries++)
        {
            if(entries->item != ItemID)
                continue; // no, we dont want that item, continue searching for the one we really want

            CustomCostTemplateMap::iterator ci = costMap.find(entries->cost_id);
            if(ci == costMap.end()) // cost ID not in table, error
            {
                ShitHappened(pCreature, pPlayer, true, false);
                return true;
            }
            // check gold
            CustomCostTemplate& cost = ci->second;
            if(pPlayer->GetMoney() < cost.money) // not enough money, bad
            {
                result = CV_BUYRESULT_NOT_ENOUGH_MONEY;
            }

            for(uint32 inum = 0; inum < 5; ++inum)
            {
                if(cost.itemOrGroup[inum] > 0 && cost.count[inum])
                {
                    if(!pPlayer->HasItemCount(uint32(cost.itemOrGroup[inum]), cost.count[inum], false))
                    {
                        result = CV_BUYRESULT_MISSING_ITEMS;
                        break;
                    }
                }
                else if(cost.itemOrGroup[inum] < 0 && cost.count[inum])
                {
                    uint32 groupcount = 0;
                    uint32 groupid = abs(cost.itemOrGroup[inum]);
                    ItemGroupMap::iterator igrp = itemgroups.find(groupid);
                    if(igrp == itemgroups.end())
                    {
                        ShitHappened(pCreature, pPlayer, true, false);
                        return true;
                    }
                    ItemGroup& grp = igrp->second;
                    for(std::deque<uint32>::iterator iit = grp.items.begin(); iit != grp.items.end(); iit++)
                    {
                        groupcount += pPlayer->GetItemCount(*iit, false);
                    }
                    if(groupcount < cost.count[inum])
                    {
                        result = CV_BUYRESULT_MISSING_ITEMS;
                        break;
                    }
                }
            }
             
            // remember he item is bought actually the 2nd time the gossip option is clicked - the first time this fun is called the
            // player just clicked on the list to select an item!!
            if(((pe_customvendorAI*)pCreature->AI())->GetBuyPhase(pPlayer->GetGUID()) == CV_BUYPHASE_LIST)
            {
                ((pe_customvendorAI*)pCreature->AI())->SetBuyPhase(pPlayer->GetGUID(), CV_BUYPHASE_CONFIRM);
                // ... show item detail and cost list below
            }
            // if we reached this point and result == CV_BUYRESULT_NULL, everything is ok and we can remove the cost items
            else if(result == CV_BUYRESULT_NULL)
            {
                if(((pe_customvendorAI*)pCreature->AI())->GetBuyPhase(pPlayer->GetGUID()) != CV_BUYPHASE_LIST)
                {
                    // at this point the player confired the buy process - check if item can be added, remove gold and items and give item
                    uint32 noSpaceForCount = 0;
                    uint32 addcount = entries->count;

                    // check space and find places
                    ItemPosCountVec dest;
                    uint8 msg = pPlayer->CanStoreNewItem( NULL_BAG, NULL_SLOT, dest, proto->ItemId, addcount, &noSpaceForCount );
                    if( msg != EQUIP_ERR_OK )                               // convert to possible store amount
                        addcount -= noSpaceForCount;

                    if( addcount == 0 || dest.empty() || noSpaceForCount > 0)                         // can't add any
                    {
                        pPlayer->SendBuyError(BUY_ERR_CANT_CARRY_MORE, pCreature, proto->ItemId, 0);
                        GossipHello_pe_customvendor(pPlayer, pCreature); // HACK: show initial list again
                        return true;
                    }

                    // remove gold
                    pPlayer->ModifyMoney(-int32(cost.money));

                    // remove req. items
                    for(uint32 inum = 0; inum < 5; ++inum)
                    {
                        if(cost.itemOrGroup[inum] > 0)
                        {
                            uint32 destroycount = cost.count[inum];
                            pPlayer->DestroyItemCount(uint32(cost.itemOrGroup[inum]), destroycount, true, false);
                        }
                        else if(cost.itemOrGroup[inum] < 0)
                        {
                            uint32 leftcount = cost.count[inum];
                            uint32 havecount;
                            uint32 groupid = abs(cost.itemOrGroup[inum]);
                            ItemGroupMap::iterator igrp = itemgroups.find(groupid);
                            ItemGroup& grp = itemgroups[groupid];
                            for(std::deque<uint32>::iterator iit = grp.items.begin(); iit != grp.items.end(); iit++)
                            {
                                // destroy max. as many items as are left to buy the item
                                havecount = std::min(pPlayer->GetItemCount(*iit, false), leftcount);
                                if(havecount)
                                {
                                    pPlayer->DestroyItemCount(*iit, havecount, true, false);
                                    leftcount -= havecount;
                                }
                            }
                            if(leftcount) // we should never get here!!! if we do, this means not all items required were removed, means the checks above failed somehow
                            {
                                ShitHappened(pCreature, pPlayer, true, false);
                                return true;
                            }
                        }
                    }

                    // at this point all items and gold were removed - add the requested item
                    Item* newitem = pPlayer->StoreNewItem( dest, proto->ItemId, true, Item::GenerateItemRandomPropertyId(proto->ItemId));
                    if(!newitem)
                    {
                        // if we are here, something is REALLY wrong o.O
                        ShitHappened(pCreature, pPlayer, true, false);
                        return true;
                    }

                    // yay, add item finally
                    pPlayer->SendNewItem(newitem,addcount,true,false);
                    result = CV_BUYRESULT_OK;
                }
            }



            // after buy is complete, reset status
            if(result == CV_BUYRESULT_OK)
            {
                ((pe_customvendorAI*)pCreature->AI())->SetBuyPhase(pPlayer->GetGUID(), CV_BUYPHASE_NULL);
                GossipHello_pe_customvendor(pPlayer, pCreature);
                return true;
            }
            else // else send confirmation gossip
            {
                // build string
                std::stringstream ss;
                if(result != CV_BUYRESULT_NULL)
                    ss << "|cffCC0000";
                else
                    ss << "|cff007700";

                ss << "Buy [" << proto->Name1 << "] for:\n";
                if(cost.money)
                {
                    // some nice formatting here
                    uint32 gold = uint32(cost.money / 10000);
                    uint32 silver = uint32((cost.money % 10000) / 100);
                    uint32 copper = cost.money % 100;
                    ss << " - ";
                    if(gold)
                        ss << gold << "g ";
                    if(silver)
                        ss << silver << "s ";
                    if(copper)
                        ss << copper << "c ";
                    ss << "\n";
                }
                for(uint8 inum = 0; inum < 5; ++inum)
                {
                    if(!cost.count[inum] || !cost.itemOrGroup[inum])
                        continue;
                    std::string tmpnam;
                    if(cost.itemOrGroup[inum] > 0)
                    {
                        const ItemPrototype *tmpproto = GetItemStore(uint32(cost.itemOrGroup[inum]));
                        tmpnam = tmpproto->Name1;
                    }
                    else if(cost.itemOrGroup[inum] < 0)
                    {
                        ItemGroup& igp = itemgroups[uint32(abs(cost.itemOrGroup[inum]))];
                        tmpnam = igp.name;
                    }
                    ss << " - " << cost.count[inum] << "x " << tmpnam << "\n";
                }

                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, ss.str(), GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + proto->ItemId);

                pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "Back to list", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

                uint32 gossiptext;
                switch(result)
                {
                    case CV_BUYRESULT_MISSING_ITEMS:  gossiptext = TXT_MISSING_ITEMS; break;
                    case CV_BUYRESULT_NOT_ENOUGH_MONEY:  gossiptext = TXT_NOT_ENOUGH_MONEY; break;
                    default: gossiptext = TXT_CAN_BUY; break;
                }

                // send gossip
                pPlayer->PlayerTalkClass->SendGossipMenu(gossiptext, pCreature->GetGUID());
            }
            break;
        }       
    }

    return true;
}

//This function is called when the player opens the gossip menu
bool GossipHello_pe_customvendor(Player* pPlayer, Creature* pCreature)
{
    CustomVendorTemplateListMap::iterator it = vendorMap.find(pCreature->GetEntry());
    if(it == vendorMap.end())
    {
        ShitHappened(pCreature, pPlayer, true, true);
        return false;
    }
    CustomVendorTemplateList& vendorlist = it->second;

    ((pe_customvendorAI*)pCreature->AI())->SetBuyPhase(pPlayer->GetGUID(), CV_BUYPHASE_LIST);

    pPlayer->CLOSE_GOSSIP_MENU();
    pPlayer->PlayerTalkClass->ClearMenus();

    if(pPlayer->GetSession()->GetSecurity() >= 5)
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "--Reload data--", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

    uint32 index = 1;
    for(CustomVendorTemplateList::iterator entries = vendorlist.begin(); entries != vendorlist.end(); entries++)
    {
        const ItemPrototype *proto = GetItemStore(entries->item);
        uint8 result = CV_BUYRESULT_NULL;

        // -COPIED from above function-
        CustomCostTemplateMap::iterator ci = costMap.find(entries->cost_id);
        if(ci == costMap.end()) // cost ID not in table, error
        {
            ShitHappened(pCreature, pPlayer, true, false);
            return true;
        }
        // check gold
        CustomCostTemplate& cost = ci->second;
        if(pPlayer->GetMoney() < cost.money) // not enough money, bad
        {
            result = CV_BUYRESULT_NOT_ENOUGH_MONEY;
        }

        for(uint32 inum = 0; inum < 5; ++inum)
        {
            if(cost.itemOrGroup[inum] > 0 && cost.count[inum])
            {
                if(!pPlayer->HasItemCount(uint32(cost.itemOrGroup[inum]), cost.count[inum], false))
                {
                    result = CV_BUYRESULT_MISSING_ITEMS;
                    break;
                }
            }
            else if(cost.itemOrGroup[inum] < 0 && cost.count[inum])
            {
                uint32 groupcount = 0;
                uint32 groupid = abs(cost.itemOrGroup[inum]);
                ItemGroupMap::iterator igrp = itemgroups.find(groupid);
                if(igrp == itemgroups.end())
                {
                    ShitHappened(pCreature, pPlayer, true, false);
                    return true;
                }
                ItemGroup& grp = igrp->second;
                for(std::deque<uint32>::iterator iit = grp.items.begin(); iit != grp.items.end(); iit++)
                {
                    groupcount += pPlayer->GetItemCount(*iit, false);
                }
                if(groupcount < cost.count[inum])
                {
                    result = CV_BUYRESULT_MISSING_ITEMS;
                    break;
                }
            }
        }
        // -END COPIED-

        std::string gossip;
        if(result != CV_BUYRESULT_NULL)
            gossip += "|cffCC0000"; // red if can NOT buy
        else
            gossip += "|cff007700"; // green if can buy

        if(!entries->fmt.empty())
        {
            char tmp[300];
            sprintf(tmp, entries->fmt.c_str(), proto->Name1);
            gossip += tmp;
        }
        else
        {
            gossip += proto->Name1;
        }
        //pPlayer->PlayerTalkClass->GetGossipMenu().AddMenuItem(GOSSIP_ICON_DOT, gossip, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + index, entries->desc, 0, 0);
        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, gossip, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + proto->ItemId);
        ++index;
    }
    pPlayer->PlayerTalkClass->SendGossipMenu(TXT_LIST, pCreature->GetGUID());

    return true;
}

//This is the actual function called only once during InitScripts()
//It must define all handled functions that are to be run in this script
void AddSC_pe_customvendor()
{
    Script* newscript;

    newscript = new Script;
    newscript->Name = "pe_customvendor";
    newscript->GetAI = &GetAI_pe_customvendor;
    newscript->pGossipHello = &GossipHello_pe_customvendor;
    newscript->pGossipSelect = &GossipSelect_pe_customvendor;
    newscript->RegisterSelf();

    LoadCustomVendorData();
}
