#include "scout_screen.h"

#include "text.h"
#include "game_utils.h"

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
            context.makeOffer(playerInfo);
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
