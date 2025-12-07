#include "pm3_utils.hh"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

gamea gameData;
gameb clubData;
gamec playerData;
saves savesDir;
prefs preferences;

char *gameaPath = nullptr;
char *gamebPath = nullptr;
char *gamecPath = nullptr;
char *savesPath = nullptr;
char *prefsPath = nullptr;
FILE *gameaFile = nullptr;
FILE *gamebFile = nullptr;
FILE *gamecFile = nullptr;
FILE *savesFile = nullptr;
FILE *prefsFile = nullptr;

static std::string gPm3LastError;

ClubRecord& getClub(int idx) {
    return clubData.club[idx];
}

PlayerRecord& getPlayer(int16_t idx) {
    return playerData.player[idx];
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

int determinePlayerPrice(const PlayerRecord &player, const ClubRecord &club, int squadSlot) {
    PlayerRecord &mutablePlayer = const_cast<PlayerRecord &>(player);
    int rating = determinePlayerRating(mutablePlayer);
    int age = player.age;

    double ageFactor = 1.0;
    if (age < 24) {
        ageFactor = 1.2;
    } else if (age < 28) {
        ageFactor = 1.1;
    } else if (age > 35) {
        ageFactor = 0.7;
    } else if (age > 32) {
        ageFactor = 0.85;
    }

    double contractFactor = 0.9 + (player.contract * 0.05);
    int wageInfluence = std::max<int>(player.wage, 200) * 30;

    // Rating-driven baseline sized to yield ~15-20m for 95-99 in top flight.
    double baseValue = static_cast<double>(rating) * static_cast<double>(rating) * 1500.0;

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
    double divisionFactor = 1.0;
    switch (club.league) {
        case 0: divisionFactor = 1.0; break;
        case 1: divisionFactor = 0.2; break;    // next league ~20%
        case 2: divisionFactor = 0.1; break;    // half again
        case 3: divisionFactor = 0.075; break;  // 75% of previous
        default: divisionFactor = 0.0375; break; // half again
    }

    double value = (baseValue * importanceFactor * squadFactor * divisionFactor + wageInfluence) * ageFactor * contractFactor * 10;
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
    loadDefaultClubdata(gamePath, defaultClubData);
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

template <typename T>
static bool load_binary_file(const std::string &filepath, T &data) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        gPm3LastError = "Missing file: " + filepath;
        return false;
    }
    file.read(reinterpret_cast<char*>(&data), sizeof(T));
    return true;
}

template <typename T>
static void save_binary_file(const std::string &filepath, const T &data) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open file for writing: " + filepath);
    }
    file.write(reinterpret_cast<const char*>(&data), sizeof(T));
}

void loadBinaries(int game_nr, const std::string &game_path, gamea &game_data, gameb &club_data, gamec &player_data) {
    load_binary_file(constructSaveFilePath(game_path, game_nr, 'A'), game_data);
    load_binary_file(constructSaveFilePath(game_path, game_nr, 'B'), club_data);
    load_binary_file(constructSaveFilePath(game_path, game_nr, 'C'), player_data);
}

void loadDefaultGamedata(const std::string &game_path, gamea &game_data) {
    load_binary_file(constructGameFilePath(game_path, std::string{kGameDataFile}), game_data);
}

void loadDefaultClubdata(const std::string &game_path, gameb &club_data) {
    load_binary_file(constructGameFilePath(game_path, std::string{kClubDataFile}), club_data);
}

void loadDefaultPlaydata(const std::string &game_path, gamec &player_data) {
    load_binary_file(constructGameFilePath(game_path, std::string{kPlayDataFile}), player_data);
}

bool loadMetadata(const std::string &game_path, saves &saves_dir_data, prefs &prefs_data) {
    Pm3GameType game_type = getPm3GameType(game_path.c_str());
    const char* saves_folder = getSavesFolder(game_type);
    if (saves_folder == nullptr) {
        gPm3LastError = "Invalid PM3 folder: could not find PM3 executable in " + game_path;
        return false;
    }

    std::filesystem::path full_path = std::filesystem::path(game_path) / saves_folder;
    bool saves_ok = load_binary_file(full_path / std::string{kSavesDirFile}, saves_dir_data);
    bool prefs_ok = load_binary_file(full_path / std::string{kPrefsFile}, prefs_data);

    if (!saves_ok || !prefs_ok) {
        gPm3LastError = "Could not load PM3 metadata (SAVES.DIR/PREFS) in " + full_path.string() + ". "
                           "Set the PM3 folder in Settings.";
        return false;
    }

    gPm3LastError.clear();
    return true;
}

void saveBinaries(int game_nr, const std::string &game_path, gamea &game_data, gameb &club_data, gamec &player_data) {
    save_binary_file(constructSaveFilePath(game_path, game_nr, 'A'), game_data);
    save_binary_file(constructSaveFilePath(game_path, game_nr, 'B'), club_data);
    save_binary_file(constructSaveFilePath(game_path, game_nr, 'C'), player_data);
}

void saveMetadata(const std::string &game_path, saves &saves_dir_data, prefs &prefs_data) {
    save_binary_file(constructGameFilePath(game_path, std::string{kSavesDirFile}), saves_dir_data);
    save_binary_file(constructGameFilePath(game_path, std::string{kPrefsFile}), prefs_data);
}

void updateMetadata(int game_nr) {
    savesDir.game[game_nr - 1].turn = gameData.turn;
    savesDir.game[game_nr - 1].year = gameData.year;

    saveMetadata(std::filesystem::path(gameaPath).parent_path(), savesDir, preferences);
}

std::filesystem::path constructSavesFolderPath(const std::string& game_path) {
    Pm3GameType game_type = getPm3GameType(game_path.c_str());
    return std::filesystem::path(game_path) / getSavesFolder(game_type);
}

std::filesystem::path constructSaveFilePath(const std::string& game_path, int gameNumber, char gameLetter) {
    return constructSavesFolderPath(game_path) / (std::string{kGameFilePrefix} + std::to_string(gameNumber) + gameLetter);
}

std::filesystem::path constructGameFilePath(const std::string &game_path, const std::string &file_name) {
    return std::filesystem::path(game_path) / file_name;
}

Pm3GameType getPm3GameType(const char *game_path) {
    std::filesystem::path full_path;
    full_path = std::filesystem::path(game_path) / kExeStandardFilename;
    if (std::filesystem::exists(full_path)) {
        return Pm3GameType::Standard;
    }

    full_path = std::filesystem::path(game_path) / kExeDeluxeFilename;
    if (std::filesystem::exists(full_path)) {
        return Pm3GameType::Deluxe;
    }

    return Pm3GameType::Unknown;
}

const char* getSavesFolder(Pm3GameType game_type) {
    switch (game_type) {
        case Pm3GameType::Standard:
            return kStandardSavesPath.data();
        case Pm3GameType::Deluxe:
            return kDeluxeSavesPath.data();
        default:
            return nullptr;
    }
}

const std::string& pm3LastError() {
    return gPm3LastError;
}
