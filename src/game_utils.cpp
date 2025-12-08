// Domain/gameplay helpers for transfers and club utilities.
#include "game_utils.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

#include "pm3_data.h"
#include "io.h"

static double computeRoleRating(char role, const PlayerRecord &p) {
    auto clamp = [](double v) { return std::clamp(v, 0.0, 99.0); };
    double rating = 0.0;
    switch (role) {
        case 'G': { // Keeper: handling heavy, heading/control secondary, some tackling/passing
            rating = 0.50 * p.hn +
                     0.15 * p.hd +
                     0.15 * p.cr +
                     0.10 * p.tk +
                     0.05 * p.ps +
                     0.05 * p.sh;
            break;
        }
        case 'D': { // Defender: tackling first, then passing, heading, control; aggression helps slightly
            rating = 0.40 * p.tk +
                     0.15 * p.ps +
                     0.15 * p.hd +
                     0.15 * p.cr +
                     0.05 * p.sh +
                     0.10 * p.aggr;
            break;
        }
        case 'M': { // Midfielder: passing primary, then tackling/shooting, control/heading, aggression minor
            rating = 0.35 * p.ps +
                     0.20 * p.tk +
                     0.15 * p.sh +
                     0.10 * p.hd +
                     0.10 * p.cr +
                     0.10 * p.aggr;
            break;
        }
        default: { // Attacker: shooting primary, then passing/heading, control, minor tackling/aggression
            rating = 0.35 * p.sh +
                     0.20 * p.ps +
                     0.15 * p.hd +
                     0.10 * p.cr +
                     0.10 * p.tk +
                     0.10 * p.aggr;
            break;
        }
    }
    return clamp(rating);
}

char determinePlayerType(PlayerRecord &p) {
    if (p.hn > p.tk && p.hn > p.ps && p.hn > p.sh) {
        return 'G';
    } else if (p.tk > p.hn && p.tk > p.ps && p.tk > p.sh) {
        return 'D';
    } else if (p.ps > p.hn && p.ps > p.tk && p.ps > p.sh) {
        return 'M';
    }

    return 'A';
}

uint8_t determinePlayerRating(PlayerRecord &p) {
    if (p.hn > p.tk && p.hn > p.ps && p.hn > p.sh) {
        return p.hn;
    } else if (p.tk > p.hn && p.tk > p.ps && p.tk > p.sh) {
        return p.tk;
    } else if (p.ps > p.hn && p.ps > p.tk && p.ps > p.sh) {
        return p.ps;
    }

    return p.sh;
}

char determineValuationRole(const PlayerRecord &p) {
    double gk = computeRoleRating('G', p);
    double def = computeRoleRating('D', p);
    double mid = computeRoleRating('M', p);
    double att = computeRoleRating('A', p);

    if (gk >= def && gk >= mid && gk >= att) {
        return 'G';
    } else if (def >= mid && def >= att) {
        return 'D';
    } else if (mid >= att) {
        return 'M';
    }
    return 'A';
}

static int normalizedLeagueTier(const ClubRecord &club) {
    // Handles both 0-based tier (0..4) and legacy hex division codes.
    switch (club.league) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            return club.league;
        default:
            break;
    }

    for (size_t i = 0; i < divisionHex.size(); ++i) {
        if (club.league == divisionHex[i]) {
            return static_cast<int>(i);
        }
    }

    return 4; // bottom tier fallback
}

