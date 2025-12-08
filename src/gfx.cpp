#include "gfx.h"

#include <stdexcept>

Graphics::~Graphics() {
    cleanup();
}

void Graphics::initialize() {
    if (SDL_Init(SDL_INIT_VIDEO & SDL_INIT_NOPARACHUTE) != 0) {
        throw std::runtime_error("Could not init SDL\nSDL_Init Error: " + std::string(SDL_GetError()));
    }

    int flags = IMG_INIT_PNG;
    if ((IMG_Init(flags) & flags) != flags) {
        throw std::runtime_error("Could not init SDL_Image\nSDL_Image Error: " + std::string(IMG_GetError()));
    }

    if (TTF_Init() != 0) {
        throw std::runtime_error("Could not init SDL_TTF\nSDL_Image Error: " + std::string(TTF_GetError()));
    }
}

void Graphics::createWindowAndRenderer(const char *title, int width, int height) {
    window = SDL_CreateWindow(title,
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              width,
                              height,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        throw std::runtime_error("SDL_CreateWindow Error: " + std::string(SDL_GetError()));
    }
    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (renderer == nullptr) {
        throw std::runtime_error("SDL_CreateRenderer Error: " + std::string(SDL_GetError()));
    }
}

void Graphics::cleanup() {
    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    SDL_Quit();
    IMG_Quit();
    TTF_Quit();
}

void Graphics::configureCursors(const char *standardCursorImagePath, const char *leftClickCursorImagePath,
                                const char *rightClickCursorImagePath) {
    SDL_Surface *standardCursorSurface = IMG_Load(standardCursorImagePath);

    if (!standardCursorSurface) {
        throw std::runtime_error("Unable to load image '" + std::string(standardCursorImagePath) + "'\nSDL_image Error: " +
                                 std::string(IMG_GetError()));
    }

    standardCursor = SDL_CreateColorCursor(standardCursorSurface, 0, 0);
    if (!standardCursor) {
        throw std::runtime_error("Unable to set cursor\nSDL_Image Error: " + std::string(IMG_GetError()));
    }

    SDL_SetCursor(standardCursor);

    SDL_Surface *leftClickCursorSurface = IMG_Load(leftClickCursorImagePath);

    if (!leftClickCursorSurface) {
        throw std::runtime_error("Unable to load image '" + std::string(leftClickCursorImagePath) + "'\nSDL_image Error: " +
                                 std::string(IMG_GetError()));
    }

    leftClickCursor = SDL_CreateColorCursor(leftClickCursorSurface, 0, 0);
    if (!leftClickCursor) {
        throw std::runtime_error("Unable to set cursor\nSDL_Image Error: " + std::string(IMG_GetError()));
    }

    SDL_Surface *rightClickCursorSurface = IMG_Load(rightClickCursorImagePath);

    if (!rightClickCursorSurface) {
        throw std::runtime_error("Unable to load image '" + std::string(rightClickCursorImagePath) + "'\nSDL_image Error: " +
                                 std::string(IMG_GetError()));
    }

    rightClickCursor = SDL_CreateColorCursor(rightClickCursorSurface, 0, 0);
    if (!rightClickCursor) {
        throw std::runtime_error("Unable to set cursor\nSDL_Image Error: " + std::string(IMG_GetError()));
    }
}

void Graphics::setStandardCursor() {
    if (standardCursor) {
        SDL_SetCursor(standardCursor);
    }
}

void Graphics::setLeftClickCursor() {
    if (leftClickCursor) {
        SDL_SetCursor(leftClickCursor);
    }
}

void Graphics::setRightClickCursor() {
    if (rightClickCursor) {
        SDL_SetCursor(rightClickCursor);
    }
}

SDL_Texture *Graphics::createRenderTarget(int width, int height) {
    return SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
}

bool Graphics::drawBackground(const char *screenImagePath, int screenWidth, int screenHeight) {

    SDL_Texture *screen = IMG_LoadTexture(renderer, screenImagePath);
    if (!screen) {
        throw std::runtime_error("Unable to load image '" + std::string(screenImagePath) + "'\nSDL_image Error: " +
                                 std::string(IMG_GetError()));
    }

    int w, h;
    SDL_QueryTexture(screen, nullptr, nullptr, &w, &h);

    SDL_Rect screenRect;

    if (w > h) {
        screenRect.w = screenWidth;
        screenRect.h = h * screenRect.w / w;
    } else {
        screenRect.h = screenHeight;
        screenRect.w = w * screenRect.h / h;
    }

    screenRect.x = screenWidth / 2 - screenRect.w / 2;
    screenRect.y = screenHeight / 2 - screenRect.h / 2;

    SDL_RenderCopy(renderer, screen, nullptr, &screenRect);

    return true;
}

void Graphics::getRendererOutputSize(int &w, int &h) const {
    if (renderer == nullptr) {
        return;
    }

    SDL_GetRendererOutputSize(renderer, &w, &h);
}
