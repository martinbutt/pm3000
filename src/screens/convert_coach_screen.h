// Convert player to coach screen.
#pragma once

#include "screen.h"

class ConvertCoachScreen : public Screen {
public:
    explicit ConvertCoachScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw(bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
