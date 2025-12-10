// Minimal SWOS TEAM.xxx parser (embedded from external/swos_extract).
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace swos {

struct Player {
    uint16_t id = 0;
    std::string name;

    // Basic fields parsed from TEAM.xxx (legacy SWOS layout)
    uint8_t nationality = 0;
    uint8_t position = 0;

    // Approximate skill fields decoded from packed bytes (scaled to 0-99)
    uint8_t handling = 0;
    uint8_t tackling = 0;
    uint8_t passing = 0;
    uint8_t shooting = 0;
    uint8_t heading = 0;
    uint8_t control = 0;
    uint8_t aggression = 0; // scaled to 0-9
    uint8_t age = 0;        // not present in data; left at 0
    char foot = 'b';        // not present in data; default both
};

struct Team {
    uint16_t id = 0;
    uint16_t league = 0;
    std::string name;
    std::string manager;
    struct Kit {
        uint8_t design = 0;
        uint8_t shirt_primary = 0;
        uint8_t shirt_secondary = 0;
        uint8_t shorts = 0;
        uint8_t socks = 0;
    };
    std::array<Kit, 3> kits{};
    std::array<uint16_t, 16> player_ids{};
};

struct PlayerDB {
    std::vector<Player> players;
};

struct TeamDB {
    std::vector<Team> teams;
};

TeamDB load_teams(const std::string &team_file);
TeamDB load_teams(const std::string &team_file, PlayerDB *players_out);

} // namespace swos
