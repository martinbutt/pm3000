// Import SWOS TEAM.xxx data into PM3 club/player datasets.
#include "swos_import.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <iostream>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <numeric>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>
#include <random>

#include "game_utils.h"
#include "io.h"
#include "pm3_data.h"

#include "swos_extract.hpp"

namespace swos_import {
namespace {

std::string normalize(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    bool lastSpace = false;
    for (char c : in) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            out.push_back(static_cast<char>(std::toupper(uc)));
            lastSpace = false;
        } else if (std::isspace(uc) || c == '-' || c == '_' || c == '.') {
            if (!lastSpace && !out.empty()) {
                out.push_back(' ');
                lastSpace = true;
            }
        }
    }
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) {
        out.pop_back();
    }
    return out;
}

std::vector<std::string> tokenize(const std::string &norm) {
    static const std::set<std::string> kStopwords = {
            "FC", "CITY", "TOWN", "COUNTY", "UNITED", "ATH", "ATHLETIC", "ATHETIC",
            "ATHLETICO", "WANDERERS", "WANDERER", "BORO", "SPORT", "SPORTING", "CLUB"
    };
    std::vector<std::string> tokens;
    std::string current;
    for (char c : norm) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!current.empty()) {
                if (!kStopwords.count(current)) {
                    tokens.push_back(current);
                }
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty() && !kStopwords.count(current)) {
        tokens.push_back(current);
    }
    return tokens;
}

std::string randomManagerName(std::mt19937 &rng) {
    static const std::vector<std::string> surnames = {
            "Taylor", "Smith", "Johnson", "Brown", "Williams", "Clark", "Jones", "Davis",
            "Wilson", "Evans", "Cooper", "Carter", "Fisher", "Grant", "Hughes", "Kelly",
            "King", "Moore", "Morgan", "Murray", "Parker", "Reed", "Scott", "Turner"
    };
    std::uniform_int_distribution<int> letterDist('A', 'Z');
    std::uniform_int_distribution<size_t> nameDist(0, surnames.size() - 1);
    char initial = static_cast<char>(letterDist(rng));
    return std::string(1, initial) + ". " + surnames[nameDist(rng)];
}

std::string randomStadiumName(std::mt19937 &rng) {
    static const std::vector<std::string> prefixes = {
            "River", "Park", "King", "Queen", "Victoria", "Liberty", "Oak", "Elm", "West", "East", "North", "South",
            "Union", "Central", "Highfield", "Stadium", "Meadow", "Valley", "Hill", "Forest", "Mill"
    };
    static const std::vector<std::string> suffixes = {
            " Park", " Ground", " Stadium", " Arena", " Field", " Gardens", " Meadows", " Lane", " Road"
    };
    std::uniform_int_distribution<size_t> preDist(0, prefixes.size() - 1);
    std::uniform_int_distribution<size_t> sufDist(0, suffixes.size() - 1);
    return prefixes[preDist(rng)] + suffixes[sufDist(rng)];
}

void copyString(char *dest, size_t destSize, const std::string &src) {
    if (destSize == 0) return;
    // Space-pad to clear remnants; no null terminator needed for fixed fields.
    std::memset(dest, ' ', destSize);
    size_t copyLen = std::min(src.size(), destSize);
    std::memcpy(dest, src.data(), copyLen);
}

std::string toTitleCase(const std::string &raw) {
    std::string out;
    out.reserve(raw.size());
    bool newWord = true;
    for (char c : raw) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isspace(uc)) {
            out.push_back(' ');
            newWord = true;
        } else {
            if (c == '\'' || c == '-') {
                out.push_back(c);
                newWord = true;
            } else {
                out.push_back(newWord ? static_cast<char>(std::toupper(uc)) : static_cast<char>(std::tolower(uc)));
                newWord = false;
            }
        }
    }
    // Collapse multiple spaces from original if any snuck in.
    std::string compact;
    compact.reserve(out.size());
    bool lastSpace = false;
    for (char c : out) {
        if (c == ' ') {
            if (!lastSpace) {
                compact.push_back(c);
            }
            lastSpace = true;
        } else {
            compact.push_back(c);
            lastSpace = false;
        }
    }
    while (!compact.empty() && compact.back() == ' ') {
        compact.pop_back();
    }
    return compact;
}

