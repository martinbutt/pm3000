// SWOS TEAM.xxx parser (embedded from external/swos_extract).
#include "swos_extract.hpp"

#include "rnc_decompress.hpp"

#include <fstream>
#include <iostream>
#include <vector>

using namespace swos;

namespace {

} // namespace

// ---------------------------------------------------------------
// Load TEAM.xxx files
// ---------------------------------------------------------------

TeamDB swos::load_teams(const std::string &team_file) {
    return load_teams(team_file, nullptr);
}

TeamDB swos::load_teams(const std::string &team_file, PlayerDB *players_out) {
    TeamDB out;

    std::ifstream f(team_file, std::ios::binary);
    if (!f) {
        std::cerr << "Failed to open team file: " << team_file << "\n";
        return out;
    }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), {});

    if (buf.size() < 2) {
        std::cerr << "TEAM file too small: " << buf.size() << " bytes\n";
        return out;
    }

    const size_t RECSZ = 684; // 0x2ac legacy SWOS team size
    uint8_t team_count = buf[1];
    size_t expected = 2 + static_cast<size_t>(team_count) * RECSZ;
    if (buf.size() < expected) {
        std::cerr << "TEAM file shorter than expected. Declared teams: "
                  << unsigned(team_count) << "\n";
        team_count = static_cast<uint8_t>((buf.size() - 2) / RECSZ);
    }

    for (size_t i = 0; i < team_count; i++) {
        const uint8_t *rec = &buf[2 + i * RECSZ];

        Team t;
        t.id = rec[0x01];         // team number
        t.league = rec[0x19];     // division

        // Team name at offset 0x05, 19 bytes, null-terminated
        t.name.clear();
        for (size_t j = 0x05; j < 0x05 + 19 && rec[j] != 0; j++) {
            t.name.push_back(static_cast<char>(rec[j]));
        }

        // Manager at offset 0x24 (16 bytes)
        for (size_t j = 0x24; j < 0x24 + 16 && rec[j] != 0; j++) {
            t.manager.push_back(static_cast<char>(rec[j]));
        }

        auto decode_color = [](uint8_t v) { return static_cast<uint8_t>(v); };
        uint8_t kit_type = rec[0x1C];
        uint8_t kit_first = rec[0x1D];
        uint8_t kit_second = rec[0x1E];
        uint8_t kit_shorts = rec[0x1F];
        uint8_t kit_socks = rec[0x20];
        t.kits[0].design = kit_type;
        t.kits[0].shirt_primary = decode_color(kit_first);
        t.kits[0].shirt_secondary = decode_color(kit_second);
        t.kits[0].shorts = decode_color(kit_shorts);
        t.kits[0].socks = decode_color(kit_socks);

        // Players start at offset 0x4C, 16 entries of 38 bytes each
        const size_t PLAYER_SZ = 38;
        const size_t PLAYER_START = 0x4C;

        for (int p = 0; p < 16; p++) {
            const uint8_t *prec = &rec[PLAYER_START + p * PLAYER_SZ];
            Player parsed;
            parsed.id = players_out ? static_cast<uint16_t>(players_out->players.size()) : 0;
            parsed.nationality = prec[0];
            parsed.position = prec[0x1A] & 0x07; // lower 3 bits = field position per SWOS format

            // Player name starts at offset 3, max 23 bytes
            for (size_t j = 3; j < 3 + 23 && prec[j] != 0; j++) {
                parsed.name.push_back(static_cast<char>(prec[j]));
            }

            auto scale_to_99 = [](uint8_t v) {
                return static_cast<uint8_t>((v * 99) / 255);
            };
            auto scale_nibble_99 = [](uint8_t nib) {
                return static_cast<uint8_t>((nib * 99) / 15);
            };
            auto scale_nibble_9 = [](uint8_t nib) {
                return static_cast<uint8_t>((nib * 9) / 15);
            };

            parsed.passing = scale_to_99(prec[0x1C]); // byte used as-is
            uint8_t shoot_head = prec[0x1D];
            parsed.shooting = scale_nibble_99(shoot_head >> 4);
            parsed.heading = scale_nibble_99(shoot_head & 0x0F);

            uint8_t tack_ctrl = prec[0x1E];
            parsed.tackling = scale_nibble_99(tack_ctrl >> 4);
            parsed.control = scale_nibble_99(tack_ctrl & 0x0F);

            uint8_t speed_fin = prec[0x1F];
            parsed.aggression = scale_nibble_9(speed_fin & 0x0F); // approximate using finishing nibble

            parsed.handling = parsed.control; // no dedicated field; use control as approximation
            parsed.age = 0;                    // age not present
            parsed.foot = 'b';                 // footedness not present

            if (players_out) {
                parsed.id = static_cast<uint16_t>(players_out->players.size());
                players_out->players.push_back(parsed);
                t.player_ids[p] = parsed.id;
            } else {
                t.player_ids[p] = static_cast<uint16_t>(p);
            }
        }

        out.teams.push_back(t);
    }

    return out;
}

