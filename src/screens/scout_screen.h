// Scout screen.
#pragma once

#include "screen.h"

class ScoutScreen : public Screen {
public:
    explicit ScoutScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw(bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
