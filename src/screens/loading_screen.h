// Loading screen.
#pragma once

#include "screen.h"

class LoadingScreen : public Screen {
public:
    explicit LoadingScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw([[maybe_unused]] bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
