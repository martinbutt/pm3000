// Change team screen.
#pragma once

#include "screen.h"

class ChangeTeamScreen : public Screen {
public:
    explicit ChangeTeamScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw(bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
