// Save game screen.
#pragma once

#include "screen.h"

class SaveGameScreen : public Screen {
public:
    explicit SaveGameScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw(bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
