#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

#include "game_utils.h"
#include "pm3_data.h"

int main() {
    std::memset(&playerData, 0, sizeof(playerData));
    std::memset(&clubData, 0, sizeof(clubData));
    std::memset(&gameData, 0, sizeof(gameData));

    // Basic type/rating/valuation helpers.
    PlayerRecord p{};
    p.hn = 90; p.tk = 20; p.ps = 10; p.sh = 5;
    if (determinePlayerType(p) != 'G') return 1;
    if (determinePlayerRating(p) != 90) return 1;
    p.tk = 95; if (determinePlayerType(p) != 'D') return 1;
    p.ps = 96; if (determinePlayerType(p) != 'M') return 1;
    p.sh = 97; if (determinePlayerType(p) != 'A') return 1;
    PlayerRecord val{}; val.hn=50; val.tk=60; val.ps=70; val.sh=80; val.hd=65; val.cr=55; val.aggr=5;
    if (determineValuationRole(val) != 'A') return 1;

    // Importance and pricing: build a tiny club.
    ClubRecord club{}; club.league = 1;
    for (int i = 0; i < 24; ++i) {
        club.player_index[i] = -1;
    }
    PlayerRecord star{}; star.hn=90; star.tk=80; star.ps=70; star.sh=75; star.wage=1000;
    PlayerRecord bench{}; bench.hn=60; bench.tk=55; bench.ps=50; bench.sh=50; bench.wage=300;
    playerData.player[0] = star;
    playerData.player[1] = bench;
    club.player_index[0] = 0;
    club.player_index[1] = 1;
    int importance = determinePlayerImportance(playerData.player[0], club);
    if (importance < 3) return 1;
    int priceStarter = determinePlayerPrice(playerData.player[0], club, 0);
    int priceBench = determinePlayerPrice(playerData.player[1], club, 12);
    if (priceStarter <= priceBench) return 1;

    // findPlayerIndex and findEmptySlot.
    int16_t idx = game_utils::findPlayerIndex(playerData.player[0]);
    if (idx != 0) return 1;
    if (game_utils::findEmptySlot(club) != 2) return 1; // first open slot after two starters

    // findFreePlayers respects contract/league.
    std::memset(&clubData, 0, sizeof(clubData));
    for (int clubIdx = 0; clubIdx < 114; ++clubIdx) {
        for (int slot = 0; slot < 24; ++slot) {
            clubData.club[clubIdx].player_index[slot] = -1;
        }
    }
    clubData.club[0].league = 2;
    clubData.club[0].player_index[0] = 0;
    playerData.player[0].contract = 0;
    auto freeList = findFreePlayers();
    if (freeList.size() != 1) return 1;

    // levelAggression sets all to 5.
    playerData.player[0].aggr = 9;
    playerData.player[3920].aggr = 2;
    levelAggression();
    if (playerData.player[0].aggr != 5 || playerData.player[3920].aggr != 5) return 1;

    return 0;
}
