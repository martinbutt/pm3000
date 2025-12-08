#include <iostream>

#include "text.h"

int main() {
    // Verify Colors exist and default text colors alternate.
    SDL_Color even = Colors::TEXT_1;
    SDL_Color odd = Colors::TEXT_2;
    if (even.a == 0 || odd.a == 0) {
        return 1;
    }

    // Smoke test TextRenderer construction with null renderer (no rendering invoked).
    TextRenderer renderer(nullptr, nullptr);
    SDL_Color c0 = renderer.getDefaultTextColor(0);
    SDL_Color c1 = renderer.getDefaultTextColor(1);
    if (c0.r != even.r || c1.r != odd.r) {
        return 1;
    }
    return 0;
}