uint8_t clampNibble(uint8_t v) {
    if (v > 15) {
        return 15;
    }
    return v;
}

uint8_t toNibble(uint8_t swosColor) {
    return clampNibble(static_cast<uint8_t>(swosColor / 17)); // scale 0-255 -> 0-15
}

using Pm3Kit = std::remove_reference<decltype(ClubRecord::kit[0])>::type;

std::string formatClubLabel(int idx) {
    if (idx < 0 || idx >= kClubIdxMax) {
        return "<invalid>";
    }
    const ClubRecord &club = getClub(idx);
    auto safeLen = strnlen(club.name, sizeof(club.name));
    return std::string(club.name, safeLen);
}

std::string clubSortKey(int idx) {
    std::string name = formatClubLabel(idx);
    for (char &c : name) {
        unsigned char uc = static_cast<unsigned char>(c);
        c = static_cast<char>(std::toupper(uc));
    }
    return name;
}

struct SwosPlacement {
    int clubIdx;
    int league;
    std::string normalizedName;
};

void applyKit(Pm3Kit &kit, const swos::Team::Kit &src) {
    kit.shirt_design = src.design;
    kit.shirt_primary_color_r = toNibble(src.shirt_primary);
    kit.shirt_primary_color_g = toNibble(src.shirt_primary);
    kit.shirt_primary_color_b = toNibble(src.shirt_primary);
    kit.shirt_secondary_color_r = toNibble(src.shirt_secondary);
    kit.shirt_secondary_color_g = toNibble(src.shirt_secondary);
    kit.shirt_secondary_color_b = toNibble(src.shirt_secondary);
    kit.shorts_color_r = toNibble(src.shorts);
    kit.shorts_color_g = toNibble(src.shorts);
    kit.shorts_color_b = toNibble(src.shorts);
    kit.socks_color_r = toNibble(src.socks);
    kit.socks_color_g = toNibble(src.socks);
    kit.socks_color_b = toNibble(src.socks);
}

struct RoleScore {
    char role;
    double score;
};

char inferRole(const swos::Player &p) {
    // SWOS encodes position in byte 0x1A, lower 3 bits: 0=GK, 1/2=DEF, 3/4=MID, 5+=ATT (per SWOS docs).
    switch (p.position & 0x07) {
        case 0: return 'G';
        case 1:
        case 2: return 'D';
        case 3:
        case 4: return 'M';
        case 5:
        case 6:
        case 7: return 'A';
        default: break;
    }
    return 'A';
}

std::string extractLastName(const std::string &name) {
    // Collapse whitespace to single spaces.
    std::string compact;
    bool space = false;
    for (char c : name) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!space) {
                compact.push_back(' ');
                space = true;
            }
        } else {
            compact.push_back(c);
            space = false;
        }
    }
    std::string last = compact;
    auto pos = compact.find_last_of(' ');
    if (pos != std::string::npos && pos + 1 < compact.size()) {
        last = compact.substr(pos + 1);
    }
    if (last.empty()) {
        return last;
    }
    bool capitalize = true;
    for (size_t i = 0; i < last.size(); ++i) {
        unsigned char uc = static_cast<unsigned char>(last[i]);
        if (capitalize && std::isalpha(uc)) {
            last[i] = static_cast<char>(std::toupper(uc));
            capitalize = false;
        } else {
            last[i] = static_cast<char>(std::tolower(uc));
        }
        if (last[i] == '\'' || last[i] == '-') {
            capitalize = true;
        }
    }
    return last;
}

void writeName(char *dest, size_t destSize, const std::string &name) {
    char padded[32];
    size_t bufSize = std::min(destSize, sizeof(padded));
    std::memset(padded, ' ', bufSize);
    size_t copyLen = std::min(name.size(), bufSize);
    std::memcpy(padded, name.data(), copyLen);
    std::memcpy(dest, padded, bufSize);
}

int16_t decodePlayerIndex(int16_t raw, bool swapEndian) {
    if (!swapEndian) {
        return raw;
    }
    uint16_t u = static_cast<uint16_t>(raw);
    return static_cast<int16_t>((u >> 8) | (u << 8));
}

