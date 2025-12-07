#include <cstring>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>

#include "pm3_utils.hh"

namespace {

int16_t gNextPlayerIdx = 0;

int16_t addPlayer(int ratingValue, int age = 26, int contract = 3, int wage = 500) {
    PlayerRecord player{};
    player.hn = ratingValue;
    player.tk = ratingValue;
    player.ps = ratingValue;
    player.sh = ratingValue;
    player.age = static_cast<uint8_t>(age);
    player.contract = static_cast<uint8_t>(contract);
    player.wage = static_cast<uint16_t>(wage);
    int16_t idx = gNextPlayerIdx++;
    playerData.player[idx] = player;
    return idx;
}

ClubRecord makeClub(int league, int squadSize, const std::vector<int16_t>& playerOrder) {
    ClubRecord club{};
    club.league = static_cast<uint8_t>(league);
    for (int i = 0; i < 24; ++i) {
        club.player_index[i] = -1;
    }
    for (int i = 0; i < squadSize && i < 24; ++i) {
        club.player_index[i] = playerOrder[i];
    }
    return club;
}

bool inRange(int value, int low, int high) {
    return value >= low && value <= high;
}

bool checkPrice(const std::string& label, int price, int low, int high) {
    std::cout << label << ": " << price << std::endl;
    if (!inRange(price, low, high)) {
        std::cerr << label << " out of expected range [" << low << ", " << high << "]\n";
        return false;
    }
    return true;
}

} // namespace