int determinePlayerPrice(const PlayerRecord &player, const ClubRecord &club, int squadSlot) {
    char valuationRole = determineValuationRole(player);
    int rating = static_cast<int>(std::lround(computeRoleRating(valuationRole, player)));
    int age = player.age;

    double ageFactor = 1.0;
    if (age < 24) {
        ageFactor = 1.2;
    } else if (age < 28) {
        ageFactor = 1.1;
    } else if (age >= 35) {
        ageFactor = 0.7;
    } else if (age >= 33) {
        ageFactor = 0.8;
    } else if (age >= 30) {
        ageFactor = 0.9;
    }

    double contractFactor = 0.9 + (player.contract * 0.05);
    int wageInfluence = std::max<int>(player.wage, 200) * 30;

    double baseValue = static_cast<double>(rating) * static_cast<double>(rating) * 1200.0;

    // Importance based on relative quality in the squad.
    int importance = determinePlayerImportance(player, club);
    double importanceFactor = 1.0;
    switch (importance) {
        case 4: importanceFactor = 1.6; break;
        case 3: importanceFactor = 1.35; break;
        case 2: importanceFactor = 1.15; break;
        default: importanceFactor = 1.0; break;
    }

    // Squad slot: 0-10 = starters, 11-13 bench, 14+ reserves.
    double squadFactor = 0.35;
    if (squadSlot >= 0 && squadSlot < 11) {
        squadFactor = 1.0;
    } else if (squadSlot >= 11 && squadSlot < 14) {
        squadFactor = 0.55;
    }

    // Division/league scaling (Premier -> Conference).
    int tier = normalizedLeagueTier(club);

    double divisionFactor;
    switch (tier) {
        case 0: divisionFactor = 1.0; break;
        case 1: divisionFactor = 0.2; break;    // next league ~20%
        case 2: divisionFactor = 0.12; break;    // half again (slightly reduced)
        case 3: divisionFactor = 0.075; break;  // 75% of previous
        default: divisionFactor = 0.0375; break; // half again
    }

    double roleMultiplier = 1.0;
    switch (valuationRole) {
        case 'D': roleMultiplier = 0.4; break;
        case 'M': roleMultiplier = 0.8; break;
        case 'A': roleMultiplier = 1.05; break;
        default: roleMultiplier = 1.0; break;
    }
    if (valuationRole == 'D' && tier >= 1) {
        roleMultiplier *= (1.0 + 0.2 * std::min<int>(tier, 3));
    }

    double scaleFactor = 1.6; // Normalizes toward realistic modern-era transfer fees.
    double value = (baseValue * importanceFactor * squadFactor * divisionFactor + wageInfluence) * ageFactor * contractFactor * scaleFactor * roleMultiplier;

    double leagueFloor = 0.0;
    switch (tier) {
        case 0: leagueFloor = 500000; break;
        case 1: leagueFloor = 250000; break;
        case 2: leagueFloor = 150000; break;
        default: leagueFloor = 100000; break;
    }

    value = std::max(value, leagueFloor);
    return static_cast<int>(std::max<double>(value, wageInfluence));
}

int determinePlayerImportance(const PlayerRecord &player, const ClubRecord &club) {
    PlayerRecord &mutablePlayer = const_cast<PlayerRecord &>(player);
    char playerType = determinePlayerType(mutablePlayer);
    int rating = determinePlayerRating(mutablePlayer);

    int bestOverall = 0;
    int bestInRole = 0;
    int squadSize = 0;

    for (int slot = 0; slot < 24; ++slot) {
        int16_t idx = club.player_index[slot];
        if (idx == -1) {
            continue;
        }
        ++squadSize;

        PlayerRecord &clubPlayer = getPlayer(idx);
        int clubRating = determinePlayerRating(clubPlayer);
        bestOverall = std::max(bestOverall, clubRating);

        if (determinePlayerType(clubPlayer) == playerType) {
            bestInRole = std::max(bestInRole, clubRating);
        }
    }

    int importance = 1;
    if (rating >= bestOverall - 2) {
        importance = 3;
    } else if (rating >= bestOverall - 6) {
        importance = 2;
    }

    if (bestInRole <= rating) {
        importance = std::max(importance, 4);
    }

    if (squadSize < 16) {
        importance = std::max(importance, 2);
    }

    return importance;
}

