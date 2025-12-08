// Text subsystem: colors, fonts, and rendering helpers.
#pragma once

#include <SDL.h>
#include <SDL_ttf.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "config/constants.h"
#include "game_utils.h"

class Colors {
public:
    static const SDL_Color GOALKEEPER;
    static const SDL_Color DEFENDER;
    static const SDL_Color MIDFIELDER;
    static const SDL_Color ATTACKER;
    static const SDL_Color TEXT_HEADING;
    static const SDL_Color TEXT_SUB_HEADING;
    static const SDL_Color TEXT_1;
    static const SDL_Color TEXT_2;
    static const SDL_Color TEXT_TOP_DETAILS;
};

enum textJustification {
    TEXT_JUSTIFICATION_LEFT,
    TEXT_JUSTIFICATION_CENTER
};

struct textBlockStruct {
    std::string text;
    int x, y, w;
    SDL_Color color;
    int textType;
    std::function<void(void)> clickCallback;
};

class TextRenderer {
public:
    using ClickHandler = std::function<void(int, int, int, int, const std::function<void(void)> &)>;

    TextRenderer(SDL_Renderer *renderer, ClickHandler clickHandler);

    void loadFont(const char *path, int type);

    void renderText(const std::string &text, const SDL_Color &color, int x, int y, int w,
                    textJustification justification, int textType,
                    const std::function<void(void)> &clickCallback, bool attachCallback);

    void writeText(const char *text, int textLine, SDL_Color textColor, int textType,
                   const std::function<void(void)> &clickCallback, int offsetLeft);

    SDL_Color getDefaultTextColor(int textLine) const;

    void addTextBlock(const char *text, int x, int y, int w, SDL_Color color, int textType,
                      const std::function<void(void)> &clickCallback);

    void drawTextBlocks(bool attachClickCallbacks);

    void resetTextBlocks();

    TTF_Font *getFont(int textType) const;

private:
    struct TextType {
        int size;
        int offsetTop;
        TTF_Font *font;
        textJustification justification;
    };

    SDL_Renderer *renderer;
    ClickHandler addClickableArea;
    std::map<int, TextType> textTypes = {
            {TEXT_TYPE_HEADER, TextType{32, -28, nullptr, TEXT_JUSTIFICATION_CENTER}},
            {TEXT_TYPE_LARGE,  TextType{32, 0, nullptr, TEXT_JUSTIFICATION_LEFT}},
            {TEXT_TYPE_SMALL,  TextType{16, 0, nullptr, TEXT_JUSTIFICATION_LEFT}},
            {TEXT_TYPE_PLAYER, TextType{8, 8, nullptr, TEXT_JUSTIFICATION_LEFT}}
    };
    std::vector<textBlockStruct> textBlocks{};
};

namespace text_utils {

void writeHeader(TextRenderer &renderer, const char *text, const std::function<void(void)> &clickCallback);

void writeSubHeader(TextRenderer &renderer, const char *text, const std::function<void(void)> &clickCallback);

void writePlayerSubHeader(TextRenderer &renderer, const char *text, int textLine,
                          const std::function<void(void)> &clickCallback);

void writePlayer(TextRenderer &renderer, const char *text, char playerPosition, int textLine,
                 const std::function<void(void)> &clickCallback);

void writeTextLarge(TextRenderer &renderer, const char *text, int textLine,
                    const std::function<void(void)> &clickCallback);

void writeTextSmall(TextRenderer &renderer, const char *text, int textLine,
                    const std::function<void(void)> &clickCallback, int offsetLeft);

int writePlayers(TextRenderer &renderer, std::vector<club_player> &players, int &textLine,
                 const std::function<void(const club_player &)> &clickCallback);

void loadFont(TextRenderer &renderer, const char *path, int type);
void renderText(TextRenderer &renderer, const std::string &text, const SDL_Color &color, int x, int y, int w,
                textJustification justification, int textType, const std::function<void(void)> &clickCallback,
                bool attachCallback);
void writeText(TextRenderer &renderer, const char *text, int textLine, SDL_Color textColor, int textType,
               const std::function<void(void)> &clickCallback, int offsetLeft);
SDL_Color defaultTextColor(const TextRenderer &renderer, int textLine);
void addTextBlock(TextRenderer &renderer, const char *text, int x, int y, int w, SDL_Color color, int textType,
                  const std::function<void(void)> &clickCallback);
void drawTextBlocks(TextRenderer &renderer, bool attachClickCallbacks);
void resetTextBlocks(TextRenderer &renderer);

} // namespace text_utils
