// First time screen.
#pragma once

#include "screen.h"

class FirstTimeScreen : public Screen {
public:
    explicit FirstTimeScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw([[maybe_unused]] bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
