#include "convert_coach_screen.h"

#include <cstring>
#include <functional>
#include <string>

#include "text.h"
#include "game_utils.h"

namespace {
void confirmConvert(ScreenContext &context, const std::string &label, const std::function<void(void)> &action) {
    if (!context.addKeyPressCallback || !context.resetKeyPressCallbacks || !context.setFooterLine) {
        action();
        return;
    }

    context.resetKeyPressCallbacks();
    context.setFooterLine(("Convert " + label + "? (Y/N)").c_str());

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

void queueConvertCoachFax(int16_t playerIdx) {
    auto &news = gameData.manager[0].news;
    int slot = 0;
    for (; slot < static_cast<int>(std::size(news)); ++slot) {
        if (news[slot].type == 0) {
            break;
        }
    }
    if (slot >= static_cast<int>(std::size(news))) {
        slot = 0;
    }

    news[slot].type = 20;
    news[slot].amount = 0;
    news[slot].ix1 = 0;
    news[slot].ix2 = playerIdx;
    news[slot].ix3 = 0;
}
} // namespace

void ConvertCoachScreen::draw(bool attachClickCallbacks) {
    context.writeHeader("CONVERT PLAYER TO COACH", 1, nullptr);

    std::map<int8_t, PlayerRecord> validPlayers;

    struct gamea::ManagerRecord &manager = gameData.manager[0];
    ClubRecord &club = getClub(gameData.manager[0].club_idx);

    for (int8_t i = 0; i < 24; ++i) {
        if (club.player_index[i] == -1) {
            continue;
        }

        PlayerRecord &p = getPlayer(club.player_index[i]);

        if (p.age >= 29) {
            validPlayers[i] = p;
        }
    }

    if (validPlayers.empty()) {
        context.writeText("No players over 29 years old found", 8, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr, 0);
        return;
    }

    int textLine = 4;


    context.writeText("T PLAYER NAME  HN TK PS SH HD CR FT F M A AG WAGES COACH RATING", 3, Colors::TEXT_SUB_HEADING,
                      TEXT_TYPE_PLAYER, nullptr, 0);

    for (auto& pair : validPlayers) {  // Avoid structured bindings
        int8_t playerIdx = pair.first;
        PlayerRecord player = pair.second;
        char playerRow[74];
        int8_t playerRating = determinePlayerRating(player);
        snprintf(playerRow, sizeof(playerRow),
                 "%1c %12.12s %2.2d %2.2d %2.2d %2.2d %2.2d %2.2d %2.2d %1.1s %1.1d %1.1d %2.2d %5d %-12.12s",
                 determinePlayerType(player), player.name, player.hn,
                 player.tk, player.ps, player.sh, player.hd, player.cr,
                 player.ft, footShortLabels[player.foot], player.morl, player.aggr,
                 player.age, player.wage, ratingLabels[(playerRating - (playerRating % 5)) / 5]);

        std::string playerName(player.name, strnlen(player.name, sizeof(player.name)));
        int16_t globalPlayerIdx = club.player_index[playerIdx];
        auto clickCallback = [&, playerIdx, playerName, globalPlayerIdx]() {
            confirmConvert(context, playerName, [&]() {
                context.convertPlayerToCoach(manager, club, playerIdx);
                queueConvertCoachFax(globalPlayerIdx);
            });
        };

        context.writePlayer(playerRow, determinePlayerType(player), textLine++,
                            attachClickCallbacks ? clickCallback : std::function<void(void)>{});
    }

    for (int i = textLine; i <= 27; i++) {
        char playerRow[69] = ". ............ .. .. .. .. .. .. .. . . . .. ..... ............";
        context.writeText(playerRow, i, Colors::TEXT_2, TEXT_TYPE_PLAYER, nullptr, 0);
    }
}
