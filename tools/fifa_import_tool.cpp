// Command-line helper to import FC (FIFA-style) player CSV data into PM3 playdata.
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <cstring>
#include <map>
#include <numeric>
#include <random>
#include <system_error>
#include <unordered_map>
#include <vector>
#include <array>

#include "io.h"
#include "pm3_defs.hh"

namespace {

std::string uppercaseToken(const std::string &word);

struct Args {
    std::string csvFile;
    std::string pm3Path;
    int gameNumber = 0;
    int year = 0;
    bool verbose = false;
    bool baseData = false;
    bool verifyGamedata = false;
    int playerId = 0;
    int debugPlayerId = 0;
    bool importLoans = false;
    std::string droppedClubsPath;
};

std::optional<Args> parseArgs(int argc, char **argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if ((a == "--csv" || a == "-c") && i + 1 < argc) {
            args.csvFile = argv[++i];
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
        } else if (a == "--player-id" && i + 1 < argc) {
            args.playerId = std::atoi(argv[++i]);
        } else if (a == "--debug-player" && i + 1 < argc) {
            args.debugPlayerId = std::atoi(argv[++i]);
        } else if (a == "--import-loans") {
            args.importLoans = true;
        } else if (a == "--dropped-clubs" && i + 1 < argc) {
            args.droppedClubsPath = argv[++i];
        }
    }

    if (args.csvFile.empty() || args.pm3Path.empty()) {
        return std::nullopt;
    }
    if (!args.baseData && (args.gameNumber < 1 || args.gameNumber > 8)) {
        return std::nullopt;
    }
    return args;
}

// Minimal CSV splitter that respects quoted fields.
std::vector<std::string> splitCsv(const std::string &line) {
    std::vector<std::string> fields;
    std::string current;
    bool inQuotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                current.push_back(c);
            }
        } else {
            if (c == '"') {
                inQuotes = true;
            } else if (c == ',') {
                fields.push_back(current);
                current.clear();
            } else {
                current.push_back(c);
            }
        }
    }
    fields.push_back(current);
    return fields;
}

int parseNumber(const std::string &field) {
    std::string digits;
    digits.reserve(field.size());
    for (char c : field) {
        if (std::isdigit(static_cast<unsigned char>(c)) || (c == '-' && digits.empty())) {
            digits.push_back(c);
        } else if (!digits.empty()) {
            break; // stop at the first non-digit once we've started capturing
        }
    }
    if (digits.empty() || digits == "-") {
        return 0;
    }
    try {
        return std::stoi(digits);
    } catch (...) {
        return 0;
    }
}

uint8_t clampByte(int v) {
    if (v < 0) return 0;
    if (v > 99) return 99;
    return static_cast<uint8_t>(v);
}

uint8_t scaleToTen(int stat) {
    // Map 0-99 style ratings onto a 0-9 range.
    int scaled = (stat + 5) / 10;
    if (scaled < 0) return 0;
    if (scaled > 9) return 9;
    return static_cast<uint8_t>(scaled);
}

std::string sanitizeName(const std::string &raw) {
    std::string out;
    out.reserve(raw.size());
    bool lastSpace = false;
    for (unsigned char c : raw) {
        if (c >= 32 && c < 127) {
            if (std::isspace(c)) {
                if (!lastSpace) {
                    out.push_back(' ');
                }
                lastSpace = true;
            } else {
                out.push_back(static_cast<char>(c));
                lastSpace = false;
            }
        } else {
            out.push_back('?');
            lastSpace = false;
        }
    }
    while (!out.empty() && std::isspace(static_cast<unsigned char>(out.back()))) {
        out.pop_back();
    }
    return out;
}

std::string trimCopy(const std::string &raw) {
    size_t start = 0;
    while (start < raw.size() && std::isspace(static_cast<unsigned char>(raw[start]))) {
        ++start;
    }
    size_t end = raw.size();
    while (end > start && std::isspace(static_cast<unsigned char>(raw[end - 1]))) {
        --end;
    }
    return raw.substr(start, end - start);
}

bool isLoanFieldSet(const std::string &value) {
    std::string trimmed = trimCopy(value);
    if (trimmed.empty()) {
        return false;
    }
    std::string upper = uppercaseToken(trimmed);
    return upper != "0" && upper != "NONE" && upper != "NULL" && upper != "N/A" && upper != "NA";
}

void writeFixed(char *dest, size_t destSize, const std::string &value) {
    std::memset(dest, ' ', destSize);
    size_t copyLen = std::min(value.size(), destSize);
    if (copyLen > 0) {
        std::memcpy(dest, value.data(), copyLen);
    }
}

struct ColumnMap {
    std::unordered_map<std::string, size_t> index;
};

ColumnMap buildColumnMap(const std::vector<std::string> &headers) {
    ColumnMap map;
    for (size_t i = 0; i < headers.size(); ++i) {
        map.index[headers[i]] = i;
    }
    return map;
}

const std::string &getField(const ColumnMap &map, const std::vector<std::string> &row, const std::string &key) {
    static const std::string kEmpty;
    auto it = map.index.find(key);
    if (it == map.index.end()) {
        return kEmpty;
    }
    size_t idx = it->second;
    if (idx >= row.size()) {
        return kEmpty;
    }
    return row[idx];
}

struct FifaRow {
    std::string name;
    std::string clubName;
    std::string clubLoanedFrom;
    std::string leagueName;
    int leagueLevel = 5;
    int leagueId = 0;
    int playerId = 0;
    std::string positions;
    int overall = 0;
    int age = 0;
    int preferredFoot = 3; // Any
    int pace = 0;
    int shooting = 0;
    int passing = 0;
    int dribbling = 0;
    int defending = 0;
    int physic = 0;
    int heading = 0;
    int ballControl = 0;
    int attackingCrossing = 0;
    int attackingFinishing = 0;
    int attackingShortPassing = 0;
    int attackingVolleys = 0;
    int skillDribbling = 0;
    int skillCurve = 0;
    int skillFkAccuracy = 0;
    int skillLongPassing = 0;
    int movementAcceleration = 0;
    int movementSprintSpeed = 0;
    int movementAgility = 0;
    int movementReactions = 0;
    int movementBalance = 0;
    int powerShotPower = 0;
    int powerJumping = 0;
    int powerStamina = 0;
    int powerStrength = 0;
    int powerLongShots = 0;
    int mentalityInterceptions = 0;
    int mentalityPositioning = 0;
    int mentalityVision = 0;
    int mentalityPenalties = 0;
    int defendingMarking = 0;
    int defendingStandingTackle = 0;
    int defendingSlidingTackle = 0;
    int aggression = 0;
    int composure = 0;
    int gkDiving = 0;
    int gkHandling = 0;
    int gkKicking = 0;
    int gkPositioning = 0;
    int gkReflexes = 0;
    int gkSpeed = 0;
    int wageEur = 0;
    int contractYear = 0;
};