void changeClub(int16_t newClubIdx, const char* gamePath, int player) {
    gamea::ManagerRecord &manager = gameData.manager[player];
    int oldClubIdx = manager.club_idx;
    manager.club_idx = newClubIdx;

    auto fillSafety = [&](int value) {
        for (auto &rating : manager.stadium.safety_rating) {
            rating = value;
        }
    };

    if (manager.club_idx >= 0 && manager.club_idx <= 21) {
        manager.division = 0;
        manager.stadium.ground_facilities.level = 3;
        manager.stadium.supporters_club.level = 3;
        manager.stadium.flood_lights.level = 2;
        manager.stadium.scoreboard.level = 3;
        manager.stadium.undersoil_heating.level = 1;
        manager.stadium.changing_rooms.level = 2;
        manager.stadium.gymnasium.level = 3;
        manager.stadium.car_park.level = 2;

        fillSafety(4);

        for (auto &capacity : manager.stadium.capacity) {
            capacity.seating = 10000;
            capacity.terraces = 0;
        }
        for (auto &conversion : manager.stadium.conversion) {
            conversion.level = 2;
        }
        for (auto &covering : manager.stadium.area_covering) {
            covering.level = 3;
        }

        manager.price.league_match_seating = 15;
        manager.price.league_match_terrace = 13;
        manager.price.cup_match_seating = 18;
        manager.price.cup_match_terrace = 15;
    } else if (manager.club_idx >= 22 && manager.club_idx <= 45) {
        manager.division = 1;
        manager.stadium.ground_facilities.level = 2;
        manager.stadium.supporters_club.level = 2;
        manager.stadium.flood_lights.level = 2;
        manager.stadium.scoreboard.level = 2;
        manager.stadium.undersoil_heating.level = 1;
        manager.stadium.changing_rooms.level = 2;
        manager.stadium.gymnasium.level = 2;
        manager.stadium.car_park.level = 2;

        fillSafety(3);

        for (auto &capacity : manager.stadium.capacity) {
            capacity.seating = 5000;
            capacity.terraces = 0;
        }
        for (auto &conversion : manager.stadium.conversion) {
            conversion.level = 2;
        }
        for (auto &covering : manager.stadium.area_covering) {
            covering.level = 2;
        }

        manager.price.league_match_seating = 13;
        manager.price.league_match_terrace = 11;
        manager.price.cup_match_seating = 16;
        manager.price.cup_match_terrace = 13;
    } else if (manager.club_idx >= 46 && manager.club_idx <= 69) {
        manager.division = 2;
        manager.stadium.ground_facilities.level = 2;
        manager.stadium.supporters_club.level = 2;
        manager.stadium.flood_lights.level = 1;
        manager.stadium.scoreboard.level = 2;
        manager.stadium.undersoil_heating.level = 0;
        manager.stadium.changing_rooms.level = 1;
        manager.stadium.gymnasium.level = 2;
        manager.stadium.car_park.level = 1;

        fillSafety(2);

        for (auto &capacity : manager.stadium.capacity) {
            capacity.seating = 2500;
            capacity.terraces = 0;
        }
        for (auto &conversion : manager.stadium.conversion) {
            conversion.level = 1;
        }
        for (auto &covering : manager.stadium.area_covering) {
            covering.level = 1;
        }

        manager.price.league_match_seating = 11;
        manager.price.league_match_terrace = 9;
        manager.price.cup_match_seating = 14;
        manager.price.cup_match_terrace = 11;
    } else if (manager.club_idx >= 70 && manager.club_idx <= 91) {
        manager.division = 3;
        manager.stadium.ground_facilities.level = 1;
        manager.stadium.supporters_club.level = 1;
        manager.stadium.flood_lights.level = 1;
        manager.stadium.scoreboard.level = 1;
        manager.stadium.undersoil_heating.level = 0;
        manager.stadium.changing_rooms.level = 1;
        manager.stadium.gymnasium.level = 1;
        manager.stadium.car_park.level = 1;

        fillSafety(1);

        for (auto &capacity : manager.stadium.capacity) {
            capacity.seating = 1000;
            capacity.terraces = 1;
        }
        for (auto &conversion : manager.stadium.conversion) {
            conversion.level = 0;
        }
        for (auto &covering : manager.stadium.area_covering) {
            covering.level = 0;
        }

        manager.price.league_match_seating = 9;
        manager.price.league_match_terrace = 7;
        manager.price.cup_match_seating = 12;
        manager.price.cup_match_terrace = 9;
    } else if (manager.club_idx >= 92 && manager.club_idx <= 113) {
        manager.division = 4;
        manager.stadium.ground_facilities.level = 0;
        manager.stadium.supporters_club.level = 0;
        manager.stadium.flood_lights.level = 0;
        manager.stadium.scoreboard.level = 0;
        manager.stadium.undersoil_heating.level = 0;
        manager.stadium.changing_rooms.level = 0;
        manager.stadium.gymnasium.level = 0;
        manager.stadium.car_park.level = 0;

        fillSafety(0);

        for (auto &capacity : manager.stadium.capacity) {
            capacity.seating = 500;
            capacity.terraces = 1;
        }
        for (auto &conversion : manager.stadium.conversion) {
            conversion.level = 0;
        }
        for (auto &covering : manager.stadium.area_covering) {
            covering.level = 0;
        }

        manager.price.league_match_seating = 7;
        manager.price.league_match_terrace = 5;
        manager.price.cup_match_seating = 10;
        manager.price.cup_match_terrace = 7;
    } else {
        std::fprintf(stderr, "Invalid Club index (%i)\n", newClubIdx);
        std::exit(EXIT_FAILURE);
    }

    clubData.club[newClubIdx].player_image = clubData.club[oldClubIdx].player_image;
    std::strncpy(clubData.club[newClubIdx].manager, clubData.club[oldClubIdx].manager, 16);

    gameb defaultClubData{};
    io::loadDefaultClubdata(gamePath, defaultClubData);
    std::strncpy(clubData.club[oldClubIdx].manager, defaultClubData.club[oldClubIdx].manager, 16);
}

