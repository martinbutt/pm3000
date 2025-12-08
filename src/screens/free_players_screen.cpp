#include "free_players_screen.h"

#include "config/constants.h"
#include "text.h"

#include <cmath>

void FreePlayersScreen::draw(bool attachClickCallbacks) {
    context.writeHeader("FREE PLAYERS", 1, nullptr);

    if (attachClickCallbacks) {
        context.refreshFreePlayers();
    }

    auto &players = context.freePlayersRef();

    if (players.empty()) {
        context.writeText("No free players found", 8, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr, 0);
        context.setPagination(0, 0);
        return;
    }

    int textLine = 4;
    int pageSize = 25;
    int currentPage = context.currentPage();
    int totalPages = 0;

    if (players.size() > 25) {
        pageSize = 24;
        totalPages = static_cast<int>(std::ceil(static_cast<double>(players.size()) / pageSize));
    }

    if (currentPage == 0) {
        currentPage = 1;
    }

    context.setPagination(currentPage, totalPages);

    int start = (currentPage - 1) * pageSize;
    long unsigned int end = pageSize * currentPage;
    end = end < players.size() ? end : players.size();

    std::vector<club_player> displayPlayers(players.begin() + start, players.begin() + end);

    context.writePlayers(displayPlayers, textLine, nullptr);
}