std::optional<FifaRow> parseFifaRow(const ColumnMap &cols, const std::vector<std::string> &row) {
    FifaRow out;
    out.name = sanitizeName(getField(cols, row, "short_name"));
    out.playerId = parseNumber(getField(cols, row, "player_id"));
    out.clubName = sanitizeName(getField(cols, row, "club_name"));
    out.clubLoanedFrom = sanitizeName(getField(cols, row, "club_loaned_from"));
    out.leagueName = sanitizeName(getField(cols, row, "league_name"));
    out.leagueLevel = parseNumber(getField(cols, row, "league_level"));
    out.leagueId = parseNumber(getField(cols, row, "league_id"));
    out.positions = getField(cols, row, "player_positions");
    out.overall = parseNumber(getField(cols, row, "overall"));
    out.age = parseNumber(getField(cols, row, "age"));
    std::string foot = getField(cols, row, "preferred_foot");
    if (foot == "Left") out.preferredFoot = 0;
    else if (foot == "Right") out.preferredFoot = 1;
    else if (foot == "Both") out.preferredFoot = 2;

    out.pace = parseNumber(getField(cols, row, "pace"));
    out.shooting = parseNumber(getField(cols, row, "shooting"));
    out.passing = parseNumber(getField(cols, row, "passing"));
    out.dribbling = parseNumber(getField(cols, row, "dribbling"));
    out.defending = parseNumber(getField(cols, row, "defending"));
    out.physic = parseNumber(getField(cols, row, "physic"));
    out.heading = parseNumber(getField(cols, row, "attacking_heading_accuracy"));
    out.ballControl = parseNumber(getField(cols, row, "skill_ball_control"));
    out.attackingCrossing = parseNumber(getField(cols, row, "attacking_crossing"));
    out.attackingFinishing = parseNumber(getField(cols, row, "attacking_finishing"));
    out.attackingShortPassing = parseNumber(getField(cols, row, "attacking_short_passing"));
    out.attackingVolleys = parseNumber(getField(cols, row, "attacking_volleys"));
    out.skillDribbling = parseNumber(getField(cols, row, "skill_dribbling"));
    out.skillCurve = parseNumber(getField(cols, row, "skill_curve"));
    out.skillFkAccuracy = parseNumber(getField(cols, row, "skill_fk_accuracy"));
    out.skillLongPassing = parseNumber(getField(cols, row, "skill_long_passing"));
    out.movementAcceleration = parseNumber(getField(cols, row, "movement_acceleration"));
    out.movementSprintSpeed = parseNumber(getField(cols, row, "movement_sprint_speed"));
    out.movementAgility = parseNumber(getField(cols, row, "movement_agility"));
    out.movementReactions = parseNumber(getField(cols, row, "movement_reactions"));
    out.movementBalance = parseNumber(getField(cols, row, "movement_balance"));
    out.powerShotPower = parseNumber(getField(cols, row, "power_shot_power"));
    out.powerJumping = parseNumber(getField(cols, row, "power_jumping"));
    out.powerStamina = parseNumber(getField(cols, row, "power_stamina"));
    out.powerStrength = parseNumber(getField(cols, row, "power_strength"));
    out.powerLongShots = parseNumber(getField(cols, row, "power_long_shots"));
    out.mentalityInterceptions = parseNumber(getField(cols, row, "mentality_interceptions"));
    out.mentalityPositioning = parseNumber(getField(cols, row, "mentality_positioning"));
    out.mentalityVision = parseNumber(getField(cols, row, "mentality_vision"));
    out.mentalityPenalties = parseNumber(getField(cols, row, "mentality_penalties"));
    out.defendingMarking = parseNumber(getField(cols, row, "defending_marking_awareness"));
    out.defendingStandingTackle = parseNumber(getField(cols, row, "defending_standing_tackle"));
    out.defendingSlidingTackle = parseNumber(getField(cols, row, "defending_sliding_tackle"));
    out.aggression = parseNumber(getField(cols, row, "mentality_aggression"));
    out.composure = parseNumber(getField(cols, row, "mentality_composure"));
    out.gkDiving = parseNumber(getField(cols, row, "goalkeeping_diving"));
    out.gkHandling = parseNumber(getField(cols, row, "goalkeeping_handling"));
    out.gkKicking = parseNumber(getField(cols, row, "goalkeeping_kicking"));
    out.gkPositioning = parseNumber(getField(cols, row, "goalkeeping_positioning"));
    out.gkReflexes = parseNumber(getField(cols, row, "goalkeeping_reflexes"));
    out.gkSpeed = parseNumber(getField(cols, row, "goalkeeping_speed"));
    out.wageEur = parseNumber(getField(cols, row, "wage_eur"));
    out.contractYear = parseNumber(getField(cols, row, "club_contract_valid_until_year"));

    if (out.name.empty() || out.overall <= 0) {
        return std::nullopt;
    }
    return out;
}

int avgOrZero(std::initializer_list<int> values) {
    int sum = 0;
    int count = 0;
    for (int v : values) {
        if (v > 0) {
            sum += v;
            ++count;
        }
    }
    if (count == 0) {
        return 0;
    }
    return sum / count;
}

bool hasBothFootedPositions(std::string_view positions) {
    auto contains = [&](std::string_view token) {
        return positions.find(token) != std::string_view::npos;
    };
    return (contains("LB") && contains("RB")) ||
           (contains("LM") && contains("RM")) ||
           (contains("LW") && contains("RW"));
}

void packPeriodTypeAndContract(PlayerRecord &rec, uint8_t periodType, uint8_t contract) {
    uint8_t packed = static_cast<uint8_t>(((contract & 0x7) << 5) | (periodType & 0x1F));
    auto *bytes = reinterpret_cast<uint8_t *>(&rec);
    bytes[offsetof(PlayerRecord, period) + 1] = packed;
}

PlayerRecord buildRecord(const FifaRow &row, int baseYear, bool onLoan) {
    PlayerRecord rec{};
    writeFixed(rec.name, sizeof(rec.name), row.name);

    uint8_t overall = clampByte(row.overall);
    rec.u13 = rec.u15 = rec.u17 = rec.u19 = rec.u21 = rec.u23 = rec.u25 = overall;

    int gkSum = row.gkDiving + row.gkHandling + row.gkKicking + row.gkPositioning + row.gkReflexes + row.gkSpeed;
    rec.hn = clampByte(gkSum / 6);
    int shooting = row.shooting > 0 ? row.shooting
                                    : avgOrZero({row.attackingFinishing, row.attackingVolleys, row.powerShotPower,
                                                 row.powerLongShots, row.mentalityPositioning, row.mentalityPenalties});
    int passing = row.passing > 0 ? row.passing
                                  : avgOrZero({row.attackingCrossing, row.attackingShortPassing, row.skillCurve,
                                               row.skillLongPassing, row.mentalityVision});
    int dribbling = row.dribbling > 0 ? row.dribbling
                                      : avgOrZero({row.skillDribbling, row.ballControl, row.movementAgility,
                                                   row.movementBalance, row.movementReactions});
    int defending = row.defending > 0 ? row.defending
                                      : avgOrZero({row.defendingMarking, row.defendingStandingTackle,
                                                   row.defendingSlidingTackle, row.mentalityInterceptions});

    rec.tk = clampByte(defending);
    rec.ps = clampByte(passing);
    rec.sh = clampByte(shooting);
    rec.hd = clampByte(row.heading);
    rec.cr = clampByte(dribbling);
    rec.ft = clampByte(row.physic > 0 ? row.physic : 85);

    rec.morl = 0;
    rec.aggr = std::min<uint8_t>(scaleToTen(row.aggression), 9);

    rec.ins = 0;
    rec.age = static_cast<uint8_t>(std::clamp(row.age, 16, 34));
    int foot = row.preferredFoot;
    if (hasBothFootedPositions(row.positions)) {
        foot = 2;
    }
    rec.foot = static_cast<uint8_t>(foot);
    rec.dpts = 0;

    rec.played = 0;
    rec.scored = 0;
    rec.unk2 = 0;

    rec.wage = 0;
    rec.ins_cost = 0;

    uint8_t period = 0;
    uint8_t periodType = 0;
    if (onLoan) {
        constexpr uint8_t kOnLoan = 20;
        constexpr int kLoanWeeks = 36;
        constexpr int kTurnsPerWeek = 3;
        int loanPeriod = kLoanWeeks * kTurnsPerWeek;
        period = static_cast<uint8_t>(std::clamp(loanPeriod, 0, 255));
        periodType = kOnLoan;
    }

    int contractYearsLeft = 0;
    if (row.contractYear > 0 && baseYear > 0) {
        contractYearsLeft = row.contractYear - baseYear;
    }
    if (contractYearsLeft <= 0) {
        contractYearsLeft = 3;
    }
    rec.contract = static_cast<uint8_t>(std::clamp(contractYearsLeft, 0, 7));
    rec.unk5 = 0;
    rec.period = period;
    rec.period_type = periodType;
    packPeriodTypeAndContract(rec, periodType, rec.contract);

    rec.train = 0;
    rec.intense = 0;

    return rec;
}