int renamePlayers(ClubRecord &club, const std::vector<swos::Player> &swosPlayers) {
    struct SlotInfo {
        int slot;
        PlayerRecord *record;
        char role;
        int rating;
        bool used = false;
    };

    std::vector<SlotInfo> slots;
    slots.reserve(24);
    for (int slot = 0; slot < 24; ++slot) {
        int16_t idx = decodePlayerIndex(club.player_index[slot], true);
        if (idx < 0 || idx >= static_cast<int>(std::size(playerData.player))) {
            continue;
        }
        PlayerRecord &p = getPlayer(idx);
        slots.push_back({slot, &p, determinePlayerType(p), determinePlayerRating(p), false});
    }

    auto findBestSlot = [&slots](char desiredRole) -> SlotInfo * {
        SlotInfo *best = nullptr;
        for (auto &entry : slots) {
            if (entry.used || entry.role != desiredRole) {
                continue;
            }
            if (!best || entry.rating > best->rating) {
                best = &entry;
            }
        }
        return best;
    };

    int renamed = 0;
    for (const auto &sw : swosPlayers) {
        char desiredRole = inferRole(sw);
        SlotInfo *target = findBestSlot(desiredRole);
        if (!target) {
            auto it = std::find_if(slots.begin(), slots.end(), [](const SlotInfo &entry) { return !entry.used; });
            if (it != slots.end()) {
                target = &(*it);
            }
        }
        if (!target) {
            continue;
        }
        std::string last = extractLastName(sw.name);
        writeName(target->record->name, sizeof(target->record->name), last);
        target->used = true;
        ++renamed;
    }
    return renamed;
}

