#include "first_time_screen.h"

void FirstTimeScreen::draw([[maybe_unused]] bool attachClickCallbacks) {
    context.writeTextLarge("Configure PM3 path in settings", 4, nullptr);
}
