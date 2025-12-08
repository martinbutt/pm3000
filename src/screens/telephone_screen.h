// Telephone screen.
#pragma once

#include "screen.h"

class TelephoneScreen : public Screen {
public:
    explicit TelephoneScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw(bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
