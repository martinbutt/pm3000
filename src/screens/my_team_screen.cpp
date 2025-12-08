#include "my_team_screen.h"

#include "text.h"

void MyTeamScreen::draw([[maybe_unused]] bool attachClickCallbacks) {
    context.writeHeader("TEAM SQUAD", 1, nullptr);

    std::vector<club_player> myPlayers = getMyPlayers(0);

    if (myPlayers.empty()) {
        context.writeText("No players found", 8, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr, 0);
        return;
    }

    int textLine = 4;

    textLine = context.writePlayers(myPlayers, textLine, std::function<void(const club_player &)>{});

    for (int i = textLine; i <= 27; i++) {
        char playerRow[69] = "................ . ............ .. .. .. .. .. .. .. . . . .. .....";
        context.writeText(playerRow, i, Colors::TEXT_2, TEXT_TYPE_PLAYER, nullptr, 0);
    }
}