void checkGameDataStructure(const std::string &stage, const std::string &pm3Path) {
    std::vector<std::string> issues;
    auto logIssue = [&](std::string msg) {
        issues.push_back(std::move(msg));
    };

    namespace fs = std::filesystem;
    fs::path path = io::constructGameFilePath(pm3Path, std::string{kGameDataFile});
    std::error_code ec;
    auto actualSize = fs::file_size(path, ec);
    if (ec) {
        logIssue("Failed to stat gamedata.dat: " + ec.message());
    } else {
        std::size_t expected = sizeof(gamea) + io::getGameaExtraBytes();
        if (actualSize != expected) {
            logIssue("Unexpected gamedata size: " + std::to_string(actualSize) +
                     " (expected " + std::to_string(expected) + ")");
        }
    }

    auto isValidClub = [](int idx) { return idx >= 0 && idx < kClubIdxMax; };
    auto isValidPlayer = [](int idx) {
        return idx >= 0 && idx < static_cast<int>(std::size(playerData.player));
    };

    auto checkClub = [&](const std::string &details, int idx) {
        if (idx == -1) {
            return;
        }
        if (!isValidClub(idx)) {
            logIssue(details + " references invalid club " + std::to_string(idx));
        }
    };

    auto checkPlayer = [&](const std::string &details, int idx) {
        if (idx == -1) {
            return;
        }
        if (!isValidPlayer(idx)) {
            logIssue(details + " references invalid player " + std::to_string(idx));
        }
    };

    auto checkIndexArray = [&](const std::string &label, const int16_t *arr, int count) {
        for (int i = 0; i < count; ++i) {
            checkClub(label + " slot " + std::to_string(i), arr[i]);
        }
    };

    checkIndexArray("club_index.premier_league", gameData.club_index.leagues.premier_league, 22);
    checkIndexArray("club_index.division_one", gameData.club_index.leagues.division_one, 24);
    checkIndexArray("club_index.division_two", gameData.club_index.leagues.division_two, 24);
    checkIndexArray("club_index.division_three", gameData.club_index.leagues.division_three, 22);
    checkIndexArray("club_index.conference_league", gameData.club_index.leagues.conference_league, 22);

    auto checkTableArray = [&](const std::string &label, const gamea::TableDivision *rows, int count) {
        for (int i = 0; i < count; ++i) {
            checkClub(label + " row " + std::to_string(i), rows[i].club_idx);
        }
    };

    checkTableArray("table.premier_league", gameData.table.leagues.premier_league, 22);
    checkTableArray("table.division_one", gameData.table.leagues.division_one, 24);
    checkTableArray("table.division_two", gameData.table.leagues.division_two, 24);
    checkTableArray("table.division_three", gameData.table.leagues.division_three, 22);
    checkTableArray("table.conference_league", gameData.table.leagues.conference_league, 22);

    for (int i = 0; i < 75; ++i) {
        const auto &entry = gameData.top_scorers.all[i];
        checkPlayer("top_scorer[" + std::to_string(i) + "].player_idx", entry.player_idx);
        checkClub("top_scorer[" + std::to_string(i) + "].club_idx", entry.club_idx);
    }

    for (int i = 0; i < 149; ++i) {
        const auto &entry = gameData.cuppy.all[i];
        checkClub("cup_entry[" + std::to_string(i) + "].club[0]", entry.club[0].idx);
        checkClub("cup_entry[" + std::to_string(i) + "].club[1]", entry.club[1].idx);
    }

    for (int i = 0; i < 2; ++i) {
        checkClub("charity_shield.club[" + std::to_string(i) + "]", gameData.the_charity_shield_history.club[i].idx);
    }

    for (int i = 0; i < 16; ++i) {
        checkClub("some_table[" + std::to_string(i) + "].club1", gameData.some_table[i].club1_idx);
        checkClub("some_table[" + std::to_string(i) + "].club2", gameData.some_table[i].club2_idx);
    }

    for (int i = 0; i < 57; ++i) {
        const auto &entry = gameData.last_results.all[i];
        checkClub("last_results[" + std::to_string(i) + "].club[0]", entry.club[0].idx);
        checkClub("last_results[" + std::to_string(i) + "].club[1]", entry.club[1].idx);
    }

    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 20; ++j) {
            const auto &history = gameData.league[i].history[j];
            checkClub("league[" + std::to_string(i) + "].history[" + std::to_string(j) + "]", history.club_idx);
        }
    }

    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 20; ++j) {
            const auto &history = gameData.cup[i].history[j];
            checkClub("cup[" + std::to_string(i) + "].history[" + std::to_string(j) + "].winner", history.club_idx_winner);
            checkClub("cup[" + std::to_string(i) + "].history[" + std::to_string(j) + "].runner_up", history.club_idx_runner_up);
        }
    }

    for (int i = 0; i < 20; ++i) {
        const auto &fixture = gameData.fixture[i];
        checkClub("fixture[" + std::to_string(i) + "].club1", fixture.club_idx1);
        checkClub("fixture[" + std::to_string(i) + "].club2", fixture.club_idx2);
    }

    for (int i = 0; i < 45; ++i) {
        const auto &entry = gameData.transfer_market[i];
        checkPlayer("transfer_market[" + std::to_string(i) + "].player_idx", entry.player_idx);
        checkClub("transfer_market[" + std::to_string(i) + "].club_idx", entry.club_idx);
    }

    for (int i = 0; i < 6; ++i) {
        const auto &entry = gameData.transfer[i];
        checkPlayer("transfer[" + std::to_string(i) + "].player_idx", entry.player_idx);
        checkClub("transfer[" + std::to_string(i) + "].from_club", entry.from_club_idx);
        checkClub("transfer[" + std::to_string(i) + "].to_club", entry.to_club_idx);
    }

    checkClub("retired_manager_club_idx", gameData.retired_manager_club_idx);
    checkClub("new_manager_club_idx", gameData.new_manager_club_idx);

    if (!issues.empty()) {
        std::cerr << "[" << stage << "] GameData structural issues (" << issues.size() << "):\n";
        for (const auto &issue : issues) {
            std::cerr << "  " << issue << "\n";
        }
    }
}