void logDebugRow(const FifaRow &row, const PlayerRecord &rec) {
    std::cout << "Debug player_id=" << row.playerId << " name=" << row.name << " positions=" << row.positions;
    if (isLoanFieldSet(row.clubLoanedFrom)) {
        std::cout << " loaned_from=" << row.clubLoanedFrom;
    }
    std::cout << "\n";
    std::cout << "  CSV: shooting=" << row.shooting
              << " passing=" << row.passing
              << " dribbling=" << row.dribbling
              << " defending=" << row.defending
              << " heading=" << row.heading
              << " physic=" << row.physic
              << " gk(d/h/k/p/r/s)=" << row.gkDiving << "/" << row.gkHandling << "/" << row.gkKicking
              << "/" << row.gkPositioning << "/" << row.gkReflexes << "/" << row.gkSpeed
              << " foot=" << row.preferredFoot << "\n";
    std::cout << "  PM3: hn=" << static_cast<int>(rec.hn)
              << " tk=" << static_cast<int>(rec.tk)
              << " ps=" << static_cast<int>(rec.ps)
              << " sh=" << static_cast<int>(rec.sh)
              << " hd=" << static_cast<int>(rec.hd)
              << " cr=" << static_cast<int>(rec.cr)
              << " ft=" << static_cast<int>(rec.ft)
              << " morl=" << static_cast<int>(rec.morl)
              << " aggr=" << static_cast<int>(rec.aggr)
              << " foot=" << static_cast<int>(rec.foot)
              << " period=" << static_cast<int>(rec.period)
              << " period_type=" << static_cast<int>(rec.period_type)
              << " contract=" << static_cast<int>(rec.contract) << "\n";
}

void writeDroppedClubs(const std::string &path, const std::vector<int> &clubs, const gameb &clubDataOut) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to write dropped clubs file: " + path);
    }
    for (int idx : clubs) {
        if (idx < 0 || idx >= kClubIdxMax) {
            continue;
        }
        const ClubRecord &club = clubDataOut.club[idx];
        auto len = strnlen(club.name, sizeof(club.name));
        out << idx << "," << std::string(club.name, len) << "\n";
    }
}

struct ImportStats {
    size_t parsed = 0;
    size_t imported = 0;
    size_t skipped = 0;
};

struct ClubBucket {
    std::string name;
    std::string leagueName;
    int leagueLevel = 5;
    std::vector<FifaRow> players;
};

struct Placement {
    int clubIdx;
    int league;
    std::string normName;
};

std::string normalize(const std::string &in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc)) {
            out.push_back(static_cast<char>(std::toupper(uc)));
        } else if (std::isspace(uc) || c == '-' || c == '_') {
            if (!out.empty() && out.back() != ' ') {
                out.push_back(' ');
            }
        }
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

std::string toTitleCase(const std::string &raw);

char foldUtf8Sequence(std::string_view seq, size_t &consumed) {
    static const std::unordered_map<std::string_view, char> kFoldMap = {
            {"\xC3\x80", 'A'}, {"\xC3\x81", 'A'}, {"\xC3\x82", 'A'}, {"\xC3\x83", 'A'}, {"\xC3\x84", 'A'}, {"\xC3\x85", 'A'},
            {"\xC3\x87", 'C'},
            {"\xC3\x88", 'E'}, {"\xC3\x89", 'E'}, {"\xC3\x8A", 'E'}, {"\xC3\x8B", 'E'},
            {"\xC3\x8C", 'I'}, {"\xC3\x8D", 'I'}, {"\xC3\x8E", 'I'}, {"\xC3\x8F", 'I'},
            {"\xC3\x91", 'N'},
            {"\xC3\x92", 'O'}, {"\xC3\x93", 'O'}, {"\xC3\x94", 'O'}, {"\xC3\x95", 'O'}, {"\xC3\x96", 'O'}, {"\xC3\x98", 'O'},
            {"\xC3\x99", 'U'}, {"\xC3\x9A", 'U'}, {"\xC3\x9B", 'U'}, {"\xC3\x9C", 'U'},
            {"\xC3\x9D", 'Y'},
            {"\xC5\x81", 'L'}, {"\xC5\x83", 'N'}, {"\xC5\x9A", 'S'}, {"\xC5\xBB", 'Z'}, {"\xC5\xBD", 'Z'},
            {"\xC3\xA0", 'a'}, {"\xC3\xA1", 'a'}, {"\xC3\xA2", 'a'}, {"\xC3\xA3", 'a'}, {"\xC3\xA4", 'a'}, {"\xC3\xA5", 'a'},
            {"\xC3\xA7", 'c'},
            {"\xC3\xA8", 'e'}, {"\xC3\xA9", 'e'}, {"\xC3\xAA", 'e'}, {"\xC3\xAB", 'e'},
            {"\xC3\xAC", 'i'}, {"\xC3\xAD", 'i'}, {"\xC3\xAE", 'i'}, {"\xC3\xAF", 'i'},
            {"\xC3\xB1", 'n'},
            {"\xC3\xB2", 'o'}, {"\xC3\xB3", 'o'}, {"\xC3\xB4", 'o'}, {"\xC3\xB5", 'o'}, {"\xC3\xB6", 'o'}, {"\xC3\xB8", 'o'},
            {"\xC3\xB9", 'u'}, {"\xC3\xBA", 'u'}, {"\xC3\xBB", 'u'}, {"\xC3\xBC", 'u'},
            {"\xC3\xBD", 'y'}, {"\xC3\xBF", 'y'},
            {"\xC5\x82", 'l'}, {"\xC5\x84", 'n'}, {"\xC5\x9B", 's'}, {"\xC5\xBC", 'z'}, {"\xC5\xBE", 'z'},
            {"\xC4\x8C", 'C'}, {"\xC4\x8D", 'c'}, {"\xC4\x86", 'C'}, {"\xC4\x87", 'c'}, {"\xC4\x90", 'D'}, {"\xC4\x91", 'd'},
            {"\xC5\xA0", 'S'}, {"\xC5\xA1", 's'}, {"\xC5\xBD", 'Z'}, {"\xC5\xBE", 'z'},
            {"\xC5\xBD", 'Z'}, {"\xC5\xBE", 'z'},
            {"\xC4\x86", 'C'}, {"\xC4\x87", 'c'},
            {"\xC5\xB9", 'Z'}, {"\xC5\xBA", 'z'}, {"\xC5\xBB", 'Z'}, {"\xC5\xBC", 'z'},
            {"\xC5\x83", 'N'}, {"\xC5\x84", 'n'},
            {"\xC4\x98", 'E'}, {"\xC4\x99", 'e'},
            {"\xC5\xB0", 'U'}, {"\xC5\xB1", 'u'},
            {"\xC5\x84", 'n'}, {"\xC5\x83", 'N'},
            {"\xC5\xB3", 'U'}, {"\xC5\xB4", 'u'},
    };
    if (seq.size() >= 2) {
        auto it = kFoldMap.find(seq.substr(0, 2));
        if (it != kFoldMap.end()) {
            consumed = 2;
            return it->second;
        }
    }
    if (seq.size() >= 3) {
        auto it = kFoldMap.find(seq.substr(0, 3));
        if (it != kFoldMap.end()) {
            consumed = 3;
            return it->second;
        }
    }
    consumed = 1;
    return '?';
}

