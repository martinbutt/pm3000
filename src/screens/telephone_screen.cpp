#include "telephone_screen.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "text.h"
#include "pm3_data.h"
#include "game_utils.h"

namespace {
void confirmTelephoneAction(ScreenContext &context, const std::string &label,
                            const std::function<void(void)> &action) {
    if (!context.addKeyPressCallback || !context.resetKeyPressCallbacks || !context.setFooterLine) {
        action();
        return;
    }

    context.resetKeyPressCallbacks();
    context.setFooterLine(("Confirm " + label + "? (Y/N)").c_str());

    auto clearPrompt = [&context]() {
        context.resetKeyPressCallbacks();
        context.setFooterLine("");
    };

    auto runAction = [clearPrompt, action]() {
        clearPrompt();
        action();
    };

    context.addKeyPressCallback('y', runAction);
    context.addKeyPressCallback('Y', runAction);
    context.addKeyPressCallback('n', clearPrompt);
    context.addKeyPressCallback('N', clearPrompt);
}

int calculateStadiumValue(struct gamea::ManagerRecord &manager) {
    int total = 0;

    switch (manager.stadium.ground_facilities.level) {
        case 3: total += 150000; [[fallthrough]];
        case 2: total += 50000; [[fallthrough]];
        case 1: total += 10000;
    }

    switch (manager.stadium.supporters_club.level) {
        case 3: total += 300000; [[fallthrough]];
        case 2: total += 75000; [[fallthrough]];
        case 1: total += 10000;
    }

    switch (manager.stadium.flood_lights.level) {
        case 2: total += 25000; [[fallthrough]];
        case 1: total += 15000;
    }

    switch (manager.stadium.scoreboard.level) {
        case 3: total += 20000; [[fallthrough]];
        case 2: total += 12000; [[fallthrough]];
        case 1: total += 8000;
    }

    switch (manager.stadium.undersoil_heating.level) {
        case 1: total += 500000; break;
    }

    switch (manager.stadium.changing_rooms.level) {
        case 2: total += 60000; [[fallthrough]];
        case 1: total += 25000;
    }

    switch (manager.stadium.gymnasium.level) {
        case 3: total += 50000; [[fallthrough]];
        case 2: total += 25000; [[fallthrough]];
        case 1: total += 250000;
    }

    switch (manager.stadium.car_park.level) {
        case 2: total += 1000000; [[fallthrough]];
        case 1: total += 400000;
    }

    switch (manager.stadium.safety_rating[0]) {
        case 4: total += 1000000; [[fallthrough]];
        case 3: total += 350000; [[fallthrough]];
        case 2: total += 150000; [[fallthrough]];
        case 1: total += 50000;
    }

    for (int i = 0; i < 4; ++i) {
        switch (manager.stadium.area_covering[i].level) {
            case 3: total += 100000; [[fallthrough]];
            case 2: total += 40000; [[fallthrough]];
            case 1: total += 15000;
        }

        if (manager.stadium.capacity[i].terraces == 0) {
            total += manager.stadium.capacity[i].seating * 25;
        } else {
            switch (manager.stadium.conversion[i].level) {
                case 2:
                    total += manager.stadium.capacity[i].seating * 100;
                    total += manager.stadium.capacity[i].seating * 100;
                    [[fallthrough]];
                case 1:
                    total += manager.stadium.capacity[i].seating * 75;
                    [[fallthrough]];
                case 0:
                    total += manager.stadium.capacity[i].seating * 50;
            }
        }
    }

    return total;
}
} // namespace