void checkConsistency(const std::string &stage, const std::string &pm3Path, bool swapIndices) {
    checkGameDataStructure(stage, pm3Path);
    constexpr int kPlayerCount = static_cast<int>(std::size(playerData.player));
    std::vector<int> owner(kPlayerCount, -1);
    std::vector<std::tuple<int, int, int>> duplicates;
    std::vector<std::tuple<int, int, int>> invalidSlots;

    auto clubName = [](const ClubRecord &c) {
        return std::string(c.name, strnlen(c.name, sizeof(c.name)));
    };
    auto playerName = [](const PlayerRecord &p) {
        return std::string(p.name, strnlen(p.name, sizeof(p.name)));
    };

    for (int clubIdx = 0; clubIdx < kClubIdxMax; ++clubIdx) {
        const ClubRecord &club = getClub(clubIdx);
        for (int slot = 0; slot < 24; ++slot) {
            int16_t rawIdx = club.player_index[slot];
            if (rawIdx == -1) {
                continue;
            }
            int16_t idx = swapIndices ? decodePlayerIndex(rawIdx, true) : rawIdx;
            if (idx < 0 || idx >= kPlayerCount) {
                invalidSlots.emplace_back(clubIdx, slot, rawIdx);
                continue;
            }
            if (owner[idx] == -1) {
                owner[idx] = clubIdx;
            } else {
                duplicates.emplace_back(idx, owner[idx], clubIdx);
            }
        }
    }

    int missing = 0;
    std::vector<int> missingSamples;
    for (int i = 0; i < kPlayerCount; ++i) {
        if (owner[i] == -1) {
            ++missing;
            if (missingSamples.size() < 8) {
                missingSamples.push_back(i);
            }
        }
    }

    std::cerr << "[" << stage << "] Consistency: duplicates=" << duplicates.size()
              << " invalid_slots=" << invalidSlots.size()
              << " unassigned=" << missing << "\n";

    auto printClub = [&](int idx) {
        const ClubRecord &club = getClub(idx);
        return clubName(club);
    };

    for (size_t i = 0; i < duplicates.size() && i < 8; ++i) {
        auto [pidx, firstClub, secondClub] = duplicates[i];
        std::cerr << "  duplicate player " << pidx << " " << playerName(playerData.player[pidx])
                  << " in both " << printClub(firstClub) << " and " << printClub(secondClub) << "\n";
    }
    if (duplicates.size() > 8) {
        std::cerr << "  (+" << (duplicates.size() - 8) << " more duplicates hidden)\n";
    }

    for (size_t i = 0; i < invalidSlots.size() && i < 8; ++i) {
        auto [clubIdx, slot, value] = invalidSlots[i];
        std::cerr << "  invalid slot: club=" << printClub(clubIdx)
                  << " slot=" << slot << " idx=" << value << "\n";
    }
    if (invalidSlots.size() > 8) {
        std::cerr << "  (+" << (invalidSlots.size() - 8) << " more invalid slots hidden)\n";
    }

    if (!missingSamples.empty()) {
        std::cerr << "  sample unassigned players:\n";
        for (int idx : missingSamples) {
            std::cerr << "    " << idx << " " << playerName(playerData.player[idx]) << "\n";
        }
        if (missing > static_cast<int>(missingSamples.size())) {
            std::cerr << "    (+" << (missing - missingSamples.size()) << " more unassigned)\n";
        }
    }
}

