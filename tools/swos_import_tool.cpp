// Command-line helper to import SWOS TEAM.xxx data into PM3 GAMEB/GAMEC saves.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include "io.h"
#include "pm3_data.h"
#include "swos_import.h"

bool verifyGamedataRoundtrip(const std::string &pm3Path) {
    namespace fs = std::filesystem;
    std::filesystem::path path = io::constructGameFilePath(pm3Path, std::string{kGameDataFile});
    if (!fs::exists(path)) {
        std::cerr << "gamedata.dat missing in " << pm3Path << "\n";
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open gamedata.dat for verification\n";
        return false;
    }
    std::vector<uint8_t> orig((std::istreambuf_iterator<char>(in)), {});
    if (orig.size() < sizeof(gamea)) {
        std::cerr << "gamedata.dat is too small for verification\n";
        return false;
    }

    gamea tmp{};
    std::memcpy(&tmp, orig.data(), sizeof(gamea));
    std::vector<uint8_t> tail;
    if (orig.size() > sizeof(gamea)) {
        tail.insert(tail.end(), orig.begin() + sizeof(gamea), orig.end());
    }

    fs::path verifyPath = path;
    verifyPath += ".verify";
    {
        std::ofstream out(verifyPath, std::ios::binary);
        if (!out) {
            std::cerr << "Failed to create temporary gamedata verify file\n";
            return false;
        }
        out.write(reinterpret_cast<const char*>(&tmp), sizeof(gamea));
        if (!tail.empty()) {
            out.write(reinterpret_cast<const char*>(tail.data()), static_cast<std::streamsize>(tail.size()));
        }
    }

    std::vector<uint8_t> roundtrip;
    std::error_code removeEc;
    {
        std::ifstream in2(verifyPath, std::ios::binary);
        if (!in2) {
            std::cerr << "Failed to re-open temporary gamedata verify file\n";
            fs::remove(verifyPath, removeEc);
            return false;
        }
        roundtrip.assign((std::istreambuf_iterator<char>(in2)), {});
    }
    fs::remove(verifyPath, removeEc);

    if (roundtrip != orig) {
        std::cerr << "gamedata.dat roundtrip mismatch\n";
        return false;
    }

    std::cout << "gamedata.dat roundtrip verified\n";
    return true;
}

namespace {

struct Args {
    std::string teamFile;
    std::string pm3Path;
    int gameNumber = 0;
    int year = 0;
    bool verbose = false;
    bool baseData = false;
    bool verifyGamedata = false;
};

std::optional<Args> parseArgs(int argc, char **argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--team" || a == "-t") && i + 1 < argc) {
            args.teamFile = argv[++i];
        } else if ((a == "--pm3" || a == "-p") && i + 1 < argc) {
            args.pm3Path = argv[++i];
        } else if ((a == "--game" || a == "-g") && i + 1 < argc) {
            args.gameNumber = std::atoi(argv[++i]);
        } else if ((a == "--year" || a == "-y") && i + 1 < argc) {
            args.year = std::atoi(argv[++i]);
        } else if (a == "--verbose" || a == "-v") {
            args.verbose = true;
        } else if (a == "--base" || a == "--default") {
            args.baseData = true;
        } else if (a == "--verify-gamedata") {
            args.verifyGamedata = true;
        }
    }

    if (args.teamFile.empty() || args.pm3Path.empty()) {
        return std::nullopt;
    }
    if (!args.baseData && (args.gameNumber < 1 || args.gameNumber > 8)) {
        return std::nullopt;
    }
    return args;
}

} // namespace

int main(int argc, char **argv) {
    auto parsed = parseArgs(argc, argv);
    if (!parsed) {
        std::cerr << "Usage: swos_import_tool --team TEAM.xxx --pm3 /path/to/PM3 (--game <1-8> | --base) [--year <value>] [--verbose]\n";
        return 1;
    }
    Args args = *parsed;

    if (args.verifyGamedata) {
        if (!verifyGamedataRoundtrip(args.pm3Path)) {
            return 1;
        }
    }

    try {
        if (args.baseData) {
            io::loadDefaultGamedata(args.pm3Path, gameData); // read-only ordering reference
            io::loadDefaultClubdata(args.pm3Path, clubData);
            io::loadDefaultPlaydata(args.pm3Path, playerData);
        } else {
            io::loadBinaries(args.gameNumber, args.pm3Path, gameData, clubData, playerData);
        }
    } catch (const std::exception &ex) {
        std::cerr << "Failed to load data: " << ex.what() << "\n";
        return 1;
    }

    if (args.year != 0) {
        gameData.year = args.year;
    }

    auto report = swos_import::importTeamsFromFile(args.teamFile, args.pm3Path, args.verbose);
    std::cout << "Imported " << report.teams_requested << " teams. "
              << "Matched: " << report.teams_matched
              << ", Created: " << report.teams_created
              << ", Unplaced: " << report.teams_unplaced
              << ", Players renamed: " << report.players_renamed << "\n";

    if (args.baseData) {
        io::saveDefaultGamedata(args.pm3Path, gameData);
        io::saveDefaultClubdata(args.pm3Path, clubData);
        io::saveDefaultPlaydata(args.pm3Path, playerData);
    } else {
        io::saveBinaries(args.gameNumber, args.pm3Path, gameData, clubData, playerData);
    }
    return 0;
}
