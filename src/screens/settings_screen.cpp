#include "settings_screen.h"

#include "config/constants.h"
#include "pm3_defs.hh"
#include "text.h"

#include <cstring>
#include <array>

constexpr std::array<const char*, static_cast<size_t>(Pm3GameType::NumGameTypes)> gameTypeNames{
        "Unknown Edition",
        "Standard Edition",
        "Deluxe Edition"
};

void SettingsScreen::draw(bool attachClickCallbacks) {
    context.writeHeader("Settings", 1, nullptr);

    std::function<void(void)> folderClickCallback = attachClickCallbacks ? [this] { context.choosePm3Folder(); }
                                                                         : std::function<void(void)>{};

    char text[70] = "Click here to choose PM3 folder";
    if (!context.gamePath().empty()) {
        strncpy(text, context.gamePath().c_str(), (sizeof text - 1));
        text[69] = '\0';
    }

    context.writeText("PM3 Folder", 2, Colors::TEXT_2, TEXT_TYPE_SMALL, nullptr, 0);
    context.writeText(text, 3, Colors::TEXT_1, TEXT_TYPE_SMALL, folderClickCallback, 0);
    context.writeText(gameTypeNames[static_cast<size_t>(context.gameType())], 4, Colors::TEXT_1, TEXT_TYPE_SMALL,
                      folderClickCallback, 0);

    if (context.currentGame() != 0) {
        std::function<void(void)> levelAggressionClickCallback = attachClickCallbacks ? [this] {
            context.levelAggression();
            context.setFooter("AGGRESSION LEVELED");
        } : std::function<void(void)>{};

        context.writeText("LEVEL AGGRESSION", 6, Colors::TEXT_1, TEXT_TYPE_SMALL, levelAggressionClickCallback, 0);
        context.addTextBlock(
                "Aggression has a disproportionate influence of a team's chances of winning a match, making the game unfair. \"Level Aggression\" sets the aggression to 5 for all players on all teams to negate its affects, making the game fairer.",
                MARGIN_LEFT, 144, SCREEN_WIDTH - (MARGIN_LEFT * 2), Colors::TEXT_2, TEXT_TYPE_SMALL,
                levelAggressionClickCallback);
    }
}
