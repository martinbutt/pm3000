// SWOS import helper: map SWOS TEAM files into PM3 club/player data.
#pragma once

#include <string>

#include "pm3_defs.hh"

namespace swos_import {

struct ImportReport {
    size_t teams_requested = 0;
    size_t teams_matched = 0;
    size_t teams_created = 0;
    size_t players_renamed = 0;
    size_t teams_unplaced = 0;
};

// Import teams from a SWOS TEAM.xxx file into the in-memory gameb/gamec globals.
// GAMEB clubs are matched by name; unmatched teams replace the first unmatched clubs.
// Players are renamed in-place (stats untouched) to match the imported squads.
// no club replacements are created and no squad structure is changed.
ImportReport importTeamsFromFile(const std::string &teamFile, const std::string &pm3Path, bool verbose = false);

} // namespace swos_import
