// Test font screen.
#pragma once

#include "screen.h"

class TestFontScreen : public Screen {
public:
    explicit TestFontScreen(const ScreenContext &ctx) : context(ctx) {}
    void draw([[maybe_unused]] bool attachClickCallbacks) override;

private:
    ScreenContext context;
};
