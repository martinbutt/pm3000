// Domain/gameplay helpers for transfers and club utilities.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "input.h"
#include "pm3_data.h"
#include "pm3_defs.hh"

struct OfferResponse {
    bool accepted;
    char message[75];
};

ClubRecord& getClub(int idx);
PlayerRecord& getPlayer(int16_t idx);
char determinePlayerType(PlayerRecord &player);
uint8_t determinePlayerRating(PlayerRecord &player);
char determineValuationRole(const PlayerRecord &player);
int determinePlayerPrice(const PlayerRecord &player, const ClubRecord &club, int squadSlot);
int determinePlayerImportance(const PlayerRecord &player, const ClubRecord &club);
std::vector<club_player> findFreePlayers();
std::vector<club_player> getMyPlayers(int player);
void levelAggression();
void changeClub(int16_t newClubIdx, const char* gamePath, int player=0);

namespace game_utils {

OfferResponse assessOffer(const club_player &playerInfo, int offerAmount, int currentGame);
void beginOffer(InputHandler &input, char *footer, size_t footerSize, const club_player &playerInfo, int currentGame);
int16_t findPlayerIndex(const PlayerRecord &player);
int findClubIndexForPlayer(int16_t playerIdx);
int findEmptySlot(ClubRecord &club);
void completeTransfer(int16_t playerIdx, int fromClubIdx, int toClubIdx, int offerAmount);
void convertPlayerToCoach(struct gamea::ManagerRecord &manager, ClubRecord &club, int8_t clubPlayerIdx, char *footer,
                          size_t footerSize);
std::string formatCurrency(int amount);

} // namespace game_utils
