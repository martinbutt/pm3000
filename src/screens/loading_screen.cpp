#include "loading_screen.h"

#include "config/constants.h"

void LoadingScreen::draw([[maybe_unused]] bool attachClickCallbacks) {
    context.drawBackground(LOADING_SCREEN_IMAGE_PATH);
}
