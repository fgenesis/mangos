#include "AuctionHouseBot.h"
#include "Bag.h"
#include "Config/ConfigEnv.h"
#include "Database/DatabaseEnv.h"
#include "Item.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "World.h"
#include "WorldSession.h"
#include <vector>

using namespace std;

//#define NO_VENDOR_ITEMS
#define ONLY_LOOT_ITEMS

static vector<uint32> whiteTradeGoods;
static vector<uint32> greenTradeGoods;
static vector<uint32> blueTradeGoods;
static vector<uint32> purpleTradeGoods;
static vector<uint32> whiteItems;
static vector<uint32> greenItems;
static vector<uint32> blueItems;
static vector<uint32> purpleItems;

static uint32 AHBplayerAccount = 0; 
static uint32 AHBplayerGUID = 0; 
static uint32 noMail = 0; 
static uint32 numAllianceItems = 0;
static uint32 numMinAllianceItems = 0;
static uint32 numHordeItems = 0;
static uint32 numMinHordeItems = 0;
static uint32 numNeutralItems = 0;
static uint32 numMinNeutralItems = 0;
static uint32 minTime = 0;
static uint32 maxTime = 0;
static uint32 percentWhiteTradeGoods = 0;
static uint32 percentGreenTradeGoods = 0;
static uint32 percentBlueTradeGoods = 0;
static uint32 percentPurpleTradeGoods = 0;
static uint32 percentWhiteItems = 0;
static uint32 percentGreenItems = 0;
static uint32 percentBlueItems = 0;
static uint32 percentPurpleItems = 0;
static uint32 minPriceWhite = 0;
static uint32 maxPriceWhite = 0;
static uint32 bidPriceWhite = 0;
static uint32 maxStackWhite = 0;
static uint32 minPriceGreen = 0;
static uint32 maxPriceGreen = 0;
static uint32 bidPriceGreen = 0;
static uint32 maxStackGreen = 0;
static uint32 minPriceBlue = 0;
static uint32 maxPriceBlue = 0;
static uint32 bidPriceBlue = 0;
static uint32 maxStackBlue = 0;
static uint32 minPricePurple = 0;
static uint32 maxPricePurple = 0;
static uint32 bidPricePurple = 0;
static uint32 maxStackPurple = 0;

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
uint32 AuctionHouseBotNoMail()
{
   return noMail != 0 ? AHBplayerGUID : 0;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
static inline uint32 minValue(uint32 a, uint32 b)
{
   return a <= b ? a : b;
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
static void deleteOldAuctions(uint32 ahMapID)
{
   AuctionHouseObject* auctionHouse = objmgr.GetAuctionsMap(ahMapID);

   AuctionHouseObject::AuctionEntryMap::iterator itr;
   itr = auctionHouse->GetAuctionsBegin();

   while (itr != auctionHouse->GetAuctionsEnd())
   {
      AuctionHouseObject::AuctionEntryMap::iterator tmp = itr;
      ++itr;

      if (tmp->second->owner != AHBplayerGUID)
         continue;

      if (tmp->second->bidder != 0)
         continue;

      if (tmp->second->time > sWorld.GetGameTime())
         continue;

      // quietly delete the item and auction...

      Item* item = objmgr.GetAItem(tmp->second->item_guidlow);
      if (item != NULL)
      {
         objmgr.RemoveAItem(tmp->second->item_guidlow);
         item->DeleteFromDB();
         delete item;
      }
      else
      {
         sLog.outString("AuctionHouseBot: "
                        "clearing auction for non-existant item_guidlow (%d)",
                        tmp->second->item_guidlow);
      }

	CharacterDatabase.PExecute("DELETE FROM `auctionhouse` WHERE `id` = '%u'",
                                 tmp->second->Id);
       AuctionEntry* auctionEntry = tmp->second;
       auctionHouse->RemoveAuction(auctionEntry->Id);
       delete auctionEntry;
   }
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
static void addNewAuctions(uint32 ahMapID, uint32 maxAuctions, uint32 minAuctions, Player *AHBplayer)
{
   AuctionHouseObject* auctionHouse = objmgr.GetAuctionsMap(ahMapID);

   if (auctionHouse->Getcount() > minAuctions)
      return;

   uint32 whiteTradeGoodsBin = percentWhiteTradeGoods;
   uint32 greenTradeGoodsBin = percentGreenTradeGoods + whiteTradeGoodsBin;
   uint32 blueTradeGoodsBin = percentBlueTradeGoods + greenTradeGoodsBin;
   uint32 purpleTradeGoodsBin = percentPurpleTradeGoods + blueTradeGoodsBin;
   uint32 whiteItemBin = percentWhiteItems + purpleTradeGoodsBin;
   uint32 greenItemBin = percentGreenItems + whiteItemBin;
   uint32 blueItemBin = percentBlueItems + greenItemBin;
   uint32 purpleItemBin = percentPurpleItems + blueItemBin;

   // only insert 100 at a time, so as not to peg the processor
   for (uint32 count = 0; 
        (count < 100) && (auctionHouse->Getcount() < maxAuctions); 
        count++)
   {
      uint32 itemID = purpleItems[urand(0, purpleItems.size() - 1)];
      uint32 value = urand(1, 100);
      
      if (value <= blueItemBin)
         itemID = blueItems[urand(0, blueItems.size() - 1)];

      if (value <= greenItemBin)
         itemID = greenItems[urand(0, greenItems.size() - 1)];

      if (value <= whiteItemBin)
         itemID = whiteItems[urand(0, whiteItems.size() - 1)];

      if (value <= purpleTradeGoodsBin)
         itemID = purpleTradeGoods[urand(0, purpleTradeGoods.size() - 1)];

      if (value <= blueTradeGoodsBin)
         itemID = blueTradeGoods[urand(0, blueTradeGoods.size() - 1)];

      if (value <= greenTradeGoodsBin)
         itemID = greenTradeGoods[urand(0, greenTradeGoods.size() - 1)];

      if (value <= whiteTradeGoodsBin)
         itemID = whiteTradeGoods[urand(0, whiteTradeGoods.size() - 1)];

      ItemPrototype const* prototype = objmgr.GetItemPrototype(itemID);
      if (prototype == NULL)
      {
         sLog.outString("AuctionHouseBot: Huh?!?! prototype == NULL");
         continue;
      }

      Item* item = Item::CreateItem(itemID, 1, AHBplayer);
	  item->AddToUpdateQueueOf(AHBplayer);
      if (item == NULL)
      {
         sLog.outString("AuctionHouseBot: Item::CreateItem() returned NULL");
         break;
      }

      uint32 randomPropertyId = Item::GenerateItemRandomPropertyId(itemID);
      if (randomPropertyId != 0)
         item->SetItemRandomProperties(randomPropertyId);

      uint32 buyoutPrice = prototype->BuyPrice * item->GetCount();
      uint32 bidPrice = 0;
      uint32 stackCount = urand(1, item->GetMaxStackCount());

      switch (prototype->Quality)
      {
         case 1:
            if (maxStackWhite != 0)
            {
               stackCount = urand(1, minValue(item->GetMaxStackCount(), 
                                              maxStackWhite));
            }

            buyoutPrice *= urand(minPriceWhite, maxPriceWhite) * stackCount;
            buyoutPrice /= 100;
            bidPrice = buyoutPrice * bidPriceWhite;
            bidPrice /= 100;

            break;

         case 2:
            if (maxStackGreen != 0)
            {
               stackCount = urand(1, minValue(item->GetMaxStackCount(), 
                                              maxStackGreen));
            }

            buyoutPrice *= urand(minPriceGreen, maxPriceGreen) * stackCount;
            buyoutPrice /= 100;
            bidPrice = buyoutPrice * bidPriceGreen;
            bidPrice /= 100;

            break;

         case 3:
            if (maxStackBlue != 0)
            {
               stackCount = urand(1, minValue(item->GetMaxStackCount(), 
                                              maxStackBlue));
            }

            buyoutPrice *= urand(minPriceBlue, maxPriceBlue) * stackCount;
            buyoutPrice /= 100;
            bidPrice = buyoutPrice * bidPriceBlue;
            bidPrice /= 100;

            break;

         case 4:
            if (maxStackPurple != 0)
            {
               stackCount = urand(1, minValue(item->GetMaxStackCount(), 
                                              maxStackPurple));
            }

            buyoutPrice *= urand(minPricePurple, maxPricePurple) * stackCount;
            buyoutPrice /= 100;
            bidPrice = buyoutPrice * bidPricePurple;
            bidPrice /= 100;

            break;
      }

      item->SetCount(stackCount);

      AuctionEntry* auctionEntry = new AuctionEntry;
      auctionEntry->Id = objmgr.GenerateAuctionID();
      auctionEntry->auctioneer = 0;
      auctionEntry->item_guidlow = item->GetGUIDLow();
      auctionEntry->item_template = item->GetEntry();
      auctionEntry->owner = AHBplayer->GetGUIDLow();
      auctionEntry->startbid = bidPrice;
      auctionEntry->buyout = buyoutPrice;
      auctionEntry->bidder = 0;
      auctionEntry->bid = 0;
      auctionEntry->deposit = 0;
      auctionEntry->location = ahMapID;
      auctionEntry->time = (time_t) (urand(minTime, maxTime) * 60 * 60 + 
                                     time(NULL));
      
      item->SaveToDB();
      item->RemoveFromUpdateQueueOf(AHBplayer);
      objmgr.AddAItem(item);
      auctionHouse->AddAuction(auctionEntry);

      CharacterDatabase.PExecute("INSERT INTO `auctionhouse` (`id`,"
                                 "`auctioneerguid`,`itemguid`,`item_template`,"
                                 "`itemowner`,`buyoutprice`,`time`,`buyguid`,"
                                 "`lastbid`,`startbid`,`deposit`,`location`) "
                                 "VALUES ('%u', '%u', '%u', '%u', '%u', '%u', "
                                 "'" I64FMTD "', '%u', '%u', '%u', '%u', '%u')",
                                 auctionEntry->Id, 
                                 auctionEntry->auctioneer,
                                 auctionEntry->item_guidlow, 
                                 auctionEntry->item_template, 
                                 auctionEntry->owner, 
                                 auctionEntry->buyout, 
                                 (uint64) auctionEntry->time, 
                                 auctionEntry->bidder, 
                                 auctionEntry->bid, 
                                 auctionEntry->startbid, 
                                 auctionEntry->deposit, 
                                 auctionEntry->location);
   }
}

///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void AuctionHouseBot()
{
   if ((AHBplayerAccount == 0) || (AHBplayerGUID == 0))
      return;

   WorldSession _session(AHBplayerAccount, NULL, 0, true, 0, LOCALE_enUS);
   Player _AHBplayer(&_session);
   _AHBplayer.MinimalLoadFromDB(NULL, AHBplayerGUID);
   ObjectAccessor::Instance().AddObject(&_AHBplayer);

   deleteOldAuctions(2);
   deleteOldAuctions(6);
   deleteOldAuctions(7);

   addNewAuctions(2, numAllianceItems, numMinAllianceItems, &_AHBplayer);
   addNewAuctions(6, numHordeItems, numMinHordeItems, &_AHBplayer);
   addNewAuctions(7, numNeutralItems, numMinNeutralItems, &_AHBplayer);
   
   ObjectAccessor::Instance().RemoveObject(&_AHBplayer);
}
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void AuctionHouseBotInit()
{
   AHBplayerAccount = sConfig.GetIntDefault("AuctionHouseBot.Account", 0);
   AHBplayerGUID = sConfig.GetIntDefault("AuctionHouseBot.GUID", 0);
                         
   if ((AHBplayerAccount == 0) || (AHBplayerGUID == 0))
   {
      sLog.outString("AuctionHouseBot disabled");
      return;            
   }

   noMail = sConfig.GetIntDefault("AuctionHouseBot.NoMail", 0);

   numAllianceItems = sConfig.GetIntDefault("AuctionHouseBot.AllianceItems", 0);
   numMinAllianceItems = sConfig.GetIntDefault("AuctionHouseBot.MinAllianceItems", -1);
   numHordeItems = sConfig.GetIntDefault("AuctionHouseBot.HordeItems", 0);
   numMinHordeItems = sConfig.GetIntDefault("AuctionHouseBot.MinHordeItems", -1);
   numNeutralItems = sConfig.GetIntDefault("AuctionHouseBot.NeutralItems", 0);
   numMinNeutralItems = sConfig.GetIntDefault("AuctionHouseBot.MinNeutralItems", -1);

   if (numMinAllianceItems < 0)
	   numMinAllianceItems = numAllianceItems;

   if (numMinHordeItems  < 0)
	   numMinHordeItems = numHordeItems;

   if (numMinNeutralItems  < 0)
	   numMinNeutralItems = numNeutralItems;

   if (numMinAllianceItems > numAllianceItems)
	   numMinAllianceItems = numAllianceItems;

   if (numMinHordeItems > numHordeItems)
	   numMinHordeItems = numHordeItems;

   if (numMinNeutralItems > numNeutralItems)
	   numMinNeutralItems = numNeutralItems;

   minTime = sConfig.GetIntDefault("AuctionHouseBot.MinTime", 8);
   maxTime = sConfig.GetIntDefault("AuctionHouseBot.MaxTime", 24);

   if (minTime < 1)
      minTime = 1;

   if (maxTime > 24)
      maxTime = 24;

   if (minTime > maxTime)
      minTime = maxTime;

   percentWhiteTradeGoods = sConfig.GetIntDefault("AuctionHouseBot."
                                                  "PercentWhiteTradeGoods",
                                                  27);
   percentGreenTradeGoods = sConfig.GetIntDefault("AuctionHouseBot."
                                                  "PercentGreenTradeGoods",
                                                  12);
   percentBlueTradeGoods = sConfig.GetIntDefault("AuctionHouseBot."
                                                  "PercentBlueTradeGoods",
                                                  10);
   percentPurpleTradeGoods = sConfig.GetIntDefault("AuctionHouseBot."
                                                  "PercentPurpleTradeGoods",
                                                  1);
   percentWhiteItems = sConfig.GetIntDefault("AuctionHouseBot."
                                             "PercentWhiteItems",
                                             10);
   percentGreenItems = sConfig.GetIntDefault("AuctionHouseBot."
                                             "PercentGreenItems",
                                             30);
   percentBlueItems = sConfig.GetIntDefault("AuctionHouseBot."
                                            "PercentBlueItems",
                                            8);
   percentPurpleItems = sConfig.GetIntDefault("AuctionHouseBot."
                                              "PercentPurpleItems",
                                              2);

   uint32 totalPercent = percentWhiteTradeGoods + percentGreenTradeGoods +
                         percentBlueTradeGoods + percentPurpleTradeGoods +
                         percentWhiteItems + percentGreenItems + 
                         percentBlueItems + percentPurpleItems;

   if (totalPercent == 0)
   {
      numAllianceItems = 0;
      numHordeItems = 0;
      numNeutralItems = 0;
   }
   else if (totalPercent != 100)
   {
      double scale = (double) 100 / (double) totalPercent;

      percentPurpleItems = (uint32) (scale * (double) percentPurpleItems);
      percentBlueItems = (uint32) (scale * (double) percentBlueItems);
      percentGreenItems = (uint32) (scale * (double) percentGreenItems);
      percentWhiteItems = (uint32) (scale * (double) percentWhiteItems);
      percentPurpleTradeGoods = (uint32) (scale * 
                                          (double) percentPurpleTradeGoods);
      percentBlueTradeGoods = (uint32) (scale * 
                                        (double) percentBlueTradeGoods);
      percentGreenTradeGoods = (uint32) (scale * 
                                         (double) percentGreenTradeGoods);
      percentWhiteTradeGoods = 100 - 
                               percentGreenTradeGoods - 
                               percentBlueTradeGoods -
                               percentPurpleTradeGoods -
                               percentWhiteItems -
                               percentGreenItems -
                               percentBlueItems -
                               percentPurpleItems;

      sLog.outString("AuctionHouseBot:");
      sLog.outString("sum of item percentages not equal to 100, adjusting...");
      sLog.outString("AuctionHouseBot.PercentWhiteTradeGoods = %d",
                     percentWhiteTradeGoods);
      sLog.outString("AuctionHouseBot.PercentGreenTradeGoods = %d",
                     percentGreenTradeGoods);
      sLog.outString("AuctionHouseBot.PercentBlueTradeGoods = %d",
                     percentBlueTradeGoods);
      sLog.outString("AuctionHouseBot.PercentPurpleTradeGoods = %d",
                     percentPurpleTradeGoods);
      sLog.outString("AuctionHouseBot.PercentWhiteItems = %d",
                     percentWhiteItems);
      sLog.outString("AuctionHouseBot.PercentGreenItems = %d",
                     percentGreenItems);
      sLog.outString("AuctionHouseBot.PercentBlueItems = %d",
                     percentBlueItems);
      sLog.outString("AuctionHouseBot.PercentPurpleItems = %d",
                     percentPurpleItems);
   }

   minPriceWhite = sConfig.GetIntDefault("AuctionHouseBot.MinPriceWhite",
                                         150);
   maxPriceWhite = sConfig.GetIntDefault("AuctionHouseBot.MaxPriceWhite",
                                         250);

   if (minPriceWhite == 0)
      minPriceWhite = 1;

   if (maxPriceWhite == 0)
      maxPriceWhite = 1;

   if (minPriceWhite > maxPriceWhite)
      minPriceWhite = maxPriceWhite;

   bidPriceWhite = sConfig.GetIntDefault("AuctionHouseBot.BidPriceWhite",
                                         100);
   if (bidPriceWhite > 100)
      bidPriceWhite = 100;

   maxStackWhite = sConfig.GetIntDefault("AuctionHouseBot.MaxStackWhite", 0);

   minPriceGreen = sConfig.GetIntDefault("AuctionHouseBot.MinPriceGreen",
                                         200);
   maxPriceGreen = sConfig.GetIntDefault("AuctionHouseBot.MaxPriceGreen",
                                         300);

   if (minPriceGreen == 0)
      minPriceGreen = 1;

   if (maxPriceGreen == 0)
      maxPriceGreen = 1;

   if (minPriceGreen > maxPriceGreen)
      minPriceGreen = maxPriceGreen;

   bidPriceGreen = sConfig.GetIntDefault("AuctionHouseBot.BidPriceGreen",
                                         100);
   if (bidPriceGreen > 100)
      bidPriceGreen = 100;

   maxStackGreen = sConfig.GetIntDefault("AuctionHouseBot.MaxStackGreen", 0);

   minPriceBlue = sConfig.GetIntDefault("AuctionHouseBot.MinPriceBlue",
                                        250);
   maxPriceBlue = sConfig.GetIntDefault("AuctionHouseBot.MaxPriceBlue",
                                        350);

   if (minPriceBlue == 0)
      minPriceBlue = 1;

   if (maxPriceBlue == 0)
      maxPriceBlue = 1;

   if (minPriceBlue > maxPriceBlue)
      minPriceBlue = maxPriceBlue;

   bidPriceBlue = sConfig.GetIntDefault("AuctionHouseBot.BidPriceBlue",
                                        100);
   if (bidPriceBlue > 100)
      bidPriceBlue = 100;

   maxStackBlue = sConfig.GetIntDefault("AuctionHouseBot.MaxStackBlue", 0);

   minPricePurple = sConfig.GetIntDefault("AuctionHouseBot.MinPricePurple",
                                          300);
   maxPricePurple = sConfig.GetIntDefault("AuctionHouseBot.MaxPricePurple",
                                          450);

   if (minPricePurple == 0)
      minPricePurple = 1;

   if (maxPricePurple == 0)
      maxPricePurple = 1;

   if (minPricePurple > maxPricePurple)
      minPricePurple = maxPricePurple;

   bidPricePurple = sConfig.GetIntDefault("AuctionHouseBot.BidPricePurple",
                                          100);
   if (bidPricePurple > 100)
      bidPricePurple = 100;

   maxStackPurple = sConfig.GetIntDefault("AuctionHouseBot.MaxStackPurple", 0);

   QueryResult* results = (QueryResult*) NULL;

#ifdef NO_VENDOR_ITEMS
   vector<uint32> npcItems;

   char npcQuery[] = "SELECT `item` FROM `npc_vendor`";

   results = WorldDatabase.PQuery(npcQuery);
   if (results != NULL)
   {
      do
      {
         Field* fields = results->Fetch();
         npcItems.push_back(fields[0].GetUInt32());

      } while (results->NextRow());

      delete results;
   }
   else
   {
      sLog.outString("AuctionHouseBot: \"%s\" failed", npcQuery);
   }
#endif

#ifdef ONLY_LOOT_ITEMS
   vector<uint32> lootItems;

   char lootQuery[] = "SELECT `item` FROM `creature_loot_template` UNION "
                      "SELECT `item` FROM `disenchant_loot_template` UNION "
                      "SELECT `item` FROM `fishing_loot_template` UNION "
                      "SELECT `item` FROM `gameobject_loot_template` UNION "
                      "SELECT `item` FROM `item_loot_template` UNION "
                      "SELECT `item` FROM `pickpocketing_loot_template` UNION "
                      "SELECT `item` FROM `prospecting_loot_template` UNION "
                      "SELECT `item` FROM `skinning_loot_template`";

   results = WorldDatabase.PQuery(lootQuery);
   if (results != NULL)
   {
      do
      {
         Field* fields = results->Fetch();
         lootItems.push_back(fields[0].GetUInt32());
   
      } while (results->NextRow());
   
      delete results;
   }
   else
   {
      sLog.outString("AuctionHouseBot: \"%s\" failed", lootQuery);
   }
#endif

   for (uint32 itemID = 0; itemID < sItemStorage.MaxEntry; itemID++)
   {
      ItemPrototype const* prototype = objmgr.GetItemPrototype(itemID);

      if (prototype == NULL)
         continue;

      if ((prototype->Bonding != NO_BIND) &&
          (prototype->Bonding != BIND_WHEN_EQUIPED))
      {
         continue;
      }

      if (prototype->BuyPrice == 0)
         continue;
         
      if ((prototype->Quality < 1) || (prototype->Quality > 4))
         continue;

#ifdef NO_VENDOR_ITEMS
      bool isVendorItem = false;

      for (unsigned int i = 0; (i < npcItems.size()) && (!isVendorItem); i++)
      {         
         if (itemID == npcItems[i])
            isVendorItem = true;
      }

      if (isVendorItem)
         continue;
#endif

#ifdef ONLY_LOOT_ITEMS
      bool isLootItem = false;

      for (unsigned int i = 0; (i < lootItems.size()) && (!isLootItem); i++)
      {         
         if (itemID == lootItems[i])
            isLootItem = true;
      }
      
      if (!isLootItem)
         continue;
#endif

      switch (prototype->Quality)
      {
         case 1:
            if (prototype->Class == ITEM_CLASS_TRADE_GOODS)
               whiteTradeGoods.push_back(itemID);
            else
               whiteItems.push_back(itemID);
            break;
   
         case 2:
            if (prototype->Class == ITEM_CLASS_TRADE_GOODS)
               greenTradeGoods.push_back(itemID);
            else
               greenItems.push_back(itemID);
            break;
   
         case 3:
            if (prototype->Class == ITEM_CLASS_TRADE_GOODS)
               blueTradeGoods.push_back(itemID);
            else
               blueItems.push_back(itemID);
            break;
   
         case 4:
            if (prototype->Class == ITEM_CLASS_TRADE_GOODS)
               purpleTradeGoods.push_back(itemID);
            else
               purpleItems.push_back(itemID);
            break;
      }
   }
   
   if ((whiteTradeGoods.size() == 0) ||
       (greenTradeGoods.size() == 0) ||
       (blueTradeGoods.size() == 0) ||
       (purpleTradeGoods.size() == 0) ||
       (whiteItems.size() == 0) ||
       (greenItems.size() == 0) ||
       (blueItems.size() == 0) ||
       (purpleItems.size() == 0))
   {
      sLog.outString("AuctionHouseBot: not loaded DB error?");
      AHBplayerAccount = 0;
      AHBplayerGUID = 0;
      return;
   }
  
   sLog.outString("AuctionHouseBot:");
   sLog.outString("loaded %d white trade goods", whiteTradeGoods.size());
   sLog.outString("loaded %d green trade goods", greenTradeGoods.size());
   sLog.outString("loaded %d blue trade goods", blueTradeGoods.size());
   sLog.outString("loaded %d purple trade goods", purpleTradeGoods.size());
   sLog.outString("loaded %d white items", whiteItems.size());
   sLog.outString("loaded %d green items", greenItems.size());
   sLog.outString("loaded %d blue items", blueItems.size());
   sLog.outString("loaded %d purple items", purpleItems.size());
   sLog.outString("AuctionHouseBot v5.8.6562 by |Paradox| (original by ChrisK)  has been loaded.");

}
