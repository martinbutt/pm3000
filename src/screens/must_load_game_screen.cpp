#include "must_load_game_screen.h"

void MustLoadGameScreen::draw([[maybe_unused]] bool attachClickCallbacks) {
    context.writeTextLarge("Load a game to start", 4, nullptr);
}