std::vector<club_player> findFreePlayers() {
    std::vector<club_player> freePlayers;

    for (int clubIdx = 0; clubIdx < 114; ++clubIdx) {
        ClubRecord &club = getClub(clubIdx);
        for (int slot = 0; slot < 24; ++slot) {
            int16_t playerIdx = club.player_index[slot];
            if (playerIdx == -1) {
                continue;
            }

            PlayerRecord &player = playerData.player[playerIdx];
            if (club.league == 0 || player.contract != 0) {
                continue;
            }

            freePlayers.push_back({club, player});
        }
    }
    return freePlayers;
}

std::vector<club_player> getMyPlayers(int player) {
    std::vector<club_player> myPlayers;

    ClubRecord &club = getClub(gameData.manager[player].club_idx);
    for (int i = 0; i < 24; ++i) {
        int16_t playerIdx = club.player_index[i];
        if (playerIdx == -1) {
            continue;
        }

        PlayerRecord &p = playerData.player[playerIdx];
        myPlayers.push_back({club, p});
    }
    return myPlayers;
}

void levelAggression() {
    for (int16_t i = 0; i < 3932; ++i) {
        PlayerRecord &player = getPlayer(i);
        player.aggr = 5;
    }
}

namespace game_utils {

OfferResponse assessOffer(const club_player &playerInfo, int offerAmount, int currentGame) {
    OfferResponse result{false, ""};

    if (offerAmount <= 0) {
        snprintf(result.message, sizeof(result.message), "Enter a numeric offer");
        return result;
    }

    if (currentGame == 0) {
        snprintf(result.message, sizeof(result.message), "Load a game before bidding");
        return result;
    }

    int16_t playerIdx = findPlayerIndex(playerInfo.player);
    if (playerIdx == -1) {
        snprintf(result.message, sizeof(result.message), "Player not found in save");
        return result;
    }

    int fromClubIdx = findClubIndexForPlayer(playerIdx);
    if (fromClubIdx == -1) {
        snprintf(result.message, sizeof(result.message), "Unable to locate player's club");
        return result;
    }

    int myClubIdx = gameData.manager[0].club_idx;
    if (fromClubIdx == myClubIdx) {
        snprintf(result.message, sizeof(result.message), "Player already in your squad");
        return result;
    }

    int squadSlot = -1;
    const ClubRecord &sourceClub = playerInfo.club;
    for (int i = 0; i < 24; ++i) {
        if (sourceClub.player_index[i] == -1) {
            continue;
        }
        if (std::memcmp(&playerData.player[sourceClub.player_index[i]], &playerInfo.player, sizeof(PlayerRecord)) == 0) {
            squadSlot = i;
            break;
        }
    }

    int basePrice = determinePlayerPrice(playerInfo.player, playerInfo.club, squadSlot);
    int importance = std::max(determinePlayerImportance(playerInfo.player, playerInfo.club), 1);
    int askingPrice = static_cast<int>(basePrice * (1.0 + (importance - 1) * 0.15));

    if (offerAmount < askingPrice) {
        std::string priceText = formatCurrency(askingPrice);
        snprintf(result.message, sizeof(result.message), "Offer rejected - needs about £%s", priceText.c_str());
        return result;
    }

    ClubRecord &myClub = getClub(myClubIdx);
    if (findEmptySlot(myClub) == -1) {
        snprintf(result.message, sizeof(result.message), "No free slot in your squad");
        return result;
    }

    completeTransfer(playerIdx, fromClubIdx, myClubIdx, offerAmount);

    snprintf(result.message, sizeof(result.message), "Offer accepted - %12.12s signed", playerInfo.player.name);
    result.accepted = true;
    return result;
}

void beginOffer(InputHandler &input, char *footer, size_t footerSize, const club_player &playerInfo, int currentGame) {
    input.resetKeyPressCallbacks();
    input.startReadingTextInput([&input, footer, footerSize, playerInfo] {
        const char *buffer = input.getTextInput();
        std::string formattedAmount = std::strlen(buffer) ? formatCurrency(std::atoi(buffer)) : "..........";
        snprintf(footer, footerSize, "           Offer amount for %12.12s £%13.13s",
                 playerInfo.player.name, formattedAmount.c_str());
    });

    snprintf(footer, footerSize, "           Offer amount for %12.12s £..........", playerInfo.player.name);

    input.addKeyPressCallback(SDLK_RETURN, [&input, footer, footerSize, playerInfo, currentGame] {
        int offer = std::atoi(input.getTextInput());
        auto response = assessOffer(playerInfo, offer, currentGame);
        snprintf(footer, footerSize, "           %.58s", response.message);
        input.resetKeyPressCallbacks();
        input.endReadingTextInput();
    });
}

int16_t findPlayerIndex(const PlayerRecord &player) {
    for (int16_t idx = 0; idx < 3932; ++idx) {
        if (std::memcmp(&getPlayer(idx), &player, sizeof(PlayerRecord)) == 0) {
            return idx;
        }
    }

    return -1;
}

int findClubIndexForPlayer(int16_t playerIdx) {
    for (int clubIdx = 0; clubIdx < 114; ++clubIdx) {
        ClubRecord &club = getClub(clubIdx);
        for (int slot = 0; slot < 24; ++slot) {
            if (club.player_index[slot] == playerIdx) {
                return clubIdx;
            }
        }
    }

    return -1;
}

int findEmptySlot(ClubRecord &club) {
    for (int slot = 0; slot < 24; ++slot) {
        if (club.player_index[slot] == -1) {
            return slot;
        }
    }
    return -1;
}

void completeTransfer(int16_t playerIdx, int fromClubIdx, int toClubIdx, int offerAmount) {
    ClubRecord &fromClub = getClub(fromClubIdx);
    ClubRecord &toClub = getClub(toClubIdx);

    for (int slot = 0; slot < 24; ++slot) {
        if (fromClub.player_index[slot] == playerIdx) {
            fromClub.player_index[slot] = -1;
            break;
        }
    }

    int destSlot = findEmptySlot(toClub);
    fromClub.bank_account += offerAmount;
    toClub.bank_account -= offerAmount;

    if (destSlot != -1) {
        toClub.player_index[destSlot] = playerIdx;
    }

    PlayerRecord &player = getPlayer(playerIdx);
    player.contract = std::max<uint8_t>(player.contract, static_cast<uint8_t>(2));
    player.morl = std::max<uint8_t>(player.morl, static_cast<uint8_t>(6));
}

void convertPlayerToCoach(struct gamea::ManagerRecord &manager, ClubRecord &club, int8_t clubPlayerIdx, char *footer,
                          size_t footerSize) {
    PlayerRecord &player = getPlayer(club.player_index[clubPlayerIdx]);

    std::unordered_map<char, int> playerTypeToEmployeePosition = {
            {'G', 8}, {'D', 9}, {'M', 10}, {'A', 11}
    };

    char playerType = determinePlayerType(player);
    int8_t playerRating = determinePlayerRating(player);

    struct gamea::ManagerRecord::employee &employee = manager.employee[playerTypeToEmployeePosition[playerType]];
    strncpy(employee.name, player.name, 12);
    employee.skill = playerRating;
    employee.age = 0;

    player.hn = player.hn / 2;
    player.tk = player.tk / 2;
    player.ps = player.ps / 2;
    player.sh = player.sh / 2;
    player.hd = player.hd / 2;
    player.cr = player.cr / 2;

    player.morl = 5;
    player.aggr = 1 + (std::rand() % 9);
    player.ins = 0;
    player.age = 16 + (std::rand() % 4);
    player.foot = (std::rand() % 2);
    player.dpts = 0;
    player.played = 0;
    player.scored = 0;
    player.unk2 = 0;
    player.wage = 50 + (std::rand() % 500);
    player.ins_cost = 0;
    player.period = 0;
    player.period_type = 0;
    player.contract = 1;
    player.unk5 = 192;
    player.u23 = 0;
    player.u25 = 0;

    club.player_index[clubPlayerIdx] = -1;

    ClubRecord &new_club = getClub(92 + (std::rand() % (113 - 92 + 1)));

    new_club.player_index[23] = clubPlayerIdx;

    snprintf(footer, footerSize, "CONVERTED TO A COACH");
}

std::string formatCurrency(int amount) {
    std::string text = std::to_string(amount);
    int insertPosition = static_cast<int>(text.length()) - 3;
    while (insertPosition > 0) {
        text.insert(insertPosition, ",");
        insertPosition -= 3;
    }
    return text;
}

} // namespace game_utils
