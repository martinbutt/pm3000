#include "text.h"

#include <algorithm>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <cstring>

const SDL_Color Colors::GOALKEEPER = {34, 170, 68, 255};
const SDL_Color Colors::DEFENDER = {170, 204, 68, 255};
const SDL_Color Colors::MIDFIELDER = {255, 238, 136, 255};
const SDL_Color Colors::ATTACKER = {255, 136, 0, 255};
const SDL_Color Colors::TEXT_HEADING = {236, 196, 25, 255};
const SDL_Color Colors::TEXT_SUB_HEADING = {52, 166, 230, 255};
const SDL_Color Colors::TEXT_1 = {236, 204, 85, 255};
const SDL_Color Colors::TEXT_2 = {221, 153, 68, 255};
const SDL_Color Colors::TEXT_TOP_DETAILS = {75, 65, 75, 255};

TextRenderer::TextRenderer(SDL_Renderer *rendererIn, ClickHandler clickHandler)
        : renderer(rendererIn), addClickableArea(std::move(clickHandler)) {}

void TextRenderer::loadFont(const char *path, int type) {
    TTF_Font *font = TTF_OpenFont(path, textTypes[type].size);
    if (!font) {
        throw std::runtime_error("Could not open font: " + std::string(TTF_GetError()));
    }
    textTypes[type].font = font;
}

void TextRenderer::renderText(const std::string &text, const SDL_Color &color, int x, int y, int w,
                              textJustification justification, int textType,
                              const std::function<void(void)> &clickCallback, bool attachCallback) {
    std::string textFormatted(text);
    std::transform(textFormatted.begin(), textFormatted.end(), textFormatted.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    SDL_Surface *textSurface = TTF_RenderUTF8_Blended_Wrapped(textTypes[textType].font, textFormatted.c_str(), color,
                                                              w);

    if (!textSurface) {
        throw std::runtime_error("Unable to create surface. SDL_ttf Error: " + std::string(TTF_GetError()));
    }

    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        throw std::runtime_error("Unable to create texture. SDL Error: " + std::string(SDL_GetError()));
    }

    if (justification == TEXT_JUSTIFICATION_CENTER) {
        x = (SCREEN_WIDTH / 2) - (textSurface->w / 2);
    }

    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);

    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);

    if (attachCallback && clickCallback && addClickableArea) {
        addClickableArea(textRect.x, textRect.y, textRect.w, textRect.h, clickCallback);
    }
}

void TextRenderer::writeText(const char *text, int textLine, SDL_Color textColor, int textType,
                             const std::function<void(void)> &clickCallback, int offsetLeft) {
    int x = MARGIN_LEFT + offsetLeft;
    int y = ((textTypes[textType].size + TEXT_LINE_SPACING) * textLine) + MARGIN_TOP + textTypes[textType].offsetTop;

    renderText(text, textColor, x, y, SCREEN_WIDTH,
               textTypes[textType].justification, textType, clickCallback, true);
}

SDL_Color TextRenderer::getDefaultTextColor(int textLine) const {
    SDL_Color textColor;

    if (textLine % 2 == 0) {
        textColor = Colors::TEXT_1;
    } else {
        textColor = Colors::TEXT_2;
    }

    return textColor;
}

void TextRenderer::addTextBlock(const char *text, int x, int y, int w, SDL_Color color, int textType,
                                const std::function<void(void)> &clickCallback) {
    textBlockStruct text_block{text, x, y, w, color, textType, clickCallback};
    textBlocks.push_back(text_block);
}

void TextRenderer::drawTextBlocks(bool attachClickCallbacks) {
    for (const auto &textBlock: textBlocks) {
        renderText(textBlock.text, textBlock.color, textBlock.x, textBlock.y, textBlock.w,
                   TEXT_JUSTIFICATION_LEFT,
                   textBlock.textType, textBlock.clickCallback, attachClickCallbacks);
    }
}

void TextRenderer::resetTextBlocks() {
    textBlocks = {};
}

TTF_Font *TextRenderer::getFont(int textType) const {
    auto it = textTypes.find(textType);
    if (it == textTypes.end()) {
        return nullptr;
    }
    return it->second.font;
}