int main() {
    std::memset(&playerData, 0, sizeof(playerData));
    std::memset(&clubData, 0, sizeof(clubData));

    bool ok = true;

    // Premier League club with 24 players
    std::vector<int16_t> premierPlayers;
    premierPlayers.reserve(24);
    premierPlayers.push_back(addPlayer(99, 26, 4, 1500)); // star starter
    premierPlayers.push_back(addPlayer(88, 27, 3, 1200));
    premierPlayers.push_back(addPlayer(85, 24, 3, 1000));
    premierPlayers.push_back(addPlayer(84, 25, 3, 900));
    premierPlayers.push_back(addPlayer(83, 25, 3, 900));
    premierPlayers.push_back(addPlayer(82, 26, 3, 850)); // starter
    premierPlayers.push_back(addPlayer(80, 27, 3, 800));
    premierPlayers.push_back(addPlayer(79, 24, 2, 750));
    premierPlayers.push_back(addPlayer(78, 25, 2, 700));
    premierPlayers.push_back(addPlayer(77, 25, 2, 700));
    premierPlayers.push_back(addPlayer(76, 25, 2, 650));
    premierPlayers.push_back(addPlayer(74, 26, 2, 600)); // bench start
    premierPlayers.push_back(addPlayer(72, 27, 2, 600));
    premierPlayers.push_back(addPlayer(70, 26, 2, 550));
    premierPlayers.push_back(addPlayer(68, 25, 2, 500)); // reserves
    premierPlayers.push_back(addPlayer(66, 24, 2, 450));
    premierPlayers.push_back(addPlayer(65, 25, 2, 450));
    premierPlayers.push_back(addPlayer(64, 25, 2, 400));
    premierPlayers.push_back(addPlayer(63, 26, 2, 400));
    premierPlayers.push_back(addPlayer(62, 27, 2, 400));
    premierPlayers.push_back(addPlayer(61, 26, 2, 400));
    premierPlayers.push_back(addPlayer(60, 27, 2, 400));
    premierPlayers.push_back(addPlayer(59, 24, 2, 350));
    premierPlayers.push_back(addPlayer(58, 25, 2, 350));
    ClubRecord premierClub = makeClub(0, 24, premierPlayers);

    ok &= checkPrice("Premier star starter", determinePlayerPrice(playerData.player[premierPlayers[0]], premierClub, 0), 10'000'000, 25'000'000);
    ok &= checkPrice("Premier first-team starter", determinePlayerPrice(playerData.player[premierPlayers[5]], premierClub, 5), 2'000'000, 10'000'000);
    ok &= checkPrice("Premier bench", determinePlayerPrice(playerData.player[premierPlayers[12]], premierClub, 12), 1'000'000, 5'000'000);
    ok &= checkPrice("Premier reserve", determinePlayerPrice(playerData.player[premierPlayers[18]], premierClub, 18), 200'000, 2'000'000);

    // Premier League small squad (17 players)
    std::vector<int16_t> premierSmall;
    for (int i = 0; i < 17; ++i) {
        premierSmall.push_back(addPlayer(78 - i, 25, 3, 600));
    }
    ClubRecord premierClubSmall = makeClub(0, 17, premierSmall);
    ok &= checkPrice("Premier small-squad starter", determinePlayerPrice(playerData.player[premierSmall[1]], premierClubSmall, 1), 1'500'000, 9'000'000);
    ok &= checkPrice("Premier small-squad bench", determinePlayerPrice(playerData.player[premierSmall[12]], premierClubSmall, 12), 500'000, 3'000'000);

    // Division 1 club (league = 1), 22 players
    std::vector<int16_t> div1Players;
    for (int i = 0; i < 22; ++i) {
        int rating = 80 - i;
        div1Players.push_back(addPlayer(rating, 25, 3, 500));
    }
    ClubRecord div1Club = makeClub(1, 22, div1Players);
    ok &= checkPrice("Div1 starter", determinePlayerPrice(playerData.player[div1Players[2]], div1Club, 2), 400'000, 2'000'000);
    ok &= checkPrice("Div1 bench", determinePlayerPrice(playerData.player[div1Players[12]], div1Club, 12), 100'000, 1'200'000);

    // Division 2 club (league = 2), 20 players
    std::vector<int16_t> div2Players;
    for (int i = 0; i < 20; ++i) {
        int rating = 76 - i;
        div2Players.push_back(addPlayer(rating, 25, 3, 450));
    }
    ClubRecord div2Club = makeClub(2, 20, div2Players);
    ok &= checkPrice("Div2 starter", determinePlayerPrice(playerData.player[div2Players[1]], div2Club, 1), 200'000, 1'100'000);

    // Division 3 club (league = 3), 18 players
    std::vector<int16_t> div3Players;
    for (int i = 0; i < 18; ++i) {
        int rating = 72 - i;
        div3Players.push_back(addPlayer(rating, 25, 3, 400));
    }
    ClubRecord div3Club = makeClub(3, 18, div3Players);
    ok &= checkPrice("Div3 starter", determinePlayerPrice(playerData.player[div3Players[0]], div3Club, 0), 50'000, 800'000);

    if (!ok) {
        return 1;
    }

    std::cout << "All pm3_utils tests passed" << std::endl;

    // Load real-world samples from CSV (tests/pricing_samples.csv)
    std::ifstream csv("tests/pricing_samples.csv");
    if (!csv) {
        csv.open("../tests/pricing_samples.csv");
    }
    if (!csv) {
        std::cerr << "Could not open pricing_samples.csv\n";
        return 1;
    }

    std::string line;
    // discard header
    std::getline(csv, line);

    bool samplesOk = true;
    while (std::getline(csv, line)) {
        if (line.empty()) {
            continue;
        }
        std::stringstream ss(line);
        std::string field;
        std::vector<std::string> fields;
        while (std::getline(ss, field, ',')) {
            fields.push_back(field);
        }
        if (fields.size() < 15) {
            continue; // skip incomplete lines
        }

        std::string playerName = fields[0];
        std::string roleStr = fields[1];
        PlayerRecord sample{};
        sample.hn = static_cast<uint8_t>(std::stoi(fields[2]));
        sample.tk = static_cast<uint8_t>(std::stoi(fields[3]));
        sample.ps = static_cast<uint8_t>(std::stoi(fields[4]));
        sample.sh = static_cast<uint8_t>(std::stoi(fields[5]));
        sample.hd = static_cast<uint8_t>(std::stoi(fields[6]));
        sample.cr = static_cast<uint8_t>(std::stoi(fields[7]));
        sample.aggr = static_cast<uint8_t>(std::stoi(fields[8]));
        sample.age = static_cast<uint8_t>(std::stoi(fields[9]));
        sample.contract = static_cast<uint8_t>(std::stoi(fields[10]));
        sample.wage = static_cast<uint16_t>(std::stoi(fields[11]));

        int division = std::stoi(fields[12]);
        int squadSlot = std::stoi(fields[13]);
        int expected = std::stoi(fields[14]);

        int16_t idx = gNextPlayerIdx++;
        playerData.player[idx] = sample;

        ClubRecord club{};
        club.league = static_cast<uint8_t>(division);
        for (int i = 0; i < 24; ++i) {
            club.player_index[i] = -1;
        }
        int slot = std::min(std::max(squadSlot, 0), 23);
        club.player_index[slot] = idx;

        char valuationRole = roleStr.empty() ? determineValuationRole(sample) : roleStr[0];
        (void)valuationRole; // currently pricing uses internal determination

        int price = determinePlayerPrice(sample, club, slot);
        double ratio = expected > 0 ? static_cast<double>(price) / expected : 1.0;
        bool within = ratio >= 0.5 && ratio <= 1.5;
        std::cout << playerName << " price=" << price << " expected~" << expected << " ratio=" << ratio << std::endl;
        if (!within) {
            samplesOk = false;
            std::cerr << playerName << " deviates from expectation\n";
        }
    }

    if (!samplesOk) {
        return 1;
    }

    std::cout << "All pricing sample checks passed" << std::endl;
    return 0;
}
