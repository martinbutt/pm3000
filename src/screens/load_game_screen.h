// Load game screen.
#pragma once

#include "screen.h"

class LoadGameScreen : public Screen {
public:
    explicit LoadGameScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw(bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
