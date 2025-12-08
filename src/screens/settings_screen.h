// Settings screen.
#pragma once

#include "screen.h"

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw(bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