void rebalanceLeagues(const std::vector<SwosPlacement> &swosPlacements) {
    constexpr std::array<int, 5> kStorageSizes{{22, 24, 24, 22, 22}};
    std::array<std::vector<int>, 5> tiers;
    std::array<std::vector<int>, 5> original;
    std::vector<bool> used(kClubIdxMax, false);

    auto clampLeague = [](int lg) {
        if (lg < 0) return 0;
        if (lg > 4) return 4;
        return lg;
    };

    auto loadLeague = [&](const int16_t *source, int count, std::vector<int> &dest) {
        dest.reserve(count);
        for (int i = 0; i < count; ++i) {
            dest.push_back(source[i]);
        }
    };
    loadLeague(gameData.club_index.leagues.premier_league, 22, original[0]);
    loadLeague(gameData.club_index.leagues.division_one, 24, original[1]);
    loadLeague(gameData.club_index.leagues.division_two, 24, original[2]);
    loadLeague(gameData.club_index.leagues.division_three, 22, original[3]);
    loadLeague(gameData.club_index.leagues.conference_league, 22, original[4]);

    std::vector<SwosPlacement> sortedPlacements = swosPlacements;
    std::sort(sortedPlacements.begin(), sortedPlacements.end(),
              [](const SwosPlacement &a, const SwosPlacement &b) {
                  if (a.league != b.league) {
                      return a.league < b.league;
                  }
                  return a.normalizedName < b.normalizedName;
              });

    for (const auto &placement : sortedPlacements) {
        int lg = clampLeague(placement.league);
        int clubIdx = placement.clubIdx;
        if (clubIdx < 0 || clubIdx >= kClubIdxMax) {
            continue;
        }
        tiers[lg].push_back(clubIdx);
        used[clubIdx] = true;
    }

    for (size_t t = 0; t < tiers.size(); ++t) {
        int storage = kStorageSizes[t];
        if (static_cast<int>(tiers[t].size()) > storage) {
            auto overflowBegin = tiers[t].begin() + storage;
            if (t + 1 < tiers.size()) {
                tiers[t + 1].insert(tiers[t + 1].end(), overflowBegin, tiers[t].end());
            }
            tiers[t].erase(overflowBegin, tiers[t].end());
        }
    }

    for (size_t t = 0; t < tiers.size(); ++t) {
        int storage = kStorageSizes[t];
        for (int idx : original[t]) {
            if (static_cast<int>(tiers[t].size()) >= storage) {
                break;
            }
            if (idx < 0 || idx >= kClubIdxMax || used[idx]) {
                continue;
            }
            tiers[t].push_back(idx);
            used[idx] = true;
        }
    }

    for (size_t t = 0; t + 1 < tiers.size(); ++t) {
        int storage = kStorageSizes[t];
        while (static_cast<int>(tiers[t].size()) < storage && !tiers[t + 1].empty()) {
            tiers[t].push_back(tiers[t + 1].front());
            tiers[t + 1].erase(tiers[t + 1].begin());
        }
    }

    auto fillWithUnused = [&](std::vector<int> &tier, int storage) {
        for (int idx = 0; idx < kClubIdxMax && static_cast<int>(tier.size()) < storage; ++idx) {
            if (used[idx]) {
                continue;
            }
            tier.push_back(idx);
            used[idx] = true;
        }
    };
    fillWithUnused(tiers.back(), kStorageSizes.back());

    auto sortTierAlphabetically = [](std::vector<int> &tier) {
        std::sort(tier.begin(), tier.end(), [](int lhs, int rhs) {
            return clubSortKey(lhs) < clubSortKey(rhs);
        });
    };
    for (auto &tier : tiers) {
        sortTierAlphabetically(tier);
    }

    auto writeLeague = [](int16_t *dest, int storageCount, const std::vector<int> &src) {
        int fill = std::min(static_cast<int>(src.size()), storageCount);
        for (int i = 0; i < storageCount; ++i) {
            if (i < fill) {
                dest[i] = static_cast<int16_t>(src[i]);
            } else {
                dest[i] = -1;
            }
        }
    };

    writeLeague(gameData.club_index.leagues.premier_league, kStorageSizes[0], tiers[0]);
    writeLeague(gameData.club_index.leagues.division_one, kStorageSizes[1], tiers[1]);
    writeLeague(gameData.club_index.leagues.division_two, kStorageSizes[2], tiers[2]);
    writeLeague(gameData.club_index.leagues.division_three, kStorageSizes[3], tiers[3]);
    writeLeague(gameData.club_index.leagues.conference_league, kStorageSizes[4], tiers[4]);
}


std::vector<swos::Player> collectTeamPlayers(const swos::Team &team, const swos::PlayerDB &db) {
    std::vector<swos::Player> out;
    for (uint16_t pid : team.player_ids) {
        if (pid < db.players.size()) {
            out.push_back(db.players[pid]);
        }
    }
    return out;
}

int levenshtein(const std::string &a, const std::string &b) {
    const size_t m = a.size();
    const size_t n = b.size();
    std::vector<int> dp(n + 1);
    for (size_t j = 0; j <= n; ++j) {
        dp[j] = static_cast<int>(j);
    }
    for (size_t i = 1; i <= m; ++i) {
        int prev = dp[0];
        dp[0] = static_cast<int>(i);
        for (size_t j = 1; j <= n; ++j) {
            int temp = dp[j];
            if (a[i - 1] == b[j - 1]) {
                dp[j] = prev;
            } else {
                dp[j] = std::min({prev, dp[j], dp[j - 1]}) + 1;
            }
            prev = temp;
        }
    }
    return dp[n];
}

double nameSimilarity(const std::string &aRaw, const std::string &bRaw) {
    std::string a = normalize(aRaw);
    std::string b = normalize(bRaw);
    if (a.empty() || b.empty()) {
        return 0.0;
    }
    if (a == b) {
        return 1.0;
    }
    if (a.find(b) != std::string::npos || b.find(a) != std::string::npos) {
        return 0.9;
    }
    int dist = levenshtein(a, b);
    double maxLen = static_cast<double>(std::max(a.size(), b.size()));
    return 1.0 - (static_cast<double>(dist) / maxLen);
}

