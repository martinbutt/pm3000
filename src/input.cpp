// Input handling implementation.
#include "input.h"

#include "config/constants.h"

#include <algorithm>
#include <cctype>
#include <cstring>

InputHandler::InputHandler(Graphics &gfxRef) : gfx(gfxRef) {
    textInput[0] = '\0';
}

void InputHandler::addClickableArea(int x, int y, int w, int h, std::function<void(void)> callback,
                                    ClickableAreaType type) {
    std::vector<ClickableArea> &target =
            type == ClickableAreaType::Persistent ? persistentClickableAreas : transientClickableAreas;
    target.push_back(ClickableArea{x, y, w, h, std::move(callback)});
}

void InputHandler::resetTransientClickableAreas() {
    transientClickableAreas.clear();
}

void InputHandler::checkClickableArea(Sint32 x, Sint32 y) {
    int w = SCREEN_WIDTH;
    int h = SCREEN_HEIGHT;
    // Fallback to logical screen size when no renderer exists (test/headless paths).
    gfx.getRendererOutputSize(w, h);
    if (w <= 0 || h <= 0) {
        w = SCREEN_WIDTH;
        h = SCREEN_HEIGHT;
    }

    x = static_cast<Sint32>((SCREEN_WIDTH / static_cast<float>(w)) * x);
    y = static_cast<Sint32>((SCREEN_HEIGHT / static_cast<float>(h)) * y);

    for (auto it = transientClickableAreas.rbegin(); it != transientClickableAreas.rend(); ++it) {
        if (x > it->x && x < it->x + it->w && y > it->y && y < it->y + it->h) {
            if (it->callback) {
                it->callback();
            }
            return;
        }
    }

    for (auto it = persistentClickableAreas.rbegin(); it != persistentClickableAreas.rend(); ++it) {
        if (x > it->x && x < it->x + it->w && y > it->y && y < it->y + it->h) {
            if (it->callback) {
                it->callback();
            }
            return;
        }
    }
}

void InputHandler::addKeyPressCallback(SDL_Keycode key, std::function<void(void)> callback) {
    keyPressCallbacks[key] = std::move(callback);
}

void InputHandler::resetKeyPressCallbacks() {
    keyPressCallbacks.clear();
}

void InputHandler::checkKeyPressCallback(SDL_Keycode key) {
    auto it = keyPressCallbacks.find(key);
    if (it != keyPressCallbacks.end()) {
        auto callback = it->second;
        callback();
    }
}

void InputHandler::startReadingTextInput(std::function<void(void)> callback) {
    addKeyPressCallback(SDLK_ESCAPE, [this] {
        resetKeyPressCallbacks();
        endReadingTextInput();
    });

    textInput[0] = '\0';
    SDL_StartTextInput();
    readingTextInput = true;
    textInputCallback = std::move(callback);
}

void InputHandler::endReadingTextInput() {
    SDL_StopTextInput();
    readingTextInput = false;
    textInput[0] = '\0';
}

bool InputHandler::isReadingTextInput() const {
    return readingTextInput;
}

const char *InputHandler::getTextInput() const {
    return textInput;
}

bool InputHandler::handleTextInputEvent(const SDL_Event &event) {
    if (!readingTextInput) {
        return false;
    }

    if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_BACKSPACE) {
        size_t len = strlen(textInput);
        if (len > 0) {
            textInput[len - 1] = '\0';
            if (textInputCallback) {
                textInputCallback();
            }
        }
        return true;
    }

    if (event.type == SDL_TEXTINPUT) {
        const char *incomingText = event.text.text;
        size_t incomingLen = strlen(incomingText);
        size_t currentLen = strlen(textInput);
        bool numeric = std::all_of(incomingText, incomingText + incomingLen, [](unsigned char c) {
            return std::isdigit(c);
        });

        if (numeric && (currentLen + incomingLen) < sizeof(textInput)) {
            strcat(textInput, incomingText);
            if (textInputCallback) {
                textInputCallback();
            }
        }
        return true;
    }

    return false;
}