std::string foldAccentsToAscii(const std::string &raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t i = 0; i < raw.size(); ) {
        unsigned char c = static_cast<unsigned char>(raw[i]);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
            ++i;
        } else {
            size_t consumed = 1;
            char folded = foldUtf8Sequence(std::string_view(raw).substr(i), consumed);
            out.push_back(folded);
            i += consumed;
        }
    }
    return out;
}

std::string extractLastName(const std::string &raw) {
    std::string ascii = foldAccentsToAscii(raw);
    std::string cleaned;
    cleaned.reserve(ascii.size());
    bool lastSpace = false;
    for (char c : ascii) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalnum(uc) || c == '\'' || c == '-') {
            cleaned.push_back(c);
            lastSpace = false;
        } else if (std::isspace(uc)) {
            if (!lastSpace) {
                cleaned.push_back(' ');
                lastSpace = true;
            }
        }
    }
    while (!cleaned.empty() && cleaned.back() == ' ') {
        cleaned.pop_back();
    }
    if (cleaned.empty()) {
        return {};
    }
    size_t lastSpacePos = cleaned.find_last_of(' ');
    if (lastSpacePos == std::string::npos) {
        return toTitleCase(cleaned);
    }
    return toTitleCase(cleaned.substr(lastSpacePos + 1));
}

char firstInitial(const std::string &raw) {
    std::string ascii = foldAccentsToAscii(raw);
    for (char c : ascii) {
        unsigned char uc = static_cast<unsigned char>(c);
        if (std::isalpha(uc)) {
            return static_cast<char>(std::toupper(uc));
        }
    }
    return 'X';
}

std::string buildDisplayName(const FifaRow &row, int duplicates) {
    std::string last = extractLastName(row.name);
    if (last.empty()) {
        last = "Player";
    }
    if (duplicates > 1) {
        char init = firstInitial(row.name);
        std::string out;
        out.push_back(init);
        out.push_back(' ');
        out += last;
        return out;
    }
    return last;
}