void TelephoneScreen::draw(bool attachClickCallbacks) {
    struct TelephoneMenuItem {
        std::string text;
        int line;
        std::function<void(void)> callback;
    };

    context.writeHeader("TELEPHONE", 1, nullptr);

    auto confirm = [this](const std::string &label, const std::function<void(void)> &action) {
        confirmTelephoneAction(context, label, action);
    };

    std::vector<TelephoneMenuItem> menuItems = {
            {"ADVERTISE FOR FANS               (£25,000)", 3, [this, confirm] {
                confirm("ADVERTISE FOR FANS", [this] {
                    context.resetTextBlocks();
                    context.setFooterLine("");
                    struct gamea::ManagerRecord &manager = gameData.manager[0];
                    ClubRecord &club = getClub(manager.club_idx);

                    int fandomIncreasePercent = 3 + std::rand() % (7 - 3 + 1);
                    club.seating_avg = std::min(club.seating_avg * (1.0 + (fandomIncreasePercent / 100.0)), static_cast<double>(club.seating_max));
                    club.bank_account -= 25000;

                    std::string result = "\"We'll certainly see our ticket sales increase after this!\" - Assistant Manager\n\n"
                                         "Fans increased by " + std::to_string(fandomIncreasePercent) + "%";
                    context.addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
                });
            }},
            {"ENTERTAIN TEAM                    (£5,000)", 4, [this, confirm] {
                confirm("ENTERTAIN TEAM", [this] {
                    context.resetTextBlocks();
                    context.setFooterLine("");
                    struct gamea::ManagerRecord &manager = gameData.manager[0];
                    ClubRecord &club = getClub(manager.club_idx);

                    for (int i = 0; i < 24; ++i) {
                        PlayerRecord &player = getPlayer(club.player_index[i]);
                        player.morl = 9;
                    }

                    std::string result = "\"The team disperse into the streets, singing the praises of their generous manager.\"\n\n"
                                         "Team morale has been boosted";
                    club.bank_account -= 5000;
                    context.addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
                });
            }},
            {"ARRANGE SMALL TRAINING CAMP     (£500,000)", 5, [this, confirm] {
                confirm("ARRANGE SMALL TRAINING CAMP", [this] {
                    context.resetTextBlocks();
                    context.setFooterLine("");
                    struct gamea::ManagerRecord &manager = gameData.manager[0];
                    ClubRecord &club = getClub(manager.club_idx);

                    for (int i = 0; i < 24; ++i) {
                        PlayerRecord &player = getPlayer(club.player_index[i]);
                        player.hn = std::min(player.hn + std::rand() % 2, 99);
                        player.tk = std::min(player.tk + std::rand() % 2, 99);
                        player.ps = std::min(player.ps + std::rand() % 2, 99);
                        player.sh = std::min(player.sh + std::rand() % 2, 99);
                        player.hd = std::min(player.hd + std::rand() % 2, 99);
                        player.cr = std::min(player.cr + std::rand() % 2, 99);
                        player.aggr = std::min(player.aggr + 1, 9);
                        player.ft = 99;
                        player.morl = 9;
                    }

                    std::string result = "\"The team is looking quicker on their feet!\" - Assistant Manager\n\n"
                                         "Team stats increased";
                    club.bank_account -= 500000;
                    context.addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
                });
            }},
            {"ARRANGE MEDIUM TRAINING CAMP  (£1,000,000)", 6, [this, confirm] {
                confirm("ARRANGE MEDIUM TRAINING CAMP", [this] {
                    context.resetTextBlocks();
                    context.setFooterLine("");
                    struct gamea::ManagerRecord &manager = gameData.manager[0];
                    ClubRecord &club = getClub(manager.club_idx);

                    for (int i = 0; i < 24; ++i) {
                        PlayerRecord &player = getPlayer(club.player_index[i]);

                        player.hn += std::rand() % 4;
                        player.hn = player.hn > 99 ? 99 : player.hn;
                        player.tk += std::rand() % 4;
                        player.tk = player.tk > 99 ? 99 : player.tk;
                        player.ps += std::rand() % 4;
                        player.ps = player.ps > 99 ? 99 : player.ps;
                        player.sh += std::rand() % 4;
                        player.sh = player.sh > 99 ? 99 : player.sh;
                        player.hd += std::rand() % 4;
                        player.hd = player.hd > 99 ? 99 : player.hd;
                        player.cr += std::rand() % 4;
                        player.cr = player.cr > 99 ? 99 : player.cr;
                        player.aggr += 1;
                        player.aggr = player.aggr > 9 ? 9 : player.aggr;
                        player.ft = 99;
                        player.morl = 9;
                    }

                    std::string result = "\"The boys showed real progress!\" - Assistant Manager\n\n"
                                         "Team stats increased";
                    club.bank_account -= 1000000;
                    context.addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
                });
            }},
            {"ARRANGE LARGE TRAINING CAMP   (£2,000,000)", 7, [this, confirm] {
                confirm("ARRANGE LARGE TRAINING CAMP", [this] {
                    context.resetTextBlocks();
                    context.setFooterLine("");
                    struct gamea::ManagerRecord &manager = gameData.manager[0];
                    ClubRecord &club = getClub(manager.club_idx);

                    for (int i = 0; i < 24; ++i) {
                        PlayerRecord &player = getPlayer(club.player_index[i]);
                        player.hn = std::min(player.hn + std::rand() % 8, 99);
                        player.tk = std::min(player.tk + std::rand() % 8, 99);
                        player.ps = std::min(player.ps + std::rand() % 8, 99);
                        player.sh = std::min(player.sh + std::rand() % 8, 99);
                        player.hd = std::min(player.hd + std::rand() % 8, 99);
                        player.cr = std::min(player.cr + std::rand() % 8, 99);
                        player.aggr = std::min(player.aggr + 1, 9);
                        player.ft = 99;
                        player.morl = 9;
                    }

                    std::string result = "\"They are like a new team!\" - Assistant Manager\n\n"
                                         "Team stats increased";
                    club.bank_account -= 2000000;
                    context.addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
                });
            }},
            {"APPEAL RED CARD                  (£10,000)", 8, [this, confirm] {
                confirm("APPEAL RED CARD", [this] {
                    context.resetTextBlocks();
                    context.setFooterLine("");
                    struct gamea::ManagerRecord &manager = gameData.manager[0];
                    ClubRecord &club = getClub(manager.club_idx);
                    std::string result = "No banned player found";

                    for (int i = 0; i < 24; ++i) {
                        PlayerRecord &player = getPlayer(club.player_index[i]);
                        if (player.period > 0 && player.period_type == 0) {
                            if (std::rand() % 2 == 0) {
                                player.period = 0;
                                result = "\"We see what you mean. We've overturned the decision for " +
                                         std::string(player.name, sizeof(player.name)) + ".\" - The FA";
                            } else {
                                result = "\"Sorry, but our decision was fair.\" - The FA";
                            }
                            club.bank_account -= 10000;
                            break;
                        }
                    }
                    context.addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
                });
            }},
            {"BUILD NEW 25k SEAT STADIUM    (£5,000,000)", 9, [this, confirm] {
                confirm("BUILD NEW 25k SEAT STADIUM", [this] {
                    context.resetTextBlocks();
                    struct gamea::ManagerRecord &manager = gameData.manager[0];
                    ClubRecord &club = getClub(manager.club_idx);

                    int currentStadiumValue = calculateStadiumValue(manager);

                    club.seating_max = 25000;

                    manager.stadium.ground_facilities.level = 2;
                    manager.stadium.supporters_club.level = 2;
                    manager.stadium.flood_lights.level = 1;
                    manager.stadium.scoreboard.level = 2;
                    manager.stadium.undersoil_heating.level = 0;
                    manager.stadium.changing_rooms.level = 1;
                    manager.stadium.gymnasium.level = 2;
                    manager.stadium.car_park.level = 1;

                    for (size_t i = 0; i < std::size(manager.stadium.safety_rating); ++i) {
                        manager.stadium.safety_rating[i] = 3;
                    }

                    for (int i = 0; i < 4; ++i) {
                        manager.stadium.capacity[i].seating = 6250;
                        manager.stadium.capacity[i].terraces = 0;
                        manager.stadium.conversion[i].level = 1;
                        manager.stadium.area_covering[i].level = 2;
                    }

                    club.bank_account -= (5000000 - currentStadiumValue);

                    std::string result = "\"The new 25,000 seat stadium is ready! The fans are going to love it!\" - Assistant Manager\n\n"
                                         "\"We'll buy your old stadium for £" + std::to_string(currentStadiumValue) +
                                         " to tear down and turn into flats\" - Local Council";
                    context.addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
                });
            }},
            {"BUILD NEW 50k SEAT STADIUM   (£15,000,000)", 10, [this, confirm] {
                confirm("BUILD NEW 50k SEAT STADIUM", [this] {
                    context.resetTextBlocks();
                    struct gamea::ManagerRecord &manager = gameData.manager[0];
                    ClubRecord &club = getClub(manager.club_idx);

                    int currentStadiumValue = calculateStadiumValue(manager);
                    club.seating_max = 50000;

                    manager.stadium.ground_facilities.level = 2;
                    manager.stadium.supporters_club.level = 2;
                    manager.stadium.flood_lights.level = 2;
                    manager.stadium.scoreboard.level = 2;
                    manager.stadium.undersoil_heating.level = 1;
                    manager.stadium.changing_rooms.level = 2;
                    manager.stadium.gymnasium.level = 2;
                    manager.stadium.car_park.level = 2;

                    for (size_t i = 0; i < std::size(manager.stadium.safety_rating); ++i) {
                        manager.stadium.safety_rating[i] = 4;
                    }

                    for (int i = 0; i < 4; ++i) {
                        manager.stadium.capacity[i].seating = 12500;
                        manager.stadium.capacity[i].terraces = 0;
                        manager.stadium.conversion[i].level = 2;
                        manager.stadium.area_covering[i].level = 3;
                    }

                    club.bank_account -= (15000000 - currentStadiumValue);

                    std::string result = "\"The new 50,000 seat stadium is ready! It's incredible!\" - Assistant Manager\n\n"
                                         "\"We'll buy your old stadium for £" + std::to_string(currentStadiumValue) +
                                         " for a nearby school to use\" - Local Council";
                    context.addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
                });
            }},
            {"BUILD NEW 100k SEAT STADIUM  (£30,000,000)", 11, [this, confirm] {
                confirm("BUILD NEW 100k SEAT STADIUM", [this] {
                    context.resetTextBlocks();
                    struct gamea::ManagerRecord &manager = gameData.manager[0];
                    ClubRecord &club = getClub(manager.club_idx);

                    int currentStadiumValue = calculateStadiumValue(manager);

                    club.seating_max = 100000;

                    manager.stadium.ground_facilities.level = 3;
                    manager.stadium.supporters_club.level = 3;
                    manager.stadium.flood_lights.level = 2;
                    manager.stadium.scoreboard.level = 3;
                    manager.stadium.undersoil_heating.level = 1;
                    manager.stadium.changing_rooms.level = 2;
                    manager.stadium.gymnasium.level = 3;
                    manager.stadium.car_park.level = 2;

                    for (size_t i = 0; i < std::size(manager.stadium.safety_rating); ++i) {
                        manager.stadium.safety_rating[i] = 4;
                    }

                    for (int i = 0; i < 4; ++i) {
                        manager.stadium.capacity[i].seating = 25000;
                        manager.stadium.capacity[i].terraces = 0;
                        manager.stadium.conversion[i].level = 2;
                        manager.stadium.area_covering[i].level = 3;
                    }

                    club.bank_account -= (30000000 - currentStadiumValue);

                    std::string result = "\"The new 100,000 seat stadium is ready! It's so nice, I bought my mum a season ticket!\" - Assistant Manager\n\n"
                                         "\"We'll buy your old stadium for £" + std::to_string(currentStadiumValue) +
                                         " to turn into a sports center\" - Local Council";
                    context.addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
                });
            }},
    };

    for (const auto &item: menuItems) {
        SDL_Color rowColor = context.defaultTextColor(item.line);
        context.writeText(
                item.text.c_str(),
                item.line,
                rowColor,
                TEXT_TYPE_SMALL,
                attachClickCallbacks ? item.callback : nullptr,
                0
        );
    }
}
