#include <SDL.h>
#include <iostream>

#include "input.h"
#include "gfx.h"

int main() {
    // Minimal SDL init for constructing Graphics/InputHandler.
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL init failed\n";
        return 0; // skip quietly if not available
    }
    Graphics gfx;
    InputHandler input(gfx);

    // Clickable area hit/miss.
    bool clicked = false;
    input.addClickableArea(10, 10, 5, 5, [&] { clicked = true; }, ClickableAreaType::Transient);
    input.checkClickableArea(12, 12);
    if (!clicked) return 1;
    clicked = false;
    input.checkClickableArea(0, 0);
    if (clicked) return 1;

    // Key press callbacks.
    bool keyTriggered = false;
    input.addKeyPressCallback(SDLK_a, [&] { keyTriggered = true; });
    input.checkKeyPressCallback(SDLK_a);
    if (!keyTriggered) return 1;
    keyTriggered = false;
    input.resetKeyPressCallbacks();
    input.checkKeyPressCallback(SDLK_a);
    if (keyTriggered) return 1;

    SDL_Quit();
    return 0;
}