void copyString(char *dest, size_t destSize, const std::string &src) {
    if (destSize == 0) return;
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

std::string uppercaseToken(const std::string &word) {
    std::string out = word;
    for (char &c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

std::string shortenClubName(const std::string &raw) {
    std::string name = toTitleCase(raw);
    if (name.empty()) {
        return name;
    }
    // Ensure AFC stays uppercase.
    {
        std::string out;
        out.reserve(name.size());
        std::string token;
        for (size_t i = 0; i <= name.size(); ++i) {
            char c = (i < name.size()) ? name[i] : ' ';
            if (c == ' ' || i == name.size()) {
                if (!token.empty()) {
                    if (uppercaseToken(token) == "AFC") {
                        out += "AFC";
                    } else {
                        out += token;
                    }
                    token.clear();
                }
                if (i < name.size()) {
                    out.push_back(' ');
                }
            } else {
                token.push_back(c);
            }
        }
        name = out;
    }

    std::vector<std::string> tokens;
    std::string current;
    for (char c : name) {
        if (c == ' ') {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }

    if (tokens.size() > 1 && uppercaseToken(tokens.back()) == "FC") {
        tokens.pop_back();
        name.clear();
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (!name.empty()) {
                name.push_back(' ');
            }
            name += tokens[i];
        }
    }

    if (name.size() <= 16) {
        return name;
    }

    static const std::unordered_map<std::string, std::string> kSpecialCases = {
            {"Queens Park Rangers", "Q.P.R."},
            {"West Bromwich Albion", "W.B.A."},
            {"West Bromwich", "W.B.A."},
            {"Preston North End", "P.N.E."},
            {"Nottingham Forest", "Nottm Forest"},
            {"Brighton & Hove Albion", "Brighton"},
            {"Brighton And Hove Albion", "Brighton"},
    };
    auto special = kSpecialCases.find(name);
    if (special != kSpecialCases.end()) {
        return special->second;
    }

    static const std::unordered_map<std::string, std::string> kSuffixMap = {
            {"United", "Utd"},
            {"City", "City"},
            {"Town", ""},
            {"Rovers", "Rvs"},
            {"Wanderers", "Wand"},
            {"Albion", "Alb"},
            {"Athletic", "Ath"},
            {"County", "Cty"},
            {"Wednesday", "Wed"}
    };

    if (tokens.empty()) {
        return name.substr(0, 16);
    }

    if (!tokens.empty()) {
        auto it = kSuffixMap.find(tokens.back());
        if (it != kSuffixMap.end()) {
            tokens.back() = it->second;
        }
    }

    std::string rebuilt;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (!rebuilt.empty()) {
            rebuilt.push_back(' ');
        }
        rebuilt += tokens[i];
    }

    if (rebuilt.size() <= 16) {
        return rebuilt;
    }

    if (tokens.size() > 1) {
        tokens.pop_back();
        rebuilt.clear();
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (!rebuilt.empty()) {
                rebuilt.push_back(' ');
            }
            rebuilt += tokens[i];
        }
        if (rebuilt.size() <= 16) {
            return rebuilt;
        }
    }

    return rebuilt.substr(0, 16);
}

std::vector<std::string> buildGeneratedNamePool() {
    static const std::vector<std::string> kLastNames = {
            "Abbott", "Adams", "Ainsworth", "Aldridge", "Allen", "Ashford", "Atkinson", "Atwood",
            "Baker", "Baldwin", "Barlow", "Barnes", "Barton", "Baxter", "Bennett", "Benson",
            "Berry", "Bevan", "Bishop", "Black", "Booth", "Bowen", "Bradley", "Brady",
            "Brennan", "Briggs", "Brooks", "Brown", "Burns", "Burton", "Byrne", "Callahan",
            "Cameron", "Carter", "Chandler", "Chapman", "Clarke", "Clayton", "Coleman", "Collins",
            "Connors", "Cooper", "Cox", "Crawford", "Cullen", "Dalton", "Dawson", "Denton",
            "Dixon", "Doyle", "Drew", "Duggan", "Dunn", "Eaton", "Edwards", "Ellison",
            "Ellis", "Evans", "Farrell", "Ferguson", "Finch", "Fletcher", "Flynn", "Foster",
            "Fox", "Gallagher", "Gibson", "Gilbert", "Goodwin", "Gordon", "Graham", "Grant",
            "Graves", "Griffin", "Hall", "Hamilton", "Hancock", "Harding", "Harper", "Harris",
            "Harrison", "Hart", "Harvey", "Hawkins", "Hayes", "Henderson", "Hewitt", "Higgins",
            "Hill", "Hodges", "Holland", "Holmes", "Howard", "Hughes", "Hunter", "Irving",
            "Jackson", "Jameson", "Jarvis", "Jenkins", "Jennings", "Johnson", "Johnston", "Jones",
            "Jordan", "Kavanagh", "Kelly", "Kendall", "Kennedy", "Kerr", "Knight", "Lacey",
            "Lambert", "Lawson", "Lennon", "Leonard", "Lewis", "Lloyd", "Logan", "Lowry",
            "Maguire", "Malone", "Manning", "Marsh", "Martin", "Mason", "Matthews", "McAllister",
            "McCarthy", "McCormack", "McDonald", "McDowell", "McGrath", "McGregor", "McKay", "McLean",
            "McNally", "McNeill", "Miller", "Mills", "Mitchell", "Monroe", "Montgomery", "Moore",
            "Moran", "Morris", "Morton", "Muir", "Murray", "Nash", "Neville", "Nolan",
            "O'Brien", "O'Connell", "O'Donnell", "O'Keefe", "O'Leary", "O'Neill", "O'Reilly", "O'Shea",
            "Olsen", "Osborne", "Owens", "Palmer", "Parker", "Parsons", "Patterson", "Payne",
            "Pearson", "Perry", "Porter", "Powell", "Quinn", "Ramsey", "Reed", "Reid",
            "Reilly", "Roberts", "Robertson", "Robinson", "Rogers", "Ross", "Rowe", "Russell",
            "Ryan", "Saunders", "Scott", "Shaw", "Shelton", "Simpson", "Sinclair", "Slater",
            "Spencer", "Stewart", "Stone", "Sullivan", "Sutton", "Taylor", "Thomas", "Thompson",
            "Thornton", "Tucker", "Turner", "Walker", "Wallace", "Walsh", "Ward", "Watson",
            "Weaver", "Webb", "Wells", "White", "Wilkins", "Wilkinson", "Williams", "Wilson",
            "Wright", "Young", "Adler", "Bannon", "Benoit", "Carlson", "Carver", "Cedric",
            "Cote", "Dahl", "Delacroix", "Duarte", "Ferrer", "Fischer", "Gallo", "Garnier",
            "Giuliani", "Hansen", "Hidalgo", "Holm", "Ibarra", "Ionescu", "Jensen", "Keller",
            "Kovacs", "Kowalski", "Larsen", "Lindholm", "Lombardi", "Madsen", "Marquez", "Mendes",
            "Moreau", "Morales", "Navarro", "Novak", "Nunez", "Okoro", "Orlov", "Papadakis",
            "Petrov", "Pires", "Quintana", "Ricci", "Rios", "Rossi", "Sakai", "Santos",
            "Sato", "Schubert", "Silva", "Soren", "Strom", "Suleiman", "Taddei", "Tanaka",
            "Tesfaye", "Tomas", "Urbina", "Valente", "Varga", "Vasquez", "Velasquez", "Vidal",
            "Volkov", "Wagner", "Weiss", "Wong", "Yamada", "Yilmaz", "Zanetti", "Zapata",
            "Zivkovic", "Zoric", "Zubkov", "Benkovic", "Dimitrov", "Katic", "Lovric", "Matija",
            "Pavlovic", "Stojanov", "Vesely", "Zielinski", "Nowak", "Hernandez", "Castillo", "Gomez"
    };
    return kLastNames;
}

bool isEnglishLeague(int leagueId) {
    static const std::array<int, 4> kEnglishLeagueIds = {13, 14, 60, 61}; // EPL + EFL tiers
    for (int id : kEnglishLeagueIds) {
        if (leagueId == id) {
            return true;
        }
    }
    return false;
}

std::string clubSortKey(const ClubRecord &club) {
    std::string key(club.name, strnlen(club.name, sizeof(club.name)));
    for (char &c : key) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return key;
}

int16_t decodePlayerIndex(int16_t raw, bool swapEndian) {
    if (!swapEndian) {
        return raw;
    }
    uint16_t u = static_cast<uint16_t>(raw);
    return static_cast<int16_t>((u >> 8) | (u << 8));
}

int16_t encodePlayerIndex(int16_t raw, bool swapEndian) {
    if (!swapEndian) {
        return raw;
    }
    uint16_t u = static_cast<uint16_t>(raw);
    return static_cast<int16_t>((u >> 8) | (u << 8));
}

void writeLeagueSlots(gamea &gd, const std::array<std::vector<int>, 5> &tiers) {
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

    constexpr std::array<int, 5> kStorageSizes{{22, 24, 24, 22, 22}};
    writeLeague(gd.club_index.leagues.premier_league, kStorageSizes[0], tiers[0]);
    writeLeague(gd.club_index.leagues.division_one, kStorageSizes[1], tiers[1]);
    writeLeague(gd.club_index.leagues.division_two, kStorageSizes[2], tiers[2]);
    writeLeague(gd.club_index.leagues.division_three, kStorageSizes[3], tiers[3]);
    writeLeague(gd.club_index.leagues.conference_league, kStorageSizes[4], tiers[4]);
}


//std::array<std::vector<int>, 5> rebalanceLeagues(const std::vector<Placement> &placements,
//                                                 const gameb &clubDataOut) {
//    constexpr std::array<int, 5> kStorageSizes{{22, 24, 24, 22, 22}};
//    std::array<std::vector<int>, 5> tiers;
//    std::vector<bool> used(kClubIdxMax, false);
//
//    auto clampLeague = [](int lg) {
//        if (lg < 0) return 0;
//        if (lg > 4) return 4;
//        return lg;
//    };
//
//    for (const auto &p : placements) {
//        int lg = clampLeague(p.league);
//        if (p.clubIdx < 0 || p.clubIdx >= kClubIdxMax) {
//            continue;
//        }
//        tiers[lg].push_back(p.clubIdx);
//        used[p.clubIdx] = true;
//    }
//
//    // Overflow trickles downward.
//    for (size_t t = 0; t < tiers.size(); ++t) {
//        int storage = kStorageSizes[t];
//        if (static_cast<int>(tiers[t].size()) > storage) {
//            auto overflowBegin = tiers[t].begin() + storage;
//            if (t + 1 < tiers.size()) {
//                tiers[t + 1].insert(tiers[t + 1].end(), overflowBegin, tiers[t].end());
//            }
//            tiers[t].erase(overflowBegin, tiers[t].end());
//        }
//    }
//
//    auto fillWithUnused = [&](std::vector<int> &tier, int storage) {
//        for (int idx = 0; idx < kClubIdxMax && static_cast<int>(tier.size()) < storage; ++idx) {
//            if (used[idx]) {
//                continue;
//            }
//            tier.push_back(idx);
//            used[idx] = true;
//        }
//    };
//    fillWithUnused(tiers.back(), kStorageSizes.back());
//
//    // Sort tiers alphabetically for stability.
//    for (auto &tier : tiers) {
//        std::sort(tier.begin(), tier.end(), [&](int lhs, int rhs) {
//            const ClubRecord &a = clubDataOut.club[lhs];
//            const ClubRecord &b = clubDataOut.club[rhs];
//            return clubSortKey(a) < clubSortKey(b);
//        });
//    }
//
//    return tiers;
//}

//std::vector<int> rebalanceLeagueTwoDown(std::array<std::vector<int>, 5> &tiers, const gameb &clubDataOut) {
//    constexpr int kLeagueTwo = 3;
//    constexpr int kNationalLeague = 4;
//    constexpr std::array<int, 5> kStorageSizes{{22, 24, 24, 22, 22}};
//
//    int overflow = static_cast<int>(tiers[kLeagueTwo].size()) - kStorageSizes[kLeagueTwo];
//    int toMove = std::min(2, std::max(0, overflow));
//    std::vector<int> dropped;
//    while (toMove > 0 && !tiers[kLeagueTwo].empty()) {
//        int idx = tiers[kLeagueTwo].back();
//        tiers[kLeagueTwo].pop_back();
//        tiers[kNationalLeague].insert(tiers[kNationalLeague].begin(), idx);
//        dropped.push_back(idx);
//        --toMove;
//    }
//
//    while (static_cast<int>(tiers[kNationalLeague].size()) > kStorageSizes[kNationalLeague]) {
//        tiers[kNationalLeague].pop_back();
//    }
//
//    std::vector<bool> used(kClubIdxMax, false);
//    for (const auto &tier : tiers) {
//        for (int idx : tier) {
//            if (idx >= 0 && idx < kClubIdxMax) {
//                used[idx] = true;
//            }
//        }
//    }
//    int storage = kStorageSizes[kNationalLeague];
//    for (int idx = 0; idx < kClubIdxMax && static_cast<int>(tiers[kNationalLeague].size()) < storage; ++idx) {
//        if (!used[idx]) {
//            tiers[kNationalLeague].push_back(idx);
//            used[idx] = true;
//        }
//    }
//
//    for (auto &tier : tiers) {
//        std::sort(tier.begin(), tier.end(), [&](int lhs, int rhs) {
//            const ClubRecord &a = clubDataOut.club[lhs];
//            const ClubRecord &b = clubDataOut.club[rhs];
//            return clubSortKey(a) < clubSortKey(b);
//        });
//    }
//    return dropped;
//}

ImportStats importCsvToPlayers(const std::string &csvPath, int baseYear, bool verbose,
                               int filterPlayerId, int debugPlayerId, bool importLoans,
                               gamea &gameDataOut, gameb &clubDataOut, gamec &playerOut,
                               std::vector<int> *droppedClubsOut) {
    std::ifstream in(csvPath);
    if (!in) {
        throw std::runtime_error("Failed to open CSV file: " + csvPath);
    }

    std::string headerLine;
    if (!std::getline(in, headerLine)) {
        throw std::runtime_error("CSV file is empty: " + csvPath);
    }

    auto headers = splitCsv(headerLine);
    ColumnMap colMap = buildColumnMap(headers);
    const std::vector<std::string> requiredColumns = {
            "player_id",
            "short_name",
            "overall",
            "player_positions",
            "age",
            "preferred_foot",
            "club_name",
            "club_loaned_from",
            "league_name",
            "league_level",
            "league_id",
            "pace",
            "shooting",
            "passing",
            "dribbling",
            "defending",
            "physic",
            "attacking_heading_accuracy",
            "skill_ball_control",
            "mentality_aggression",
            "mentality_composure",
            "goalkeeping_diving",
            "goalkeeping_handling",
            "goalkeeping_kicking",
            "goalkeeping_positioning",
            "goalkeeping_reflexes",
            "goalkeeping_speed",
            "attacking_crossing",
            "attacking_finishing",
            "attacking_short_passing",
            "attacking_volleys",
            "skill_dribbling",
            "skill_curve",
            "skill_fk_accuracy",
            "skill_long_passing",
            "movement_acceleration",
            "movement_sprint_speed",
            "movement_agility",
            "movement_reactions",
            "movement_balance",
            "power_shot_power",
            "power_jumping",
            "power_stamina",
            "power_strength",
            "power_long_shots",
            "mentality_interceptions",
            "mentality_positioning",
            "mentality_vision",
            "mentality_penalties",
            "defending_marking_awareness",
            "defending_standing_tackle",
            "defending_sliding_tackle",
            "wage_eur",
            "club_contract_valid_until_year"
    };
    for (const auto &key : requiredColumns) {
        if (!colMap.index.count(key)) {
            throw std::runtime_error("CSV missing required column: " + key);
        }
    }

    std::map<std::string, size_t> bucketIndex;
    std::vector<ClubBucket> buckets;
    ImportStats stats;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        auto fields = splitCsv(line);
        ++stats.parsed;
        auto parsedRow = parseFifaRow(colMap, fields);
        if (!parsedRow) {
            ++stats.skipped;
            continue;
        }
        if (filterPlayerId > 0 && parsedRow->playerId != filterPlayerId) {
            ++stats.skipped;
            continue;
        }
        if (!isEnglishLeague(parsedRow->leagueId)) {
            ++stats.skipped;
            continue;
        }
        std::string normClub = normalize(parsedRow->clubName);
        if (normClub.empty()) {
            ++stats.skipped;
            continue;
        }
        size_t idx;
        auto it = bucketIndex.find(normClub);
        if (it == bucketIndex.end()) {
            idx = buckets.size();
            bucketIndex[normClub] = idx;
            ClubBucket bucket;
            bucket.name = parsedRow->clubName.empty() ? "Club " + std::to_string(idx + 1) : parsedRow->clubName;
            bucket.leagueName = parsedRow->leagueName;
            bucket.leagueLevel = parsedRow->leagueLevel > 0 ? parsedRow->leagueLevel : 5;
            buckets.push_back(bucket);
        } else {
            idx = it->second;
        }
        if (parsedRow->leagueLevel > 0 && parsedRow->leagueLevel < buckets[idx].leagueLevel) {
            buckets[idx].leagueLevel = parsedRow->leagueLevel;
        }
        if (buckets[idx].leagueName.empty() && !parsedRow->leagueName.empty()) {
            buckets[idx].leagueName = parsedRow->leagueName;
        }
        buckets[idx].players.push_back(*parsedRow);
        ++stats.imported;
    }

    if (buckets.empty()) {
        throw std::runtime_error("No clubs parsed from " + csvPath);
    }

    // Preserve National League (tier 5) club indices from the existing data.
    std::vector<int> originalConference;
    originalConference.reserve(22);
    for (int i = 0; i < 22; ++i) {
        int idx = gameDataOut.club_index.leagues.conference_league[i];
        if (idx >= 0 && idx < kClubIdxMax) {
            originalConference.push_back(idx);
        }
    }
    std::vector<int> targetClubs;
    auto collectOriginal = [&](const int16_t *src, int count) {
        for (int i = 0; i < count; ++i) {
            int idx = src[i];
            if (idx >= 0 && idx < kClubIdxMax) {
                targetClubs.push_back(idx);
            }
        }
    };
    collectOriginal(gameDataOut.club_index.leagues.premier_league, 22);
    collectOriginal(gameDataOut.club_index.leagues.division_one, 24);
    collectOriginal(gameDataOut.club_index.leagues.division_two, 24);
    collectOriginal(gameDataOut.club_index.leagues.division_three, 22);

    // Reset outputs.
    std::memset(&gameDataOut.club_index, 0xFF, sizeof(gameDataOut.club_index)); // -1 fill
    gamec newPlayers = playerOut;

    constexpr size_t kCapacity = std::extent_v<decltype(playerOut.player)>;
    size_t nextPlayerIdx = 0;
    std::vector<bool> used(kCapacity, false);
    for (int clubIdx : originalConference) {
        if (clubIdx < 0 || clubIdx >= kClubIdxMax) {
            continue;
        }
        const ClubRecord &club = clubDataOut.club[clubIdx];
        for (int slot = 0; slot < 24; ++slot) {
            int16_t idx = decodePlayerIndex(club.player_index[slot], true);
            if (idx >= 0 && static_cast<size_t>(idx) < kCapacity) {
                used[static_cast<size_t>(idx)] = true;
            }
        }
    }

    auto nextFreeIndex = [&]() -> int {
        while (nextPlayerIdx < kCapacity && used[nextPlayerIdx]) {
            ++nextPlayerIdx;
        }
        if (nextPlayerIdx >= kCapacity) {
            return -1;
        }
        int idx = static_cast<int>(nextPlayerIdx);
        used[nextPlayerIdx] = true;
        ++nextPlayerIdx;
        return idx;
    };
    std::vector<Placement> placements;
    placements.reserve(std::min<size_t>(buckets.size(), kClubIdxMax));
    auto countPremier = [&placements]() {
        int count = 0;
        for (const auto &p : placements) {
            if (p.league == 0) {
                ++count;
            }
        }
        return count;
    };

    enum class GeneratedRole { Keeper, Defender, Midfielder, Attacker };
    auto skewedStat = [](int minVal, int maxVal, std::mt19937 &rng) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        double u = dist(rng);
        double curved = u * u; // bias toward low values
        int val = static_cast<int>(std::lround(minVal + (maxVal - minVal) * curved));
        return std::clamp(val, minVal, maxVal);
    };
    auto buildGeneratedPlayer = [&](const std::string &name, int baseYear, std::mt19937 &rng, GeneratedRole role) {
        PlayerRecord rec{};
        copyString(rec.name, sizeof(rec.name), name);
        rec.hn = static_cast<uint8_t>(skewedStat(30, 99, rng));
        rec.tk = static_cast<uint8_t>(skewedStat(30, 99, rng));
        rec.ps = static_cast<uint8_t>(skewedStat(30, 99, rng));
        rec.sh = static_cast<uint8_t>(skewedStat(30, 99, rng));
        rec.hd = static_cast<uint8_t>(skewedStat(30, 99, rng));
        rec.cr = static_cast<uint8_t>(skewedStat(30, 99, rng));
        rec.ft = static_cast<uint8_t>(skewedStat(30, 99, rng));
        uint8_t minCore = std::min({rec.hn, rec.tk, rec.ps, rec.sh});
        uint8_t boosted = static_cast<uint8_t>(skewedStat(std::min<int>(minCore + 1, 99), 99, rng));
        switch (role) {
            case GeneratedRole::Keeper: rec.hn = boosted; break;
            case GeneratedRole::Defender: rec.tk = boosted; break;
            case GeneratedRole::Midfielder: rec.ps = boosted; break;
            case GeneratedRole::Attacker: rec.sh = boosted; break;
        }
        uint8_t overall = std::max({rec.hn, rec.tk, rec.ps, rec.sh});
        rec.u13 = rec.u15 = rec.u17 = rec.u19 = rec.u21 = rec.u23 = rec.u25 = overall;
        std::uniform_int_distribution<int> ageDist(18, 34);
        std::uniform_int_distribution<int> aggrDist(2, 9);
        std::uniform_int_distribution<int> footDist(0, 2);
        rec.morl = 0;
        rec.aggr = static_cast<uint8_t>(aggrDist(rng));
        rec.ins = 0;
        rec.age = static_cast<uint8_t>(ageDist(rng));
        rec.foot = static_cast<uint8_t>(footDist(rng));
        rec.dpts = 0;
        rec.played = 0;
        rec.scored = 0;
        rec.unk2 = 0;
        rec.wage = 0;
        rec.ins_cost =0;
        rec.period = 0;
        rec.period_type = 0;
        rec.contract = 3;
        rec.unk5 = 0;
        rec.train = 0;
        rec.intense = 0;
        (void)baseYear;
        return rec;
    };
    std::vector<std::string> namePool = buildGeneratedNamePool();
    std::mt19937 nameRng(20250921);
    std::shuffle(namePool.begin(), namePool.end(), nameRng);
    size_t nameIndex = 0;

    // Sort buckets by tier then name for deterministic assignment.
    std::vector<size_t> order(buckets.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const auto &ba = buckets[a];
        const auto &bb = buckets[b];
        if (ba.leagueLevel != bb.leagueLevel) {
            return ba.leagueLevel < bb.leagueLevel;
        }
        std::string an = ba.name;
        std::string bn = bb.name;
        std::transform(an.begin(), an.end(), an.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        std::transform(bn.begin(), bn.end(), bn.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
        return an < bn;
    });

    int premierBuckets = 0;
    for (const auto &bucket : buckets) {
        if (bucket.leagueLevel == 1) {
            ++premierBuckets;
        }
    }
    size_t reserveSlots = premierBuckets < 22 ? 2 : 0;
    size_t maxClubSlots = targetClubs.size() > reserveSlots ? targetClubs.size() - reserveSlots : 0;

    size_t clubIdx = 0;
    for (size_t ordIdx : order) {
        if (clubIdx >= maxClubSlots) {
            if (verbose) {
                std::cout << "Reached reserved slots for generated teams; remaining FIFA clubs skipped\n";
            }
            break;
        }
        if (nextPlayerIdx >= kCapacity) {
            if (verbose) {
                std::cout << "Player capacity reached (" << kCapacity << "). Skipping remaining clubs.\n";
            }
            break;
        }
        if (clubIdx >= targetClubs.size()) {
            ++stats.skipped;
            continue;
        }
        ClubBucket &bucket = buckets[ordIdx];
        int targetIdx = targetClubs[clubIdx];
        ClubRecord &club = clubDataOut.club[targetIdx];

        copyString(club.name, sizeof(club.name), shortenClubName(bucket.name));
        copyString(club.manager, sizeof(club.manager), "Manager");
        copyString(club.stadium, sizeof(club.stadium), toTitleCase(bucket.name + " Stadium"));

        for (int i = 0; i < 24; ++i) {
            club.player_index[i] = -1;
        }

        auto &players = bucket.players;
        auto isGoalkeeper = [](const FifaRow &p) {
            return p.positions.find("GK") != std::string::npos;
        };
        std::vector<FifaRow> goalkeepers;
        std::vector<FifaRow> outfield;
        goalkeepers.reserve(players.size());
        outfield.reserve(players.size());
        for (const auto &p : players) {
            if (isGoalkeeper(p)) {
                goalkeepers.push_back(p);
            } else {
                outfield.push_back(p);
            }
        }
        std::sort(goalkeepers.begin(), goalkeepers.end(), [](const FifaRow &lhs, const FifaRow &rhs) {
            return lhs.overall > rhs.overall;
        });
        std::sort(outfield.begin(), outfield.end(), [](const FifaRow &lhs, const FifaRow &rhs) {
            return lhs.overall > rhs.overall;
        });

        while (goalkeepers.size() < 2 && goalkeepers.size() < 3) {
            if (nameIndex >= namePool.size()) {
                std::shuffle(namePool.begin(), namePool.end(), nameRng);
                nameIndex = 0;
            }
            FifaRow gk{};
            gk.name = namePool[nameIndex++];
            gk.positions = "GK";
            gk.overall = 50;
            gk.age = 24;
            gk.gkDiving = 60;
            gk.gkHandling = 60;
            gk.gkKicking = 55;
            gk.gkPositioning = 58;
            gk.gkReflexes = 62;
            gk.gkSpeed = 50;
            goalkeepers.push_back(gk);
        }

        std::vector<FifaRow> selected;
        for (size_t i = 0; i < goalkeepers.size() && selected.size() < 3 && selected.size() < 16; ++i) {
            selected.push_back(goalkeepers[i]);
        }
        for (const auto &p : outfield) {
            if (selected.size() >= 16) {
                break;
            }
            selected.push_back(p);
        }

        size_t assigned = 0;
        std::unordered_map<std::string, int> nameCounts;
        for (const auto &p : selected) {
            std::string last = extractLastName(p.name);
            if (last.empty()) {
                last = "Player";
            }
            ++nameCounts[uppercaseToken(last)];
        }

        for (const auto &p : selected) {
            if (nextPlayerIdx >= kCapacity || assigned >= 16) {
                break;
            }
            bool onLoan = importLoans && isLoanFieldSet(p.clubLoanedFrom);
            PlayerRecord rec = buildRecord(p, baseYear, onLoan);
            if (debugPlayerId > 0 && p.playerId == debugPlayerId) {
                logDebugRow(p, rec);
            }
            std::string lastKey = uppercaseToken(extractLastName(p.name));
            int dupCount = 1;
            auto it = nameCounts.find(lastKey);
            if (it != nameCounts.end()) {
                dupCount = it->second;
            }
            std::string display = buildDisplayName(p, dupCount);
            copyString(rec.name, sizeof(rec.name), display);
            int idx = nextFreeIndex();
            if (idx == -1) {
                break;
            }
            newPlayers.player[idx] = rec;
            club.player_index[assigned] = encodePlayerIndex(static_cast<int16_t>(idx), true);
            ++assigned;
        }

        // Fill remaining squad slots with -1
        for (size_t i = assigned; i < 24; ++i) {
            club.player_index[i] = -1;
        }
        int leagueTier = bucket.leagueLevel > 0 ? bucket.leagueLevel - 1 : 4;
        if (leagueTier < 0) leagueTier = 4;
        if (leagueTier > 4) leagueTier = 4;
        club.league = static_cast<uint8_t>(leagueTier);

        placements.push_back({targetIdx, leagueTier, normalize(bucket.name)});
        ++clubIdx;
    }

    if (nextPlayerIdx == 0) {
        throw std::runtime_error("No players assigned from " + csvPath);
    }

    int premierCount = countPremier();
    if (premierCount < 22) {
        static const std::array<std::string, 2> kGeneratedTeams = {"AFC Richmond", "Melchester Rovers"};
        for (const auto &teamName : kGeneratedTeams) {
            if (premierCount >= 22) {
                break;
            }
            if (clubIdx >= targetClubs.size()) {
                if (verbose) {
                    std::cout << "No free club slots to add generated team " << teamName << "\n";
                }
                break;
            }
            if (nextPlayerIdx >= kCapacity) {
                if (verbose) {
                    std::cout << "No player capacity for generated team " << teamName << "\n";
                }
                break;
            }
            int targetIdx = targetClubs[clubIdx];
            ClubRecord &club = clubDataOut.club[targetIdx];
            copyString(club.name, sizeof(club.name), shortenClubName(teamName));
            copyString(club.manager, sizeof(club.manager), "Generated");
            copyString(club.stadium, sizeof(club.stadium), toTitleCase(teamName + " Stadium"));
            for (int i = 0; i < 24; ++i) {
                club.player_index[i] = -1;
            }
            for (int i = 0; i < 16; ++i) {
                if (nameIndex >= namePool.size()) {
                    std::shuffle(namePool.begin(), namePool.end(), nameRng);
                    nameIndex = 0;
                }
                std::string pname = namePool[nameIndex++];
                GeneratedRole role = GeneratedRole::Attacker;
                if (i < 2) {
                    role = GeneratedRole::Keeper;
                } else if (i < 7) {
                    role = GeneratedRole::Defender;
                } else if (i < 12) {
                    role = GeneratedRole::Midfielder;
                }
                PlayerRecord rec = buildGeneratedPlayer(pname, baseYear, nameRng, role);
                int idx = nextFreeIndex();
                if (idx == -1) {
                    break;
                }
                newPlayers.player[idx] = rec;
                club.player_index[i] = encodePlayerIndex(static_cast<int16_t>(idx), true);
            }
            club.league = 0;
            placements.push_back({targetIdx, 0, normalize(teamName)});
            ++clubIdx;
            ++premierCount;
        }
    }

    std::array<std::vector<int>, 5> tiers;
    for (const auto &p : placements) {
        if (p.league >= 0 && p.league <= 3) {
            tiers[p.league].push_back(p.clubIdx);
        }
    }
    for (auto &tier : tiers) {
        std::sort(tier.begin(), tier.end(), [&](int lhs, int rhs) {
            const ClubRecord &a = clubDataOut.club[lhs];
            const ClubRecord &b = clubDataOut.club[rhs];
            return clubSortKey(a) < clubSortKey(b);
        });
    }

    constexpr std::array<int, 5> kStorageSizes{{22, 24, 24, 22, 22}};
    std::vector<int> dropped;
    while (static_cast<int>(tiers[3].size()) > kStorageSizes[3]) {
        dropped.push_back(tiers[3].back());
        tiers[3].pop_back();
    }
    std::vector<bool> usedTier(kClubIdxMax, false);
    for (int t = 0; t < 4; ++t) {
        for (int idx : tiers[t]) {
            if (idx >= 0 && idx < kClubIdxMax) {
                usedTier[idx] = true;
            }
        }
    }
    for (int t = 0; t < 4; ++t) {
        while (static_cast<int>(tiers[t].size()) < kStorageSizes[t]) {
            int fill = -1;
            for (int idx : targetClubs) {
                if (!usedTier[idx]) {
                    fill = idx;
                    break;
                }
            }
            if (fill == -1) {
                break;
            }
            tiers[t].push_back(fill);
            usedTier[fill] = true;
        }
    }
    tiers[4] = originalConference;
    if (droppedClubsOut) {
        *droppedClubsOut = dropped;
    }
    writeLeagueSlots(gameDataOut, tiers);
    for (size_t t = 0; t < tiers.size(); ++t) {
        for (int idx : tiers[t]) {
            if (idx >= 0 && idx < kClubIdxMax) {
                clubDataOut.club[idx].league = static_cast<uint8_t>(t);
            }
        }
    }
    if (!tiers[0].empty()) {
        gameDataOut.manager[0].club_idx = static_cast<int16_t>(tiers[0][0]);
    }

    if (verbose) {
        std::cout << "Imported " << nextPlayerIdx << " players across " << placements.size() << " clubs\n";
    }

    stats.imported = nextPlayerIdx;
    playerOut = newPlayers;
    return stats;
}

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

} // namespace

