#include "change_team_screen.h"

#include "text.h"
#include "game_utils.h"

void ChangeTeamScreen::draw(bool attachClickCallbacks) {
    context.writeHeader("CHANGE TEAM", 1, nullptr);

    if (context.selectedDivision() == -1) {
        context.writeDivisionsMenu("CHOOSE DIVISION", attachClickCallbacks);

    } else if (context.selectedClub() == -1) {
        context.writeClubMenu("CHOOSE CLUB", attachClickCallbacks);

    } else {
        ClubRecord &club = getClub(context.selectedClub());
        char clubText[34];
        snprintf(clubText, sizeof(clubText), "Club changed to %16.16s", club.name);

        changeClub(context.selectedClub(), context.gamePath().c_str(), 0);
        context.writeText(clubText, 8, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr, 0);

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