namespace text_utils {

void loadFont(TextRenderer &renderer, const char *path, int type) {
    try {
        renderer.loadFont(path, type);
    } catch (const std::exception &ex) {
        throw std::runtime_error("Could not open font\nSDL_TTF Error: " + std::string(ex.what()));
    }
}

void renderText(TextRenderer &renderer, const std::string &text, const SDL_Color &color, int x, int y, int w,
                textJustification justification, int textType, const std::function<void(void)> &clickCallback,
                bool attachCallback) {
    renderer.renderText(text, color, x, y, w, justification, textType, clickCallback, attachCallback);
}

void writeText(TextRenderer &renderer, const char *text, int textLine, SDL_Color textColor, int textType,
               const std::function<void(void)> &clickCallback, int offsetLeft) {
    renderer.writeText(text, textLine, textColor, textType, clickCallback, offsetLeft);
}

SDL_Color defaultTextColor(const TextRenderer &renderer, int textLine) {
    return renderer.getDefaultTextColor(textLine);
}

void addTextBlock(TextRenderer &renderer, const char *text, int x, int y, int w, SDL_Color color, int textType,
                  const std::function<void(void)> &clickCallback) {
    renderer.addTextBlock(text, x, y, w, color, textType, clickCallback);
}

void drawTextBlocks(TextRenderer &renderer, bool attachClickCallbacks) {
    renderer.drawTextBlocks(attachClickCallbacks);
}

void resetTextBlocks(TextRenderer &renderer) {
    renderer.resetTextBlocks();
}

void writeHeader(TextRenderer &renderer, const char *text, const std::function<void(void)> &clickCallback) {
    renderer.writeText(text, 1, Colors::TEXT_HEADING, TEXT_TYPE_HEADER, clickCallback, 0);
}

void writeSubHeader(TextRenderer &renderer, const char *text, const std::function<void(void)> &clickCallback) {
    renderer.writeText(text, 2, Colors::TEXT_SUB_HEADING, TEXT_TYPE_SMALL, clickCallback, 0);
}

void writePlayerSubHeader(TextRenderer &renderer, const char *text, int textLine,
                          const std::function<void(void)> &clickCallback) {
    renderer.writeText(text, textLine, Colors::TEXT_SUB_HEADING, TEXT_TYPE_PLAYER, clickCallback, 0);
}

void writePlayer(TextRenderer &renderer, const char *text, char playerPosition, int textLine,
                 const std::function<void(void)> &clickCallback) {
    SDL_Color textColor;
    switch (playerPosition) {
        case 'G':
            textColor = Colors::GOALKEEPER;
            break;
        case 'D':
            textColor = Colors::DEFENDER;
            break;
        case 'M':
            textColor = Colors::MIDFIELDER;
            break;
        case 'A':
            textColor = Colors::ATTACKER;
            break;
        default:
            textColor = Colors::TEXT_1;
    }

    renderer.writeText(text, textLine, textColor, TEXT_TYPE_PLAYER, clickCallback, 0);
}

void writeTextLarge(TextRenderer &renderer, const char *text, int textLine,
                    const std::function<void(void)> &clickCallback) {
    if (std::strlen(text) > 30) {
        throw std::runtime_error("Text too long: '" + std::string(text) + "'");
    }
    renderer.writeText(text, textLine, renderer.getDefaultTextColor(textLine), TEXT_TYPE_LARGE, clickCallback, 0);
}

void writeTextSmall(TextRenderer &renderer, const char *text, int textLine,
                    const std::function<void(void)> &clickCallback, int offsetLeft) {
    renderer.writeText(text, textLine, renderer.getDefaultTextColor(textLine), TEXT_TYPE_SMALL, clickCallback,
                       offsetLeft);
}

int writePlayers(TextRenderer &renderer, std::vector<club_player> &players, int &textLine,
                 const std::function<void(const club_player &)> &clickCallback) {
    writePlayerSubHeader(renderer,
                         "CLUB NAME        T PLAYER NAME  HN TK PS SH HD CR FT F M A AG WAGES",
                         3,
                         nullptr);

    for (auto &player: players) {
        char playerRow[77];
        snprintf(playerRow, sizeof(playerRow),
                 "%16.16s %1c %12.12s %2.2d %2.2d %2.2d %2.2d %2.2d %2.2d %2.2d %1.1s %1.1d %1.1d %2.2d %5d",
                 player.club.name, determinePlayerType(player.player), player.player.name, player.player.hn,
                 player.player.tk, player.player.ps, player.player.sh, player.player.hd, player.player.cr,
                 player.player.ft, footShortLabels[player.player.foot], player.player.morl, player.player.aggr,
                 player.player.age, player.player.wage);

        std::function<void(void)> playerCallback;
        if (clickCallback) {
            playerCallback = [player, clickCallback] { clickCallback(player); };
        }

        writePlayer(renderer,
                    playerRow,
                    determinePlayerType(player.player),
                    textLine++,
                    playerCallback);
    }

    return textLine;
}

} // namespace text_utils
