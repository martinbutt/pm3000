// Input handling: clickable areas, keypress callbacks, and text input orchestration.
#pragma once

#include <SDL.h>
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

#include "gfx.h"

enum class ClickableAreaType {
    Persistent,
    Transient,
};

class InputHandler {
public:
    explicit InputHandler(Graphics &gfxRef);

    void addClickableArea(int x, int y, int w, int h, std::function<void(void)> callback, ClickableAreaType type);
    void resetTransientClickableAreas();
    void checkClickableArea(Sint32 x, Sint32 y);

    void addKeyPressCallback(SDL_Keycode key, std::function<void(void)> callback);
    void resetKeyPressCallbacks();
    void checkKeyPressCallback(SDL_Keycode key);

    void startReadingTextInput(std::function<void(void)> callback);
    void endReadingTextInput();
    bool isReadingTextInput() const;
    const char *getTextInput() const;
    bool handleTextInputEvent(const SDL_Event &event);

private:
    struct ClickableArea {
        int x, y, w, h;
        std::function<void(void)> callback;
    };

    Graphics &gfx;
    std::vector<ClickableArea> persistentClickableAreas;
    std::vector<ClickableArea> transientClickableAreas;
    std::unordered_map<SDL_Keycode, std::function<void(void)>> keyPressCallbacks;

    bool readingTextInput = false;
    char textInput[13]{};
    std::function<void(void)> textInputCallback;
};
