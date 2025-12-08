// Free players screen.
#pragma once

#include "screen.h"

class FreePlayersScreen : public Screen {
public:
    explicit FreePlayersScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw(bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
