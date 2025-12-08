// Graphics subsystem: SDL/renderer/window lifecycle, cursors, and common draw helpers.
#pragma once

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <string>

class Graphics {
public:
    Graphics() = default;
    ~Graphics();

    void initialize();
    void createWindowAndRenderer(const char *title, int width, int height);
    void cleanup();

    SDL_Renderer *getRenderer() const { return renderer; }
    SDL_Window *getWindow() const { return window; }

    void configureCursors(const char *standardCursorImagePath, const char *leftClickCursorImagePath,
                          const char *rightClickCursorImagePath);
    void setStandardCursor();
    void setLeftClickCursor();
    void setRightClickCursor();

    SDL_Texture *createRenderTarget(int width, int height);
    bool drawBackground(const char *screenImagePath, int screenWidth, int screenHeight);
    void getRendererOutputSize(int &w, int &h) const;

private:
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;

    SDL_Cursor *standardCursor{};
    SDL_Cursor *leftClickCursor{};
    SDL_Cursor *rightClickCursor{};
};
