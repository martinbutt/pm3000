#include "scout_screen.h"

#include <cstdlib>
#include <memory>
#include <string>

#include "text.h"
#include "game_utils.h"

namespace {
constexpr int kMaxLoanWeeks = 36;
constexpr int kLoanPeriodType = 20;
constexpr int kTurnsPerWeek = 3;

struct LoanState {
    club_player playerInfo;
    int weeks = 0;
    int fee = 0;
    int16_t playerIdx = -1;
    int fromClubIdx = -1;
};

void clearInput(ScreenContext &context) {
    if (context.endReadingTextInput) {
        context.endReadingTextInput();
    }
    if (context.resetKeyPressCallbacks) {
        context.resetKeyPressCallbacks();
    }
    if (context.setFooterLine) {
        context.setFooterLine("");
    }
}

void finalizeLoan(ScreenContext &context, const std::shared_ptr<LoanState> &state) {
    if (!context.setFooterLine || !context.resetKeyPressCallbacks) {
        return;
    }

    context.resetKeyPressCallbacks();

    if (state->playerIdx < 0 || state->fromClubIdx < 0) {
        context.setFooterLine("Player not found");
        return;
    }

    int myClubIdx = gameData.manager[0].club_idx;
    if (state->fromClubIdx == myClubIdx) {
        context.setFooterLine("Player already in your squad");
        return;
    }

    ClubRecord &myClub = getClub(myClubIdx);
    ClubRecord &fromClub = getClub(state->fromClubIdx);

    if (game_utils::findEmptySlot(myClub) == -1) {
        context.setFooterLine("No free slot in your squad");
        return;
    }

    if (state->fee > 0 && myClub.bank_account < state->fee) {
        context.setFooterLine("Insufficient funds");
        return;
    }

    for (int slot = 0; slot < 24; ++slot) {
        if (fromClub.player_index[slot] == state->playerIdx) {
            fromClub.player_index[slot] = -1;
            break;
        }
    }

    int destSlot = game_utils::findEmptySlot(myClub);
    if (destSlot == -1) {
        context.setFooterLine("No free slot in your squad");
        return;
    }

    myClub.player_index[destSlot] = state->playerIdx;
    myClub.bank_account -= state->fee;
    fromClub.bank_account += state->fee;

    PlayerRecord &player = getPlayer(state->playerIdx);
    player.period = static_cast<uint8_t>(state->weeks * kTurnsPerWeek);
    player.period_type = static_cast<uint8_t>(kLoanPeriodType);

    context.setFooterLine("Player is loaned");
}

void startLoanFlow(ScreenContext &context, const club_player &playerInfo) {
    if (!context.startReadingTextInput || !context.endReadingTextInput || !context.currentTextInput ||
        !context.addKeyPressCallback || !context.resetKeyPressCallbacks || !context.setFooterLine) {
        return;
    }

    auto state = std::make_shared<LoanState>();
    state->playerInfo = playerInfo;
    state->playerIdx = game_utils::findPlayerIndex(playerInfo.player);
    if (state->playerIdx >= 0) {
        state->fromClubIdx = game_utils::findClubIndexForPlayer(state->playerIdx);
    }

    if (state->playerIdx < 0 || state->fromClubIdx < 0) {
        context.setFooterLine("Player not found");
        return;
    }

    context.resetKeyPressCallbacks();
    context.endReadingTextInput();

    context.startReadingTextInput([&context]() {
        const char *buffer = context.currentTextInput();
        std::string input = buffer ? buffer : "";
        std::string footer = "           Weeks [1-36] " + (input.empty() ? "0" : input);
        context.setFooterLine(footer.c_str());
    });

    context.setFooterLine("           Weeks [1-36] 0");

    context.addKeyPressCallback(SDLK_RETURN, [state, &context]() {
        int weeks = std::atoi(context.currentTextInput());
        if (weeks < 1 || weeks > kMaxLoanWeeks) {
            context.setFooterLine("Weeks must be 1-36");
            return;
        }

        state->weeks = weeks;
        state->fee = weeks * state->playerInfo.player.wage;

        context.endReadingTextInput();
        context.resetKeyPressCallbacks();

        std::string feeText = game_utils::formatCurrency(state->fee);
        std::string confirm =
            "           Club is asking for additional £" + feeText + " (" + std::to_string(weeks) + "w) (Y/N)";
        context.setFooterLine(confirm.c_str());

        context.addKeyPressCallback('y', [state, &context]() {
            finalizeLoan(context, state);
        });
        context.addKeyPressCallback('Y', [state, &context]() {
            finalizeLoan(context, state);
        });
        context.addKeyPressCallback('n', [&context]() { clearInput(context); });
        context.addKeyPressCallback('N', [&context]() { clearInput(context); });
    });

    context.addKeyPressCallback(SDLK_ESCAPE, [&context]() { clearInput(context); });
}

void startLoanOrBuyFlow(ScreenContext &context, const club_player &playerInfo) {
    if (!context.addKeyPressCallback || !context.resetKeyPressCallbacks || !context.setFooterLine) {
        if (context.makeOffer) {
            context.makeOffer(playerInfo);
        }
        return;
    }

    context.resetKeyPressCallbacks();
    context.setFooterLine("           Loan or buy [L/B]?");

    context.addKeyPressCallback('b', [&context, playerInfo]() {
        if (context.makeOffer) {
            context.makeOffer(playerInfo);
        }
    });
    context.addKeyPressCallback('B', [&context, playerInfo]() {
        if (context.makeOffer) {
            context.makeOffer(playerInfo);
        }
    });
    context.addKeyPressCallback('l', [&context, playerInfo]() { startLoanFlow(context, playerInfo); });
    context.addKeyPressCallback('L', [&context, playerInfo]() { startLoanFlow(context, playerInfo); });
    context.addKeyPressCallback(SDLK_ESCAPE, [&context]() { clearInput(context); });
}
} // namespace

void ScoutScreen::draw(bool attachClickCallbacks) {
    context.writeHeader("SCOUT", 1, nullptr);

    if (context.selectedDivision() == -1) {
        context.writeDivisionsMenu("CHOOSE DIVISION TO SCOUT", attachClickCallbacks);
    } else if (context.selectedClub() == -1) {
        context.writeClubMenu("CHOOSE TEAM TO SCOUT", attachClickCallbacks);
    } else {
        ClubRecord &club = getClub(context.selectedClub());
        std::vector<club_player> players{};

        for (int i = 0; i < 24; ++i) {
            if (club.player_index[i] == -1) {
                continue;
            }
            PlayerRecord &p = getPlayer(club.player_index[i]);
            players.push_back(club_player{club, p});
        }

        int textLine = 4;
        context.writePlayers(players, textLine, attachClickCallbacks ? [this](const club_player &playerInfo) {
            startLoanOrBuyFlow(context, playerInfo);
        } : std::function<void(const club_player &)>{});

        context.writeText(
                "« Back",
                16,
                Colors::TEXT_1,
                TEXT_TYPE_SMALL,
                attachClickCallbacks
                ? std::function<void(void)>{ [this] {
                    context.resetSelection();
                    context.resetClickableAreas();
                    context.setClickableAreasConfigured(false);
                }}
                : nullptr,
                0
        );
    }
}
