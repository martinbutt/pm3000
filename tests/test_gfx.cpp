#include <iostream>

#include "gfx.h"

int main() {
    // SDL_Init guard: skip if video not available.
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL init failed, skipping gfx smoke test\n";
        return 0;
    }

    Graphics gfx;
    // We can't create a window in headless CI reliably; just ensure cleanup works.
    gfx.cleanup();
    SDL_Quit();
    return 0;
}
