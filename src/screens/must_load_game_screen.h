// Must-load screen shown when a game is required.
#pragma once

#include "screen.h"

class MustLoadGameScreen : public Screen {
public:
    explicit MustLoadGameScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw([[maybe_unused]] bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
