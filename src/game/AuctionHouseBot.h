#ifndef AUCTION_HOUSE_BOT_H
#define AUCTION_HOUSE_BOT_H

#include "Common.h"

enum AuctionHouseBotFactions
{
    AUCTION_ALLIANCE = 55,
    AUCTION_HORDE = 29,
    AUCTION_NEUTRAL = 120
};

uint32 AuctionHouseBotNoMail();
void AuctionHouseBot();
void AuctionHouseBotInit();

#endif