std::optional<int> findBestClubMatch(const std::string &teamName,
                                     const std::vector<int> &candidateIdxs,
                                     const std::unordered_set<int> &alreadyMatched) {
    std::string normTeam = normalize(teamName);
    auto teamTokens = tokenize(normTeam);

    static const std::unordered_map<std::string, std::string> kSynonyms = {
            {"MIDDLESBROUGH", "MIDDLESBOROUGH"},
            {"QPR", "Q P R"},
            {"WEST BROMWICH", "W B A"},
            {"WOLVES", "WOLVERHAMPTON"},
    };
    auto synIt = kSynonyms.find(normTeam);
    if (synIt != kSynonyms.end()) {
        normTeam = synIt->second;
        teamTokens = tokenize(normTeam);
    }

    double bestScore = 0.0;
    int bestIdx = -1;
    for (int idx : candidateIdxs) {
        if (alreadyMatched.count(idx)) {
            continue;
        }
        const ClubRecord &club = getClub(idx);
        std::string clubName(club.name, strnlen(club.name, sizeof(club.name)));
        std::string normClub = normalize(clubName);
        auto clubTokens = tokenize(normClub);

        // Require token overlap to avoid loose matches.
        std::unordered_set<std::string> a(teamTokens.begin(), teamTokens.end());
        std::unordered_set<std::string> b(clubTokens.begin(), clubTokens.end());
        size_t overlap = 0;
        for (const auto &t : a) {
            if (b.count(t)) {
                ++overlap;
            }
        }
        size_t unionSize = a.size() + b.size() - overlap;
        if (unionSize == 0 || overlap == 0) {
            // If tokens were stripped entirely, fall back to exact normalized name.
            if (!(normTeam == normClub && !normTeam.empty())) {
                continue;
            }
        }

        double tokenScore = unionSize ? static_cast<double>(overlap) / static_cast<double>(unionSize) : 0.0;
        double score = tokenScore;
        // Tie-break with Levenshtein similarity.
        score += 0.25 * nameSimilarity(normTeam, normClub);

        if (score > bestScore) {
            bestScore = score;
            bestIdx = idx;
        }
    }

    // Require solid overlap to avoid wild matches.
    if (bestScore >= 0.60) {
        return bestIdx;
    }
    return std::nullopt;
}

} // namespace

