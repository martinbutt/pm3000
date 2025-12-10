#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "pm3_defs.hh"

template <typename T>
void printArray(const std::string &label, const T *data, std::size_t count) {
    std::cout << label << ":";
    for (std::size_t i = 0; i < count; ++i) {
        std::cout << " " << data[i];
    }
    std::cout << "\n";
}

void dumpClubIndex(const gamea &gameData) {
    constexpr std::array<const char *, 5> labels{
            "premier_league",
            "division_one",
            "division_two",
            "division_three",
            "conference_league"
    };
    constexpr std::array<int, 5> lengths{22, 24, 24, 22, 22};
    const int16_t *arrays[] = {
            gameData.club_index.leagues.premier_league,
            gameData.club_index.leagues.division_one,
            gameData.club_index.leagues.division_two,
            gameData.club_index.leagues.division_three,
            gameData.club_index.leagues.conference_league
    };
    for (std::size_t i = 0; i < labels.size(); ++i) {
        printArray(std::string{"club_index_leagues."} + labels[i], arrays[i], lengths[i]);
    }
}

void dumpLeagueTable(const gamea &gameData) {
    const auto &leagues = gameData.table.leagues;
    const gamea::TableDivision *arrays[] = {
            leagues.premier_league,
            leagues.division_one,
            leagues.division_two,
            leagues.division_three,
            leagues.conference_league
    };
    constexpr std::array<int, 5> lengths{22, 24, 24, 22, 22};
    const char *labels[] = {"premier", "division_one", "division_two", "division_three", "conference"};
    for (std::size_t li = 0; li < std::size(lengths); ++li) {
        for (int row = 0; row < lengths[li]; ++row) {
            const auto &entry = arrays[li][row];
            std::cout << "table." << labels[li] << "[" << row << "] club=" << entry.club_idx
                      << " hx=" << entry.hx << " hw=" << entry.hw << " hd=" << entry.hd << " hl=" << entry.hl
                      << " hf=" << entry.hf << " ha=" << entry.ha
                      << " ax=" << entry.ax << " aw=" << entry.aw << " ad=" << entry.ad << " al=" << entry.al
                      << " af=" << entry.af << " aa=" << entry.aa << " xx=" << entry.xx << "\n";
        }
    }
}

void dumpTopScorers(const gamea &gameData) {
    for (std::size_t i = 0; i < std::size(gameData.top_scorers.all); ++i) {
        const auto &entry = gameData.top_scorers.all[i];
        std::cout << "top_scorer[" << i << "] player=" << entry.player_idx << " club=" << entry.club_idx
                  << " played=" << static_cast<int>(entry.pl) << " scored=" << static_cast<int>(entry.sc) << "\n";
    }
}

void dumpSortedNumbers(const gamea &gameData) {
    printArray("sorted_numbers", gameData.sorted_numbers, std::size(gameData.sorted_numbers));
}

void dumpReferees(const gamea &gameData) {
    for (std::size_t i = 0; i < std::size(gameData.referee); ++i) {
        const auto &ref = gameData.referee[i];
        std::cout << "referee[" << i << "] name=" << std::string(ref.name, sizeof(ref.name))
                  << " age=" << static_cast<int>(ref.age) << "\n";
    }
}

void dumpCups(const gamea &gameData) {
    struct CupInfo {
        const gamea::CupEntry *entries;
        int count;
        std::string label;
    };
    CupInfo info[] = {
            {gameData.cuppy.competitions.the_fa_cup, 36, "the_fa_cup"},
            {gameData.cuppy.competitions.the_league_cup, 28, "the_league_cup"},
            {gameData.cuppy.competitions.data090, 4, "data090"},
            {gameData.cuppy.competitions.the_champions_cup, 16, "the_champions_cup"},
            {gameData.cuppy.competitions.data091, 16, "data091"},
            {gameData.cuppy.competitions.the_cup_winners_cup, 16, "the_cup_winners_cup"},
            {gameData.cuppy.competitions.the_uefa_cup, 32, "the_uefa_cup"},
            {&gameData.cuppy.competitions.the_charity_shield, 1, "the_charity_shield"}
    };
    for (const auto &cup : info) {
        for (int i = 0; i < cup.count; ++i) {
            const auto &entry = cup.entries[i];
            std::cout << "cup." << cup.label << "[" << i << "] club0=" << entry.club[0].idx
                      << " club1=" << entry.club[1].idx << " goals=(" << entry.club[0].goals << "," << entry.club[1].goals
                      << ") audience=(" << entry.club[0].audience << "," << entry.club[1].audience << ")\n";
        }
    }
}

int main(int argc, char **argv) {
    std::string pm3Path;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--pm3" && i + 1 < argc) {
            pm3Path = argv[++i];
        }
    }
    if (pm3Path.empty()) {
        std::cerr << "--pm3 <path> is required\n";
        return 1;
    }

    std::filesystem::path file = std::filesystem::path(pm3Path) / std::string{kGameDataFile};
    std::ifstream in(file, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open " << file << "\n";
        return 1;
    }
    gamea data{};
    in.read(reinterpret_cast<char *>(&data), static_cast<std::streamsize>(sizeof(gamea)));
    if (!in) {
        std::cerr << "Failed to read " << file << "\n";
        return 1;
    }

    dumpClubIndex(data);
    dumpLeagueTable(data);
    dumpTopScorers(data);
    dumpSortedNumbers(data);
    dumpReferees(data);
    dumpCups(data);
    return 0;
}
