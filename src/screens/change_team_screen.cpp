#include "change_team_screen.h"

#include <cstring>
#include <string>

#include "text.h"
#include "game_utils.h"

namespace {
void confirmChangeTeam(ScreenContext &context, const std::string &label,
                       const std::function<void(void)> &action,
                       const std::function<void(void)> &cancel) {
    if (!context.addKeyPressCallback || !context.resetKeyPressCallbacks || !context.setFooterLine) {
        action();
        return;
    }

    context.resetKeyPressCallbacks();
    const std::string prompt = "            Change team to " + label + "? (Y/N)";
    context.setFooterLine(prompt.c_str());

    auto clearPrompt = [&context]() {
        context.resetKeyPressCallbacks();
        context.setFooterLine("");
    };

    auto runAction = [clearPrompt, action]() {
        clearPrompt();
        action();
    };

    auto runCancel = [clearPrompt, cancel]() {
        clearPrompt();
        cancel();
    };

    context.addKeyPressCallback('y', runAction);
    context.addKeyPressCallback('Y', runAction);
    context.addKeyPressCallback('n', runCancel);
    context.addKeyPressCallback('N', runCancel);
}
} // namespace

void ChangeTeamScreen::draw(bool attachClickCallbacks) {
    context.writeHeader("CHANGE TEAM", 1, nullptr);

    auto clearSelection = [this]() {
        context.resetSelection();
        context.resetClickableAreas();
        context.setClickableAreasConfigured(false);
        if (context.resetKeyPressCallbacks) {
            context.resetKeyPressCallbacks();
        }
        if (context.setFooterLine) {
            context.setFooterLine("");
        }
        pendingClubIdx = -1;
        changeApplied = false;
    };

    if (context.selectedDivision() == -1) {
        pendingClubIdx = -1;
        changeApplied = false;
        context.writeDivisionsMenu("CHOOSE DIVISION", attachClickCallbacks);

    } else if (context.selectedClub() == -1) {
        pendingClubIdx = -1;
        changeApplied = false;
        context.writeClubMenu("CHOOSE CLUB", attachClickCallbacks);

    } else {
        int selectedClub = context.selectedClub();

        if (pendingClubIdx != selectedClub) {
            pendingClubIdx = selectedClub;
            changeApplied = false;
        }

        ClubRecord &club = getClub(selectedClub);
        char clubText[34];

        if (!changeApplied) {
            snprintf(clubText, sizeof(clubText), "Change team to %16.16s", club.name);
            context.writeText(clubText, 8, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr, 0);

            if (attachClickCallbacks) {
                std::string clubName(club.name, strnlen(club.name, sizeof(club.name)));
                confirmChangeTeam(context, clubName,
                                  [this, selectedClub]() {
                                      changeClub(selectedClub, context.gamePath(), 0);
                                      changeApplied = true;
                                  },
                                  clearSelection);
            }
        } else {
            snprintf(clubText, sizeof(clubText), "Club changed to %16.16s", club.name);
            context.writeText(clubText, 8, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr, 0);
        }

        context.writeText(
                "« Back",
                16,
                Colors::TEXT_1,
                TEXT_TYPE_SMALL,
                attachClickCallbacks
                ? std::function<void(void)>{ clearSelection }
                : nullptr,
                0
        );
    }
}