ImportReport importTeamsFromFile(const std::string &teamFile, const std::string &pm3Path, bool verbose) {
    ImportReport report{};
    constexpr int kImportClubLimit = 114; // Only import into the first 114 clubs of GAMEB.

    swos::PlayerDB playerDb;
    swos::TeamDB teamDb = swos::load_teams(teamFile, &playerDb);
    report.teams_requested = teamDb.teams.size();
    if (teamDb.teams.empty()) {
        return report;
    }

    int clubLimit = std::min<int>(kImportClubLimit, kClubIdxMax);
    std::cout << "PM3 Teams:\n";
    for (int idx = 0; idx < clubLimit; ++idx) {
        const ClubRecord &club = getClub(idx);
        int length = strnlen(club.name, sizeof(club.name));
        std::cout << "  [" << idx << "] " << std::string(club.name, length) << "\n";
    }
    std::cout << "SWOS Teams:\n";
    for (const auto &team : teamDb.teams) {
        std::cout << "  " << team.name << "\n";
    }
    std::vector<int> allClubs(clubLimit);
    std::iota(allClubs.begin(), allClubs.end(), 0);

    std::unordered_set<int> matchedClubIdxs;
    std::unordered_set<std::string> matchedNames;
    std::vector<SwosPlacement> swosPlacements;
    std::vector<std::string> replacementTeamNames;

    std::random_device rd;
    std::mt19937 rng(rd());

    checkConsistency("Before base import", pm3Path, true);

    for (const auto &team : teamDb.teams) {
        auto match = findBestClubMatch(team.name, allClubs, matchedClubIdxs);
            if (match) {
                int clubIdx = *match;
                ClubRecord &club = getClub(clubIdx);
                matchedClubIdxs.insert(clubIdx);
                matchedNames.insert(normalize(team.name));
                ++report.teams_matched;

            if (verbose) {
            std::cout << "[MATCH] " << team.name << " -> club idx " << clubIdx
                      << " (" << std::string(club.name, strnlen(club.name, sizeof(club.name))) << ")\n";
            }

            if (!team.manager.empty()) {
                copyString(club.manager, sizeof(club.manager), team.manager);
            }

            // Preserve existing club name on a match to avoid clobbering PM3 data.

            swosPlacements.push_back({clubIdx, team.league, normalize(team.name)});
            int validSlots = 0;
            if (verbose) {
                for (int slot = 0; slot < 24; ++slot) {
                    int16_t idx = decodePlayerIndex(club.player_index[slot], true);
                    if (idx >= 0 && idx < static_cast<int>(std::size(playerData.player))) {
                        ++validSlots;
                    }
                }
            }
            auto players = collectTeamPlayers(team, playerDb);
            int renamed = renamePlayers(club, players);
                club.league = static_cast<uint8_t>(team.league);
                if (!team.kits.empty()) {
                    applyKit(club.kit[0], team.kits[0]);
                }
            report.players_renamed += renamed;
            if (verbose) {
                std::cout << "        players renamed: +" << renamed << " (team size " << players.size()
                          << ", valid slots " << validSlots << ")\n";
                if (validSlots == 0) {
                    std::cout << "        sample slots: ";
                    for (int slot = 0; slot < 6; ++slot) {
                        std::cout << club.player_index[slot] << (slot == 5 ? "" : ",");
                    }
                    std::cout << "\n";
                }
            }
        } else {
            replacementTeamNames.push_back(team.name);
        }
    }

    std::vector<int> unmatchedClubs;
    unmatchedClubs.reserve(clubLimit);
    for (int i = 0; i < clubLimit; ++i) {
        if (!matchedClubIdxs.count(i)) {
            unmatchedClubs.push_back(i);
        }
    }
    std::vector<SwosPlacement> replacementPlacements;
    std::vector<std::string> unmatchedIncomingTeams;

    for (const auto &team : teamDb.teams) {
        std::string norm = normalize(team.name);
        if (matchedNames.count(norm)) {
            continue;
        }
        if (unmatchedClubs.empty()) {
            ++report.teams_unplaced;
            unmatchedIncomingTeams.push_back(team.name);
            continue;
        }
        int clubIdx = unmatchedClubs.back();
        unmatchedClubs.pop_back();
        replacementPlacements.push_back({clubIdx, team.league, norm});

        ClubRecord &club = getClub(clubIdx);

        if (verbose) {
            std::cout << "[REPLACE] " << team.name << " -> club idx " << clubIdx
                      << " (was " << std::string(club.name, strnlen(club.name, sizeof(club.name))) << ")\n";
        }

        // Overwrite identity and season metadata.
        copyString(club.name, sizeof(club.name), toTitleCase(team.name));
        std::string managerName = team.manager.empty() ? randomManagerName(rng) : team.manager;
        copyString(club.manager, sizeof(club.manager), managerName);

        club.league = static_cast<uint8_t>(team.league);
        copyString(club.stadium, sizeof(club.stadium), randomStadiumName(rng));
        std::memset(club.weekly_league_position, 0, sizeof(club.weekly_league_position));

        applyKit(club.kit[0], team.kits[0]);
        applyKit(club.kit[1], team.kits[0]);
        applyKit(club.kit[2], team.kits[0]);

        auto players = collectTeamPlayers(team, playerDb);
        int renamed = renamePlayers(club, players);


        report.players_renamed += renamed;
        ++report.teams_created;
        if (verbose) {
            std::cout << "        players renamed: +" << renamed << "\n";
        }
    }

    swosPlacements.insert(swosPlacements.end(), replacementPlacements.begin(), replacementPlacements.end());
    rebalanceLeagues(swosPlacements);
    checkConsistency("After base import", pm3Path, true);

    if (!unmatchedIncomingTeams.empty()) {
        std::cout << "Unmatched incoming teams:\n";
        for (const auto &name : unmatchedIncomingTeams) {
            std::cout << "  " << name << "\n";
        }
    }

    if (!unmatchedClubs.empty()) {
        std::cout << "Unmatched existing clubs:\n";
        for (int idx : unmatchedClubs) {
            std::cout << "  [" << idx << "] " << formatClubLabel(idx) << "\n";
        }
    }

    if (!replacementTeamNames.empty()) {
        std::cout << "Replacement candidates (no MATCH):\n";
        for (const auto &name : replacementTeamNames) {
            std::cout << "  " << name << "\n";
        }
    }

    return report;
}

} // namespace swos_import
