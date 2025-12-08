// My team screen.
#pragma once

#include "screen.h"

class MyTeamScreen : public Screen {
public:
    explicit MyTeamScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw(bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