int main(int argc, char **argv) {
    auto parsed = parseArgs(argc, argv);
    if (!parsed) {
        std::cerr << "Usage: fifa_import_tool --csv FC26_YYYYMMDD.csv --pm3 /path/to/PM3 (--game <1-8> | --base) "
                     "[--year <value>] [--verbose] [--verify-gamedata] [--player-id <id>] [--debug-player <id>] "
                     "[--import-loans] [--dropped-clubs <path>]\n";
        return 1;
    }
    Args args = *parsed;
    if (args.baseData && args.importLoans) {
        std::cerr << "Warning: --import-loans with --base will cause loan players to show as banned when starting a new game\n";
    }

    if (!io::backupPm3Files(args.pm3Path)) {
        std::cerr << "Failed to backup PM3 files: " << io::pm3LastError() << "\n";
        return 1;
    }

    if (args.verifyGamedata) {
        if (!verifyGamedataRoundtrip(args.pm3Path)) {
            return 1;
        }
    }

    gamea gameDataOut{};
    gameb clubDataOut{};
    gamec playerDataOut{};

    try {
        if (args.baseData) {
            io::loadDefaultGamedata(args.pm3Path, gameDataOut);
            io::loadDefaultClubdata(args.pm3Path, clubDataOut);
            io::loadDefaultPlaydata(args.pm3Path, playerDataOut);
        } else {
            io::loadBinaries(args.gameNumber, args.pm3Path, gameDataOut, clubDataOut, playerDataOut);
        }
    } catch (const std::exception &ex) {
        std::cerr << "Failed to load data: " << ex.what() << "\n";
        return 1;
    }

    if (args.year != 0) {
        gameDataOut.year = args.year;
    }
    int baseYear = gameDataOut.year;
    if (baseYear <= 0) {
        baseYear = 2025;
    }

    try {
        std::vector<int> droppedClubs;
        ImportStats stats = importCsvToPlayers(args.csvFile, baseYear, args.verbose,
                                               args.playerId, args.debugPlayerId, args.importLoans,
                                               gameDataOut, clubDataOut, playerDataOut,
                                               args.droppedClubsPath.empty() ? nullptr : &droppedClubs);
        std::cout << "Imported " << stats.imported << " players (parsed " << stats.parsed
                  << ", skipped " << stats.skipped << "). Base year: " << baseYear << "\n";
        if (!args.droppedClubsPath.empty()) {
            writeDroppedClubs(args.droppedClubsPath, droppedClubs, clubDataOut);
            if (args.verbose) {
                std::cout << "Dropped club list written to " << args.droppedClubsPath << "\n";
            }
        }
    } catch (const std::exception &ex) {
        std::cerr << "Import failed: " << ex.what() << "\n";
        return 1;
    }

    try {
        if (args.baseData) {
            io::saveDefaultGamedata(args.pm3Path, gameDataOut);
            io::saveDefaultClubdata(args.pm3Path, clubDataOut);
            io::saveDefaultPlaydata(args.pm3Path, playerDataOut);
        } else {
            io::saveBinaries(args.gameNumber, args.pm3Path, gameDataOut, clubDataOut, playerDataOut);
        }
    } catch (const std::exception &ex) {
        std::cerr << "Failed to save data: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
