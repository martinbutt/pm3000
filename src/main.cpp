#include <iostream>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <functional>
#include "nfd.h"
#include "pm3.hh"
#include <unistd.h>
#include <map>
#include <filesystem>
#include <fstream>
#include <bitset>
#include <random>

#define TEXT_TYPE_HEADER   0
#define TEXT_TYPE_LARGE    1
#define TEXT_TYPE_SMALL    2
#define TEXT_TYPE_PLAYER   3

#define TEXT_LINE_SPACING  2

#define SCREEN_WIDTH     640
#define SCREEN_HEIGHT    400
#define MARGIN_LEFT       40
#define MARGIN_TOP        19

#define ICON_WIDTH        48
#define ICON_HEIGHT       37
#define ICON_LEFT_MARGIN  23
#define ICON_TOP_MARGIN  355
#define ICON_SPACING      13

#define SCREEN_IMAGE_PATH             "assets/screen.png"
#define LOADING_SCREEN_IMAGE_PATH     "assets/loading.png"
#define CURSOR_STANDARD_IMAGE_PATH    "assets/cursor.png"
#define CURSOR_CLICK_LEFT_IMAGE_PATH  "assets/cursor-click-left.png"
#define CURSOR_CLICK_RIGHT_IMAGE_PATH "assets/cursor-click-right.png"
#define ICON_LOAD_IMAGE_PATH          "assets/icon-load.png"
#define ICON_SAVE_IMAGE_PATH          "assets/icon-save.png"
#define ICON_FREE_PLAYERS_IMAGE_PATH  "assets/icon-free-players.png"
#define ICON_MY_TEAM_IMAGE_PATH       "assets/icon-my-team.png"
#define ICON_SCOUT_IMAGE_PATH         "assets/icon-scout.png"
#define ICON_CHANGE_TEAM_IMAGE_PATH   "assets/icon-change-team.png"
#define ICON_TELEPHONE_IMAGE_PATH     "assets/icon-telephone.png"
#define ICON_CONVERT_COACH_IMAGE_PATH "assets/icon-convert-coach.png"
#define ICON_SETTINGS_IMAGE_PATH      "assets/icon-settings.png"

#define HEADER_FONT_PATH              "assets/acknowtt.ttf"
#define TALL_FONT_PATH                "assets/unscii-16.ttf"
#define SHORT_FONT_PATH               "assets/unscii-8.ttf"

#define PREFS_PATH                    "PM3000.PREFS"
#define BACKUP_SAVE_PATH              "PM3000"

typedef enum {
    LOADING_SCREEN,
    FIRST_TIME_GAME_SCREEN,
    MUST_LOAD_GAME_SCREEN,
    SETTINGS_SCREEN,
    LOAD_GAME_SCREEN,
    SAVE_GAME_SCREEN,
    FREE_PLAYERS_SCREEN,
    MY_TEAM_SCREEN,
    SCOUT_SCREEN,
    CHANGE_TEAM_SCREEN,
    TELEPHONE_SCREEN,
    CONVERT_COACH_SCREEN,
    TEST_SCREEN
} screen;

typedef enum {
    PERSISTENT_CLICKABLE_AREA_TYPE,
    TRANSIENT_CLICKABLE_AREA_TYPE,
} clickableAreaType;

typedef enum {
    TEXT_JUSTIFICATION_LEFT,
    TEXT_JUSTIFICATION_CENTER
} textJustification;

const char *gameTypeNames[NUM_GAME_TYPES] = {"Unknown Edition", "Standard Edition", "Deluxe Edition"};

const int saveGameSizes[3] = {29554, 139080, 157280};

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

const SDL_Color Colors::GOALKEEPER = {34, 170, 68, 255};
const SDL_Color Colors::DEFENDER = {170, 204, 68, 255};
const SDL_Color Colors::MIDFIELDER = {255, 238, 136, 255};
const SDL_Color Colors::ATTACKER = {255, 136, 0, 255};
const SDL_Color Colors::TEXT_HEADING = {236, 196, 25, 255};
const SDL_Color Colors::TEXT_SUB_HEADING = {52, 166, 230, 255};
const SDL_Color Colors::TEXT_1 = {236, 204, 85, 255};
const SDL_Color Colors::TEXT_2 = {221, 153, 68, 255};
const SDL_Color Colors::TEXT_TOP_DETAILS = {75, 65, 75, 255};

class Application {
public:
    Application() : keyPressCallbacks(256) {
        initializeSDL();
    };

    ~Application();

    void run();

private:
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;

    bool quit = false;

    struct TextType {
        int size;
        int offsetTop;
        TTF_Font *font;
        textJustification justification;
    };

    std::map<int, TextType> textTypes = {
            {TEXT_TYPE_HEADER, TextType{32, -28, nullptr, TEXT_JUSTIFICATION_CENTER}},
            {TEXT_TYPE_LARGE,  TextType{32, 0, nullptr, TEXT_JUSTIFICATION_LEFT}},
            {TEXT_TYPE_SMALL,  TextType{16, 0, nullptr, TEXT_JUSTIFICATION_LEFT}},
            {TEXT_TYPE_PLAYER, TextType{8, 8, nullptr, TEXT_JUSTIFICATION_LEFT}}
    };

    SDL_Cursor *standardCursor{};
    SDL_Cursor *leftClickCursor{};
    SDL_Cursor *rightClickCursor{};

    std::bitset<8> saveFiles{};

    std::vector<club_player> freePlayers{};

    struct settingsStruct {
        std::filesystem::path gamePath = "";
        pm3_game_type gameType = PM3_UNKNOWN;

        void serialize(std::ostream &out) const {
            std::string pathStr = gamePath.string();
            size_t length = pathStr.size();
            out.write(reinterpret_cast<const char *>(&length), sizeof(length));
            out.write(pathStr.c_str(), length);
        }

        void deserialize(std::istream &in) {
            size_t length;
            in.read(reinterpret_cast<char *>(&length), sizeof(length));
            std::string pathStr(length, '\0');
            in.read(pathStr.data(), length);
            gamePath = pathStr;
        }
    };

    struct settingsStruct settings{};

    struct iconStruct {
        SDL_Rect rect;
        SDL_Texture *texture;
        std::function<void(void)> callback;
    };

    struct iconStruct icons[9]{};
    int iconsIdx = 0;

    struct clickableAreaStruct {
        int x, y, w, h;
        std::function<void(void)> callback;

        void reset() {
            x = 0;
            y = 0;
            w = 0;
            h = 0;
            callback = nullptr;
        }
    };

    struct clickableAreaStruct persistentClickableAreas[40]{};
    struct clickableAreaStruct transientClickableAreas[40]{};
    int persistentClickableAreasIdx = 0;
    int transientClickableAreasIdx = 0;
    bool clickableAreasConfigured = false;

    struct keyPressCallbackStruct {
        std::function<void(void)> callback;

        void reset() {
            callback = nullptr;
        }
    };

    std::vector<keyPressCallbackStruct> keyPressCallbacks;

    screen currentScreen = LOADING_SCREEN;

    int currentGame = 0;

    int currentPage = 0;
    int totalPages = 0;

    int selectedDivision = -1;
    int selectedClub = -1;

    using screenCallback = std::function<void(bool)>;

    std::map<int, screenCallback> screenCallbacks = {
            {FIRST_TIME_GAME_SCREEN, [this](bool attachClickCallbacks) { firstTimeScreen(attachClickCallbacks); }},
            {MUST_LOAD_GAME_SCREEN,  [this](bool attachClickCallbacks) { mustLoadGameScreen(attachClickCallbacks); }},
            {SETTINGS_SCREEN,        [this](bool attachClickCallbacks) { settingsScreen(attachClickCallbacks); }},
            {LOAD_GAME_SCREEN,       [this](bool attachClickCallbacks) { loadGameScreen(attachClickCallbacks); }},
            {SAVE_GAME_SCREEN,       [this](bool attachClickCallbacks) { saveGameScreen(attachClickCallbacks); }},
            {FREE_PLAYERS_SCREEN,    [this](bool attachClickCallbacks) { freePlayersScreen(attachClickCallbacks); }},
            {MY_TEAM_SCREEN,         [this](bool attachClickCallbacks) { myTeamScreen(attachClickCallbacks); }},
            {SCOUT_SCREEN,           [this](bool attachClickCallbacks) { scoutScreen(attachClickCallbacks); }},
            {CHANGE_TEAM_SCREEN,     [this](bool attachClickCallbacks) { changeTeamScreen(attachClickCallbacks); }},
            {TELEPHONE_SCREEN,       [this](bool attachClickCallbacks) { telephoneScreen(attachClickCallbacks); }},
            {CONVERT_COACH_SCREEN,       [this](bool attachClickCallbacks) { convertCoachScreen(attachClickCallbacks); }},
            {TEST_SCREEN,            [this](bool attachClickCallbacks) { testFontScreen(attachClickCallbacks); }},
    };

    struct textBlockStruct {
        std::string text;
        int x, y, w;
        SDL_Color color;
        int textType;
        std::function<void(void)> clickCallback;
    };

    std::vector<textBlockStruct> textBlocks{};

    char footer[70]{};

    void initializeSDL();

    void cleanupSDL();

    void changeScreen(screen newScreen);

    void writeHeader(const char *headerText, const std::function<void(void)> &clickCallback);

    void writeSubHeader(const char *headerText, const std::function<void(void)> &clickCallback);

    void writePlayerSubHeader(const char *headerText, int textLine, const std::function<void(void)> &clickCallback);

    void writePlayer(const char *playerText, char playerPosition, int textLine,
                     const std::function<void(void)> &clickCallback);

    void writeTextLarge(const char *text, int textLine, const std::function<void(void)> &clickCallback);

    void writeTextSmall(const char *text, int textLine, const std::function<void(void)> &clickCallback, int offsetLeft);

    void drawTextBlocks(bool attachClickCallbacks);

    static SDL_Color getDefaultTextColor(int textLine);

    void writeText(const char *text, int textLine, SDL_Color textColor, int textType,
                   const std::function<void(void)> &clickCallback, int offsetLeft);

    bool drawBackground(const char *screenImagePath);

    void configureCursors(const char *standardCursorImagePath, const char *leftClickCursorImagePath,
                          const char *rightClickCursorImagePath);

    void addIcon(const char *iconImagePath, int iconPosition, const std::function<void(void)> &clickCallback);

    bool drawIcons();

    void checkClickableArea(Sint32 x, Sint32 y);

    void
    addClickableArea(int x, int y, int w, int h, std::function<void(void)> callback, clickableAreaType type);

    void resetClickableAreas();

    void testButtonCallback();

    void choosePm3Folder();

    void loadGame(int gameNumber);

    void loadGameConfirm(int gameNumber);

    void saveGame(int gameNumber);

    void saveGameConfirm(int gameNumber);

    void drawCurrentScreen();

    void loadingScreen();

    void firstTimeScreen([[maybe_unused]] bool attachClickCallbacks);

    void mustLoadGameScreen([[maybe_unused]] bool attachClickCallbacks);

    void settingsScreen(bool attachClickCallbacks);

    void loadGameScreen(bool attachClickCallbacks);

    void saveGameScreen(bool attachClickCallbacks);

    void myTeamScreen([[maybe_unused]] bool attachClickCallbacks);

    void testFontScreen([[maybe_unused]] bool attachClickCallbacks);

    void drawPagination(bool attachClickCallbacks);

    void loadPrefs();

    void savePrefs();

    bool checkSaveFileExists(int gameNumber, char gameLetter);

    void memoizeSaveFiles();

    void freePlayersScreen(bool attachClickCallbacks);

    void scoutScreen(bool attachClickCallbacks);

    void changeTeamScreen(bool attachClickCallbacks);

    void telephoneScreen(bool attachClickCallbacks);

    int calculateStadiumValue(struct gamea::manager &manager);

    void toggleWindowed();

    void addTextBlock(const char *text, int x, int y, int w, SDL_Color color, int textType,
                      const std::function<void(void)> &clickCallback);

    void resetTextBlocks();

    void drawTopDetails();

    void writeDivisionsMenu(const char *heading, bool attachClickCallbacks);

    void writeClubMenu(const char *heading, bool attachClickCallbacks);

    int writePlayers(std::vector<club_player> &players, int &textLine);

    void renderText(const std::string &text, const SDL_Color &color, int x, int y, int w,
                    textJustification justification, TTF_Font *font,
                    const std::function<void(void)> &clickCallback, bool attachCallback);

    [[noreturn]] static void exitError(const std::string &errorMessage);

    void loadFont(const char *path, int type);

    bool backupSaveFile(int number);

    void checkGamePathSettings();

    void ensureMetadataLoaded(bool attachClickCallbacks);

    static void formatSaveGameLabel(int i, char *gameLabel, size_t gameLabelSize);

    void checkKeyPressCallback(char key);

    void addKeyPressCallback(char key, std::function<void(void)> callback);

    void resetKeyPressCallbacks();

    void convertCoachScreen(bool attachClickCallbacks);

    void convertPlayerToCoach(struct gamea::manager &manager, struct gameb::club &club, int8_t clubPlayerIdx);
};

bool windowed = true;

Application::~Application() {
    cleanupSDL();
}

void Application::initializeSDL() {
    if (SDL_Init(SDL_INIT_VIDEO & SDL_INIT_NOPARACHUTE) != 0) {
        exitError("Could not init SDL\nSDL_Init Error: " + std::string(SDL_GetError()));
    }
    SDL_SetRelativeMouseMode(SDL_TRUE);

    int flags = IMG_INIT_PNG;
    if ((IMG_Init(flags) & flags) != flags) {
        exitError("Could not init SDL_Image\nSDL_Image Error: " + std::string(IMG_GetError()));
    }

    if (TTF_Init() != 0) {
        exitError("Could not init SDL_TTF\nSDL_Image Error: " + std::string(TTF_GetError()));
    }

    loadFont(HEADER_FONT_PATH, TEXT_TYPE_HEADER);
    loadFont(TALL_FONT_PATH, TEXT_TYPE_LARGE);
    loadFont(TALL_FONT_PATH, TEXT_TYPE_SMALL);
    loadFont(SHORT_FONT_PATH, TEXT_TYPE_PLAYER);

    Application::loadPrefs();
    settings.gameType = get_pm3_game_type(settings.gamePath.c_str());

#if defined linux && SDL_VERSION_ATLEAST(2, 0, 8)
    // Disable compositor bypass
    if (!SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0")) {
        exitError("SDL can not disable compositor bypass!");
    }
#endif

    window = SDL_CreateWindow("Premier Manager 3000",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              SCREEN_WIDTH,
                              SCREEN_HEIGHT,
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        exitError("SDL_CreateWindow Error: " + std::string(SDL_GetError()));
    }
    renderer = SDL_CreateRenderer(window, -1,
                                  SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_TARGETTEXTURE);
    if (renderer == nullptr) {
        exitError("SDL_CreateRenderer Error: " + std::string(SDL_GetError()));
    }
}

void Application::loadFont(const char *path, int type) {
    TTF_Font *font = TTF_OpenFont(path, textTypes[type].size);
    if (!font) {
        exitError("Could not open font\nSDL_TTF Error: " + std::string(TTF_GetError()));
    }
    textTypes[type].font = font;
}

void Application::cleanupSDL() {
    if (renderer != nullptr) {
        SDL_DestroyRenderer(renderer);
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
    }

    SDL_Quit();
    IMG_Quit();
    TTF_Quit();
}

void Application::run() {
    SDL_Event event;

    Application::loadingScreen();
    SDL_RenderPresent(renderer);
    sleep(3);

    std::random_device rd;
    std::srand(rd());

    if (settings.gamePath.empty()) {
        Application::changeScreen(FIRST_TIME_GAME_SCREEN);
    } else {
        Application::changeScreen(MUST_LOAD_GAME_SCREEN);
    }

    Application::configureCursors(CURSOR_STANDARD_IMAGE_PATH, CURSOR_CLICK_LEFT_IMAGE_PATH,
                                  CURSOR_CLICK_RIGHT_IMAGE_PATH);
    Application::addClickableArea(572, 358, 48, 25, [this] { quit = true; }, PERSISTENT_CLICKABLE_AREA_TYPE);
    Application::addIcon(ICON_LOAD_IMAGE_PATH, 1, [this] { changeScreen(LOAD_GAME_SCREEN); });
    Application::addIcon(ICON_SAVE_IMAGE_PATH, 2, [this] { changeScreen(SAVE_GAME_SCREEN); });
    Application::addIcon(ICON_CHANGE_TEAM_IMAGE_PATH, 3, [this] { changeScreen(CHANGE_TEAM_SCREEN); });
    Application::addIcon(ICON_MY_TEAM_IMAGE_PATH, 4, [this] { changeScreen(MY_TEAM_SCREEN); });
    Application::addIcon(ICON_SCOUT_IMAGE_PATH, 5, [this] { changeScreen(SCOUT_SCREEN); });
    Application::addIcon(ICON_FREE_PLAYERS_IMAGE_PATH, 6, [this] { changeScreen(FREE_PLAYERS_SCREEN); });
    Application::addIcon(ICON_CONVERT_COACH_IMAGE_PATH, 7, [this] { changeScreen(CONVERT_COACH_SCREEN); });
    Application::addIcon(ICON_TELEPHONE_IMAGE_PATH, 8, [this] { changeScreen(TELEPHONE_SCREEN); });
//    Application::addIcon(ICON_SETTINGS_IMAGE_PATH, 8, [this] { testButtonCallback(); });
    Application::addIcon(ICON_SETTINGS_IMAGE_PATH, 9, [this] { changeScreen(SETTINGS_SCREEN); });

    SDL_RenderPresent(renderer);

    SDL_Texture *texTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                               SCREEN_WIDTH, SCREEN_HEIGHT);

    while (!quit) {
        // Event handling loop
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || this->quit) {
                quit = true;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == 1) {
                    SDL_SetCursor(leftClickCursor);
                } else {
                    SDL_SetCursor(rightClickCursor);
                }
                Application::checkClickableArea(event.button.x, event.button.y);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                SDL_SetCursor(standardCursor);
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_f:
                        toggleWindowed();
                        break;
                    case SDLK_q:
                        quit = true;
                        break;
                    default:
                        checkKeyPressCallback(event.key.keysym.sym);
                }
            }

            SDL_SetRenderTarget(renderer, texTarget);
            SDL_RenderClear(renderer);

            Application::drawCurrentScreen();

            SDL_SetRenderTarget(renderer, nullptr);
            SDL_RenderClear(renderer);
            SDL_RenderCopyEx(renderer, texTarget, nullptr, nullptr, 0, nullptr, SDL_FLIP_NONE);
            SDL_RenderPresent(renderer);
        }
        SDL_Delay(16);
    }
}

void Application::writeHeader(const char *headerText, const std::function<void(void)> &clickCallback) {
    writeText(headerText, 1, Colors::TEXT_HEADING, TEXT_TYPE_HEADER, clickCallback, 0);
}

void Application::writeSubHeader(const char *headerText, const std::function<void(void)> &clickCallback) {
    writeText(headerText, 2, Colors::TEXT_SUB_HEADING, TEXT_TYPE_SMALL, clickCallback, 0);
}

void Application::writePlayerSubHeader(const char *headerText, int textLine,
                                       const std::function<void(void)> &clickCallback) {
    writeText(headerText, textLine, Colors::TEXT_SUB_HEADING, TEXT_TYPE_PLAYER, clickCallback, 0);
}

void
Application::writePlayer(const char *playerText, const char playerPosition, int textLine,
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

    Application::writeText(playerText, textLine, textColor, TEXT_TYPE_PLAYER, clickCallback, 0);
}

void Application::writeTextLarge(const char *text, int textLine, const std::function<void(void)> &clickCallback) {
    if (strlen(text) > 30) {
        exitError("Text too long: '" + std::string(text) + "'");
    }
    Application::writeText(text, textLine, getDefaultTextColor(textLine), TEXT_TYPE_LARGE, clickCallback,
                           0);
}

void Application::writeTextSmall(const char *text, int textLine, const std::function<void(void)> &clickCallback,
                                 int offsetLeft) {
    Application::writeText(text, textLine, getDefaultTextColor(textLine), TEXT_TYPE_SMALL, clickCallback,
                           offsetLeft);
}

void Application::addTextBlock(const char *text, int x, int y, int w, SDL_Color color, int textType,
                               const std::function<void(void)> &clickCallback) {
    textBlockStruct text_block{text, x, y, w, color, textType, clickCallback};
    textBlocks.push_back(text_block);
}

void Application::renderText(const std::string &text, const SDL_Color &color, int x, int y, int w,
                             textJustification justification, TTF_Font *font,
                             const std::function<void(void)> &clickCallback = nullptr, bool attachCallback = false) {
    std::string textFormatted(text);
    std::transform(textFormatted.begin(), textFormatted.end(), textFormatted.begin(),
                   [](unsigned char c) { return std::toupper(c); });

    SDL_Surface *textSurface = TTF_RenderUTF8_Blended_Wrapped(font, textFormatted.c_str(), color, w);

    if (!textSurface) {
        exitError("Unable to create surface. SDL_ttf Error: " + std::string(TTF_GetError()));
    }

    SDL_Texture *textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        exitError("Unable to create texture. SDL Error: " + std::string(SDL_GetError()));
    }

    if (justification == TEXT_JUSTIFICATION_CENTER) {
        x = (SCREEN_WIDTH / 2) - (textSurface->w / 2);
    }

    SDL_Rect textRect = {x, y, textSurface->w, textSurface->h};
    SDL_RenderCopy(renderer, textTexture, nullptr, &textRect);

    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);

    if (attachCallback && clickCallback) {
        Application::addClickableArea(textRect.x, textRect.y, textRect.w, textRect.h, clickCallback,
                                      TRANSIENT_CLICKABLE_AREA_TYPE);
    }
}

void Application::drawTextBlocks(bool attachClickCallbacks) {
    for (const auto &textBlock: textBlocks) {
        renderText(textBlock.text, textBlock.color, textBlock.x, textBlock.y, textBlock.w,
                   TEXT_JUSTIFICATION_LEFT,
                   textTypes[textBlock.textType].font, textBlock.clickCallback, attachClickCallbacks);
    }
}

void Application::writeText(const char *text, int textLine, SDL_Color textColor, int textType,
                            const std::function<void(void)> &clickCallback, int offsetLeft) {
    int x = MARGIN_LEFT + offsetLeft;
    int y = ((textTypes[textType].size + TEXT_LINE_SPACING) * textLine) + MARGIN_TOP + textTypes[textType].offsetTop;

    renderText(text, textColor, x, y, SCREEN_WIDTH,
               textTypes[textType].justification, textTypes[textType].font, clickCallback, true);
}

SDL_Color Application::getDefaultTextColor(int textLine) {
    SDL_Color textColor;

    if (textLine % 2 == 0) {
        textColor = Colors::TEXT_1;
    } else {
        textColor = Colors::TEXT_2;
    }

    return textColor;
}


bool Application::drawBackground(const char *screenImagePath) {

    SDL_Texture *screen = IMG_LoadTexture(renderer, screenImagePath);
    if (!screen) {
        exitError("Unable to load image '" + std::string(screenImagePath) + "'\nSDL_image Error: " +
                  std::string(IMG_GetError()));
    }

    int w, h;
    SDL_QueryTexture(screen, nullptr, nullptr, &w, &h);

    SDL_Rect screenRect;

    if (w > h) {
        screenRect.w = SCREEN_WIDTH;
        screenRect.h = h * screenRect.w / w;
    } else {
        screenRect.h = SCREEN_HEIGHT;
        screenRect.w = w * screenRect.h / h;
    }

    screenRect.x = SCREEN_WIDTH / 2 - screenRect.w / 2;
    screenRect.y = SCREEN_HEIGHT / 2 - screenRect.h / 2;

    SDL_RenderCopy(renderer, screen, nullptr, &screenRect);

    return true;
}

void Application::configureCursors(const char *standardCursorImagePath, const char *leftClickCursorImagePath,
                                   const char *rightClickCursorImagePath) {
    SDL_Surface *standardCursorSurface = IMG_Load(standardCursorImagePath);

    if (!standardCursorSurface) {
        exitError("Unable to load image '" + std::string(standardCursorImagePath) + "'\nSDL_image Error: " +
                  std::string(IMG_GetError()));
    }

    standardCursor = SDL_CreateColorCursor(standardCursorSurface, 0, 0);
    if (!standardCursor) {
        exitError("Unable to set cursor\nSDL_Image Error: " + std::string(IMG_GetError()));
    }

    SDL_SetCursor(standardCursor);

    SDL_Surface *leftClickCursorSurface = IMG_Load(leftClickCursorImagePath);

    if (!leftClickCursorSurface) {
        exitError("Unable to load image '" + std::string(leftClickCursorImagePath) + "'\nSDL_image Error: " +
                  std::string(IMG_GetError()));
    }

    leftClickCursor = SDL_CreateColorCursor(leftClickCursorSurface, 0, 0);
    if (!leftClickCursor) {
        exitError("Unable to set cursor\nSDL_Image Error: " + std::string(IMG_GetError()));

    }

    SDL_Surface *rightClickCursorSurface = IMG_Load(rightClickCursorImagePath);

    if (!rightClickCursorSurface) {
        exitError("Unable to load image '" + std::string(rightClickCursorImagePath) + "'\nSDL_image Error: " +
                  std::string(IMG_GetError()));
    }

    rightClickCursor = SDL_CreateColorCursor(rightClickCursorSurface, 0, 0);
    if (!rightClickCursor) {
        exitError("Unable to set cursor\nSDL_Image Error: " + std::string(IMG_GetError()));
    }
}

[[noreturn]] void Application::exitError(const std::string &errorMessage) {
    std::cout << "An error occurred: " << errorMessage << std::endl;
    exit(1);
}

void Application::addIcon(const char *iconImagePath, int iconPosition, const std::function<void(void)> &clickCallback) {

    if (iconPosition > 9) {
        exitError("Unable to draw icon\nMaximum icon position 9. Got " + std::to_string(iconPosition) + "!");
    }

    SDL_Texture *iconTexture = IMG_LoadTexture(renderer, iconImagePath);
    if (!iconTexture) {
        exitError("Unable to load image '" + std::string(iconImagePath) + "'\nSDL_image Error: " +
                  std::string(IMG_GetError()));
    }

    int w, h;
    SDL_QueryTexture(iconTexture, nullptr, nullptr, &w, &h);

    SDL_Rect iconRect;

    iconRect.h = ICON_HEIGHT;
    iconRect.w = w * iconRect.h / h;

    iconRect.x = ICON_LEFT_MARGIN + ((iconPosition - 1) * (ICON_WIDTH + ICON_SPACING));
    iconRect.y = ICON_TOP_MARGIN;

    icons[iconsIdx].rect = iconRect;
    icons[iconsIdx].texture = iconTexture;
    icons[iconsIdx].callback = clickCallback;

    iconsIdx++;

    Application::addClickableArea(iconRect.x, iconRect.y, iconRect.w, iconRect.h, clickCallback,
                                  PERSISTENT_CLICKABLE_AREA_TYPE);
}

bool Application::drawIcons() {
    for (int i = 0; i < iconsIdx; i++) {
        SDL_RenderCopy(renderer, icons[i].texture, nullptr, &icons[i].rect);
    }

    return true;
}

void Application::checkClickableArea(Sint32 x, Sint32 y) {

    int w, h;

    SDL_GetRendererOutputSize(renderer, &w, &h);

    x = (SCREEN_WIDTH / (float) w) * x;
    y = (SCREEN_HEIGHT / (float) h) * y;

    for (int i = transientClickableAreasIdx; i >= 0; i--) {
        if (x > transientClickableAreas[i].x && x < transientClickableAreas[i].x + transientClickableAreas[i].w &&
            y > transientClickableAreas[i].y && y < transientClickableAreas[i].y + transientClickableAreas[i].h) {
            transientClickableAreas[i].callback();
            return;
        }
    }

    for (int i = persistentClickableAreasIdx; i >= 0; i--) {
        if (x > persistentClickableAreas[i].x && x < persistentClickableAreas[i].x + persistentClickableAreas[i].w &&
            y > persistentClickableAreas[i].y && y < persistentClickableAreas[i].y + persistentClickableAreas[i].h) {
            persistentClickableAreas[i].callback();
            return;
        }
    }
}

void
Application::addClickableArea(int x, int y, int w, int h, std::function<void(void)> callback, clickableAreaType type) {
    struct clickableAreaStruct *clickableAreaPtr;
    if (type == PERSISTENT_CLICKABLE_AREA_TYPE) {
        clickableAreaPtr = &persistentClickableAreas[persistentClickableAreasIdx];
        persistentClickableAreasIdx++;
    } else {
        clickableAreaPtr = &transientClickableAreas[transientClickableAreasIdx];
        transientClickableAreasIdx++;
    }
    clickableAreaPtr->x = x;
    clickableAreaPtr->y = y;
    clickableAreaPtr->w = w;
    clickableAreaPtr->h = h;
    clickableAreaPtr->callback = std::move(callback);
}

void Application::resetClickableAreas() {
    for (auto &clickableArea: transientClickableAreas) {
        clickableArea.reset();
    }

    transientClickableAreasIdx = 0;
}

void Application::checkKeyPressCallback(char key) {
    if (key > 0 && keyPressCallbacks[key].callback) {
        keyPressCallbacks[key].callback();
    }
}

void
Application::addKeyPressCallback(char key, std::function<void(void)> callback) {
    struct keyPressCallbackStruct keyPressCallback;
    keyPressCallback.callback = std::move(callback);
    keyPressCallbacks[key] = keyPressCallback;
}

void Application::resetKeyPressCallbacks() {
    for (auto &keyPressCallback: keyPressCallbacks) {
        keyPressCallback.reset();
    }
}

void Application::testButtonCallback() {
    Application::changeScreen(TEST_SCREEN);
}

void Application::choosePm3Folder() {
    NFD_Init();

    nfdchar_t *outPath;

    nfdresult_t result = NFD_PickFolder(&outPath, settings.gamePath.empty() ? nullptr : settings.gamePath.c_str());
    if (result == NFD_OKAY) {
        settings.gameType = get_pm3_game_type(outPath);
        settings.gamePath = outPath;
        NFD_FreePath(outPath);
        Application::savePrefs();
        memoizeSaveFiles();
    } else {
        exitError("Unable to select folder\nNFD Error: " + std::string(NFD_GetError()));
    }

    NFD_Quit();
}

void Application::loadGame(int gameNumber) {
    std::string gameaName = construct_save_file_path(settings.gamePath, gameNumber, 'A');
    std::string gamebName = construct_save_file_path(settings.gamePath, gameNumber, 'B');
    std::string gamecName = construct_save_file_path(settings.gamePath, gameNumber, 'C');

    if (std::filesystem::file_size(gameaName) != saveGameSizes[0]) {
        sprintf(footer, "INVALID %s FILESIZE", gameaName.c_str());
        return;
    } else if (std::filesystem::file_size(gamebName) != saveGameSizes[1]) {
        sprintf(footer, "INVALID %s FILESIZE", gamebName.c_str());
        return;
    } else if (std::filesystem::file_size(gamecName) != saveGameSizes[2]) {
        sprintf(footer, "INVALID %s FILESIZE", gamecName.c_str());
        return;
    }

    load_binaries(gameNumber, settings.gamePath);

    currentGame = gameNumber;
    sprintf(footer, "GAME %d LOADED", gameNumber);
}

void Application::loadGameConfirm(int gameNumber) {
    sprintf(footer, "Load Game %d: Are you sure?", gameNumber);
    addKeyPressCallback('y', [this, gameNumber] {loadGame(gameNumber); resetKeyPressCallbacks();});
    addKeyPressCallback('Y', [this, gameNumber] {loadGame(gameNumber); resetKeyPressCallbacks();});
    addKeyPressCallback('n', [this] {footer[0] = '\0'; resetKeyPressCallbacks();});
    addKeyPressCallback('N', [this] {footer[0] = '\0'; resetKeyPressCallbacks();});
}

void Application::saveGame(int gameNumber) {
    if (backupSaveFile(gameNumber)) {
        update_metadata(gameNumber);
        save_binaries(gameNumber, settings.gamePath);
        save_metadata(settings.gamePath);
        sprintf(footer, "GAME %d SAVED", gameNumber);
    } else {
        sprintf(footer, "ERROR SAVING: COULDN'T BACKUP SAVE GAME %d", gameNumber);
    }
}

void Application::saveGameConfirm(int gameNumber) {
    sprintf(footer, "Save Game %d: Are you sure?", gameNumber);
    addKeyPressCallback('y', [this, gameNumber] {saveGame(gameNumber); resetKeyPressCallbacks();});
    addKeyPressCallback('Y', [this, gameNumber] {saveGame(gameNumber); resetKeyPressCallbacks();});
    addKeyPressCallback('n', [this] {footer[0] = '\0'; resetKeyPressCallbacks();});
    addKeyPressCallback('N', [this] {footer[0] = '\0'; resetKeyPressCallbacks();});
}

void Application::changeScreen(screen newScreen) {
    if (currentGame == 0 && settings.gamePath.empty() && newScreen != SETTINGS_SCREEN) {
        newScreen = FIRST_TIME_GAME_SCREEN;
    } else if (currentGame == 0 && newScreen != LOAD_GAME_SCREEN && newScreen != SETTINGS_SCREEN) {
        newScreen = MUST_LOAD_GAME_SCREEN;
    }
    if (newScreen != currentScreen) {
        resetClickableAreas();
        resetKeyPressCallbacks();
        resetTextBlocks();
        selectedDivision = -1;
        selectedClub = -1;
        clickableAreasConfigured = false;
    }
    currentScreen = newScreen;
    footer[0] = '\0';
    currentPage = 0;
    totalPages = 0;
}

void Application::drawCurrentScreen() {
    drawBackground(SCREEN_IMAGE_PATH);
    drawIcons();
    if (currentGame) {
        drawTopDetails();
    }

    screenCallbacks[currentScreen](!clickableAreasConfigured);

    if (totalPages > 1) {
        drawPagination(!clickableAreasConfigured);
    }

    drawTextBlocks(!clickableAreasConfigured);

    if (strlen(footer)) {
        writeTextSmall(footer, 16, nullptr, 0);
    }

    clickableAreasConfigured = true;
}

void Application::loadingScreen() {
    Application::drawBackground(LOADING_SCREEN_IMAGE_PATH);
}

void Application::firstTimeScreen([[maybe_unused]] bool attachClickCallbacks) {
    Application::writeTextLarge("Configure PM3 path in settings", 4, nullptr);
}

void Application::mustLoadGameScreen([[maybe_unused]] bool attachClickCallbacks) {
    Application::writeTextLarge("Load a game to start", 4, nullptr);
}

void Application::settingsScreen(bool attachClickCallbacks) {
    Application::writeHeader("Settings", nullptr);

    std::function<void(void)> folderClickCallback = attachClickCallbacks ? [this] { choosePm3Folder(); } : std::function<void(void)>{};

    char text[70] = "Click here to choose PM3 folder";
    if (settings.gamePath != "") {
        strncpy(text, settings.gamePath.c_str(), (sizeof text - 1));
        text[69] = '\0';
    }

    Application::writeTextSmall("PM3 Folder", 2, nullptr, 0);
    Application::writeTextSmall(text, 3, folderClickCallback, 0);
    Application::writeTextSmall(gameTypeNames[settings.gameType], 4, folderClickCallback, 0);

    if (currentGame != 0) {
        std::function<void(void)> levelAggressionClickCallback = attachClickCallbacks ? [this] { level_aggression(); strcpy(footer, "AGGRESSION LEVELED"); } : std::function<void(void)>{};

        Application::writeTextSmall("LEVEL AGGRESSION", 6, levelAggressionClickCallback, 0);
        Application::addTextBlock(
                "Aggression has a disproportionate influence of a team's chances of winning a match, making the game unfair. \"Level Aggression\" sets the aggression to 5 for all players on all teams to negate its affects, making the game fairer.",
                MARGIN_LEFT, 144, SCREEN_WIDTH - (MARGIN_LEFT * 2), Colors::TEXT_2, TEXT_TYPE_SMALL,
                levelAggressionClickCallback);
    }

}

void Application::loadGameScreen(bool attachClickCallbacks) {
    checkGamePathSettings();

    ensureMetadataLoaded(attachClickCallbacks);

    Application::writeHeader("Load Game", nullptr);

    if (saveFiles.none()) {
        Application::writeTextSmall("No valid save files found", 2, nullptr, 0);
        return;
    }

    Application::writeSubHeader("Choose game to load", nullptr);

    for (int i = 1; i <= 8; i++) {
        if (!saveFiles.test(i - 1)) {
            continue;
        }

        char gameLabel[62];
        formatSaveGameLabel(i, gameLabel, sizeof(gameLabel));

        Application::writeTextSmall(
                gameLabel,
                i + 2,
                attachClickCallbacks ? std::function<void(void)>{ [this, i] { loadGameConfirm(i); } } : nullptr,
                0
        );
    }
}

void Application::saveGameScreen(bool attachClickCallbacks) {
    checkGamePathSettings();

    ensureMetadataLoaded(attachClickCallbacks);

    Application::writeHeader("Save Game", nullptr);

    if (saveFiles.none()) {
        Application::writeTextSmall("No valid save files found", 2, nullptr, 0);
        return;
    }

    Application::writeSubHeader("Choose game to save", nullptr);

    for (int i = 1; i <= 8; i++) {
        if (!saveFiles.test(i - 1)) {
            continue;
        }

        char gameLabel[62];
        formatSaveGameLabel(i, gameLabel, sizeof(gameLabel));

        Application::writeTextSmall(
                gameLabel,
                i + 2,
                attachClickCallbacks ? std::function<void(void)>{ [this, i] { saveGameConfirm(i); } } : nullptr,
                0
        );
    }
}

void Application::formatSaveGameLabel(int i, char *gameLabel, size_t gameLabelSize) {
    snprintf(gameLabel, gameLabelSize, "GAME %1.1d %16.16s %16.16s %3.3s Week %2.2d %4.4d", i,
             saves.game[i - 1].manager[0].name, get_club(saves.game[i - 1].manager[0].club_idx).name,
             day[saves.game[i - 1].turn % 3],
             (saves.game[i - 1].turn / 3) + 1, saves.game[i - 1].year);
}


void Application::checkGamePathSettings() {
    if (settings.gamePath == "") {
        writeTextLarge("Configure PM3 path in settings", 4, nullptr);
        return;
    }
    if (settings.gameType == PM3_UNKNOWN) {
        writeTextLarge("UNKNOWN PM3 GAME TYPE", 4, nullptr);
        return;
    }
}

void Application::ensureMetadataLoaded(bool attachClickCallbacks) {
    if (!attachClickCallbacks) {
        return;
    }
    memoizeSaveFiles();
    if (currentGame == 0) {
        load_default_clubdata(settings.gamePath);
    }
    load_metadata(settings.gamePath);
}

bool Application::checkSaveFileExists(int gameNumber, char gameLetter) {
    return std::filesystem::exists(construct_save_file_path(settings.gamePath, gameNumber, gameLetter));
}

void Application::memoizeSaveFiles() {
    for (int i = 1; i <= 8; i++) {
        saveFiles.set(i - 1, checkSaveFileExists(i, 'A') && checkSaveFileExists(i, 'B') && checkSaveFileExists(i, 'C'));
    }
}

void Application::freePlayersScreen(bool attachClickCallbacks) {
    Application::writeHeader("FREE PLAYERS", nullptr);

    if (attachClickCallbacks) {
        freePlayers = find_free_players();
    }

    if (find_free_players().empty()) {
        Application::writeTextSmall("No free players found", 8, nullptr, 0);
        return;
    }

    int textLine = 4;
    int pageSize = 25;
    currentPage = (currentPage == 0) ? 1 : currentPage;
    totalPages = 0;

    if (freePlayers.size() > 25) {
        pageSize = 24;
        totalPages = std::ceil(static_cast<double>(freePlayers.size()) / pageSize);
    }

    int start = (currentPage - 1) * pageSize;
    long unsigned int end = pageSize * currentPage;
    end = end < freePlayers.size() ? end : freePlayers.size();

    std::vector<club_player> displayPlayers(freePlayers.begin() + start, freePlayers.begin() + end);

    writePlayers(displayPlayers, textLine);
}

void Application::myTeamScreen([[maybe_unused]] bool attachClickCallbacks) {
    writeHeader("TEAM SQUAD", nullptr);

    std::vector<club_player> myPlayers = get_my_players(0);

    if (myPlayers.empty()) {
        writeTextSmall("No players found", 8, nullptr, 0);
        return;
    }

    int textLine = 4;

    textLine = writePlayers(myPlayers, textLine);

    for (int i = textLine; i <= 27; i++) {
        char playerRow[69] = "................ . ............ .. .. .. .. .. .. .. . . . .. .....";
        writeText(playerRow, i, Colors::TEXT_2, TEXT_TYPE_PLAYER, nullptr, 0);
    }
}

int Application::writePlayers(std::vector<club_player> &players, int &textLine) {
    writePlayerSubHeader("CLUB NAME        T PLAYER NAME  HN TK PS SH HD CR FT F M A AG WAGES", 3,
                         nullptr);

    for (auto &player: players) {
        char playerRow[77];
        snprintf(playerRow, sizeof(playerRow),
                 "%16.16s %1c %12.12s %2.2d %2.2d %2.2d %2.2d %2.2d %2.2d %2.2d %1.1s %1.1d %1.1d %2.2d %5d",
                 player.club.name, determine_player_type(player.player), player.player.name, player.player.hn,
                 player.player.tk, player.player.ps, player.player.sh, player.player.hd, player.player.cr,
                 player.player.ft, foot_short[player.player.foot], player.player.morl, player.player.aggr,
                 player.player.age, player.player.wage);
        writePlayer(playerRow, determine_player_type(player.player), textLine++, nullptr);
    }
    return textLine;
}

void Application::scoutScreen(bool attachClickCallbacks) {

    Application::writeHeader("SCOUT", nullptr);

    if (selectedDivision == -1) {
        writeDivisionsMenu("CHOOSE DIVISION TO SCOUT", attachClickCallbacks);
    } else if (selectedClub == -1) {
        writeClubMenu("CHOOSE TEAM TO SCOUT", attachClickCallbacks);
    } else {
        struct gameb::club &club = get_club(selectedClub);
        std::vector<club_player> players{};

        for (int i = 0; i < 24; ++i) {
            if (club.player_index[i] == -1) {
                continue;
            }
            struct gamec::player &p = get_player(club.player_index[i]);
            players.push_back(club_player{club, p});
        }

        int textLine = 4;
        Application::writePlayers(players, textLine);

        Application::writeTextSmall(
                "« Back",
                16,
                attachClickCallbacks
                ? std::function<void(void)>{ [this] {
                    selectedClub = -1;
                    resetClickableAreas();
                    clickableAreasConfigured = false;
                }}
                : nullptr,
                0
        );
    }
}

void Application::changeTeamScreen(bool attachClickCallbacks) {

    Application::writeHeader("CHANGE TEAM", nullptr);

    if (selectedDivision == -1) {
        writeDivisionsMenu("CHOOSE DIVISION", attachClickCallbacks);

    } else if (selectedClub == -1) {
        writeClubMenu("CHOOSE CLUB", attachClickCallbacks);

    } else {
        struct gameb::club &club = get_club(selectedClub);
        char clubText[34];
        snprintf(clubText, sizeof(clubText), "Club changed to %16.16s", club.name);

        change_club(selectedClub, settings.gamePath.c_str(), 0);
        Application::writeTextSmall(clubText, 8, nullptr, 0);

        Application::writeTextSmall(
                "« Back",
                16,
                attachClickCallbacks
                ? std::function<void(void)>{ [this] {
                    selectedClub = -1;
                    resetClickableAreas();
                    clickableAreasConfigured = false;
                }}
                : nullptr,
                0
        );
    }
}

void Application::writeClubMenu(const char *heading, bool attachClickCallbacks) {
    writeSubHeader(heading, nullptr);

    int textLine = 3;
    int offsetLeft = 0;

    std::vector<int> clubs{};

    for (int i = 0; i < 114; ++i) {
        struct gameb::club &club = get_club(i);
        if (club.league == division_hex[selectedDivision]) {
            clubs.push_back(i);
        }
    }

    std::sort(clubs.begin(), clubs.end(), [](int a, int b) {
        return strcmp(get_club(a).name, get_club(b).name) < 0;
    });

    for (auto club_idx: clubs) {
        if (textLine == 15) {
            offsetLeft = SCREEN_WIDTH / 2;
            textLine = 3;
        }

        char clubName[17];
        snprintf(clubName, sizeof(clubName), "%16.16s", get_club(club_idx).name);

        writeTextSmall(
                clubName,
                textLine,
                attachClickCallbacks
                ? std::function<void(void)>{ [this, club_idx] {
                    selectedClub = club_idx;
                    resetClickableAreas();
                    clickableAreasConfigured = false;
                }}
                : nullptr,
                offsetLeft
        );

        textLine++;
    }

    writeTextSmall(
            "« Back",
            16,
            attachClickCallbacks
            ? std::function<void(void)>{ [this] {
                selectedDivision = -1;
                selectedClub = -1;
                resetClickableAreas();
                clickableAreasConfigured = false;
            }}
            : nullptr,
            0
    );
}

void Application::writeDivisionsMenu(const char *heading, bool attachClickCallbacks) {
    writeSubHeader(heading, nullptr);

    for (size_t i = 0; i < std::size(division); i++) {
        writeTextSmall(
                division[i],
                i + 3,
                attachClickCallbacks
                ? std::function<void(void)>{ [this, i] {
                    selectedDivision = i;
                    selectedClub = -1;
                    resetClickableAreas();
                    clickableAreasConfigured = false;
                }}
                : nullptr,
                0
        );
    }
}

void Application::convertCoachScreen([[maybe_unused]] bool attachClickCallbacks) {
    writeHeader("CONVERT PLAYER TO COACH", nullptr);

    std::map<int8_t, struct gamec::player> validPlayers;

    struct gamea::manager &manager = gamea.manager[0];
    struct gameb::club &club = get_club(gamea.manager[0].club_idx);

    for (int8_t i = 0; i < 24; ++i) {
        if (club.player_index[i] == -1) {
            continue;
        }

        struct gamec::player &p = get_player(club.player_index[i]);

        if (p.age >= 29) {
            validPlayers[i] = p;
        }
    }


    if (validPlayers.empty()) {
        writeTextSmall("No players over 29 years old found", 8, nullptr, 0);
        return;
    }

    int textLine = 4;


    writePlayerSubHeader("T PLAYER NAME  HN TK PS SH HD CR FT F M A AG WAGES COACH RATING", 3,
                         nullptr);

    for (auto& pair : validPlayers) {  // Avoid structured bindings
        int8_t playerIdx = pair.first;
        struct gamec::player player = pair.second;
        char playerRow[74];
        int8_t playerRating = determine_player_rating(player);
        snprintf(playerRow, sizeof(playerRow),
                 "%1c %12.12s %2.2d %2.2d %2.2d %2.2d %2.2d %2.2d %2.2d %1.1s %1.1d %1.1d %2.2d %5d %-12.12s",
                 determine_player_type(player), player.name, player.hn,
                 player.tk, player.ps, player.sh, player.hd, player.cr,
                 player.ft, foot_short[player.foot], player.morl, player.aggr,
                 player.age, player.wage, rating[(playerRating - (playerRating % 5)) / 5]);

        auto clickCallback = [&, playerIdx]() {
            Application::convertPlayerToCoach(manager, club, playerIdx);
        };

        writePlayer(playerRow, determine_player_type(player), textLine++, attachClickCallbacks ? clickCallback : std::function<void(void)>{});
    }

    for (int i = textLine; i <= 27; i++) {
        char playerRow[69] = ". ............ .. .. .. .. .. .. .. . . . .. ..... ............";
        writeText(playerRow, i, Colors::TEXT_2, TEXT_TYPE_PLAYER, nullptr, 0);
    }
}

void Application::telephoneScreen(bool attachClickCallbacks) {

    struct TelephoneMenuItem {
        std::string text;
        int line;
        std::function<void(void)> callback;
    };

    Application::writeHeader("TELEPHONE", nullptr);

    std::vector<TelephoneMenuItem> menuItems = {
            {"ADVERTISE FOR FANS               (£25,000)", 3, [this] {
                resetTextBlocks();
                struct gamea::manager &manager = gamea.manager[0];
                struct gameb::club &club = get_club(manager.club_idx);

                int fandomIncreasePercent = 3 + std::rand() % (7 - 3 + 1);
                club.seating_avg = std::min(club.seating_avg * (1.0 + (fandomIncreasePercent / 100.0)), static_cast<double>(club.seating_max));
                club.bank_account -= 25000;

                std::string result = "\"We'll certainly see our ticket sales increase after this!\" - Assistant Manager\n\n"
                                     "Fans increased by " + std::to_string(fandomIncreasePercent) + "%";
                Application::addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
            }},
            {"ENTERTAIN TEAM                    (£5,000)", 4, [this] {
                resetTextBlocks();
                struct gamea::manager &manager = gamea.manager[0];
                struct gameb::club &club = get_club(manager.club_idx);

                for (int i = 0; i < 24; ++i) {
                    struct gamec::player &player = get_player(club.player_index[i]);
                    player.morl = 9;
                }

                std::string result = "\"The team disperse into the streets, singing the praises of their generous manager.\"\n\n"
                                     "Team morale has been boosted";
                club.bank_account -= 5000;
                Application::addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
            }},
            {"ARRANGE SMALL TRAINING CAMP     (£500,000)", 5, [this] {
                resetTextBlocks();
                struct gamea::manager &manager = gamea.manager[0];
                struct gameb::club &club = get_club(manager.club_idx);

                for (int i = 0; i < 24; ++i) {
                    struct gamec::player &player = get_player(club.player_index[i]);
                    player.hn = std::min(player.hn + std::rand() % 2, 99);
                    player.tk = std::min(player.tk + std::rand() % 2, 99);
                    player.ps = std::min(player.ps + std::rand() % 2, 99);
                    player.sh = std::min(player.sh + std::rand() % 2, 99);
                    player.hd = std::min(player.hd + std::rand() % 2, 99);
                    player.cr = std::min(player.cr + std::rand() % 2, 99);
                    player.aggr = std::min(player.aggr + 1, 9);
                    player.ft = 99;
                    player.morl = 9;
                }

                std::string result = "\"The team is looking quicker on their feet!\" - Assistant Manager\n\n"
                                     "Team stats increased";
                club.bank_account -= 500000;
                Application::addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
            }},
            {"ARRANGE MEDIUM TRAINING CAMP  (£1,000,000)", 6, [this] {
                resetTextBlocks();
                struct gamea::manager &manager = gamea.manager[0];
                struct gameb::club &club = get_club(manager.club_idx);

                for (int i = 0; i < 24; ++i) {
                    struct gamec::player &player = get_player(club.player_index[i]);

                    player.hn += std::rand() % 4;
                    player.hn = player.hn > 99 ? 99 : player.hn;
                    player.tk += std::rand() % 4;
                    player.tk = player.tk > 99 ? 99 : player.tk;
                    player.ps += std::rand() % 4;
                    player.ps = player.ps > 99 ? 99 : player.ps;
                    player.sh += std::rand() % 4;
                    player.sh = player.sh > 99 ? 99 : player.sh;
                    player.hd += std::rand() % 4;
                    player.hd = player.hd > 99 ? 99 : player.hd;
                    player.cr += std::rand() % 4;
                    player.cr = player.cr > 99 ? 99 : player.cr;
                    player.aggr += 1;
                    player.aggr = player.aggr > 9 ? 9 : player.aggr;
                    player.ft = 99;
                    player.morl = 9;
                }

                std::string result = "\"The boys showed real progress!\" - Assistant Manager\n\n"
                                     "Team stats increased";
                club.bank_account -= 1000000;
                Application::addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
            }},
            {"ARRANGE LARGE TRAINING CAMP   (£2,000,000)", 7, [this] {
                resetTextBlocks();
                struct gamea::manager &manager = gamea.manager[0];
                struct gameb::club &club = get_club(manager.club_idx);

                for (int i = 0; i < 24; ++i) {
                    struct gamec::player &player = get_player(club.player_index[i]);
                    player.hn = std::min(player.hn + std::rand() % 8, 99);
                    player.tk = std::min(player.tk + std::rand() % 8, 99);
                    player.ps = std::min(player.ps + std::rand() % 8, 99);
                    player.sh = std::min(player.sh + std::rand() % 8, 99);
                    player.hd = std::min(player.hd + std::rand() % 8, 99);
                    player.cr = std::min(player.cr + std::rand() % 8, 99);
                    player.aggr = std::min(player.aggr + 1, 9);
                    player.ft = 99;
                    player.morl = 9;
                }

                std::string result = "\"They are like a new team!\" - Assistant Manager\n\n"
                                     "Team stats increased";
                club.bank_account -= 2000000;
                Application::addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
            }},
            {"APPEAL RED CARD                  (£10,000)", 8, [this] {
                resetTextBlocks();
                struct gamea::manager &manager = gamea.manager[0];
                struct gameb::club &club = get_club(manager.club_idx);
                std::string result = "No banned player found";

                for (int i = 0; i < 24; ++i) {
                    struct gamec::player &player = get_player(club.player_index[i]);
                    if (player.period > 0 && player.period_type == 0) {
                        if (std::rand() % 2 == 0) {
                            player.period = 0;
                            result = "\"We see what you mean. We've overturned the decision for " +
                                     std::string(player.name, sizeof(player.name)) + ".\" - The FA";
                        } else {
                            result = "\"Sorry, but our decision was fair.\" - The FA";
                        }
                        club.bank_account -= 10000;
                        break;
                    }
                }
                Application::addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
            }},
            {"BUILD NEW 25k SEAT STADIUM    (£5,000,000)", 9, [this] {
                resetTextBlocks();
                struct gamea::manager &manager = gamea.manager[0];
                struct gameb::club &club = get_club(manager.club_idx);

                int currentStadiumValue = calculateStadiumValue(manager);

                club.seating_max = 25000;

                manager.stadium.ground_facilities.level = 2;
                manager.stadium.supporters_club.level = 2;
                manager.stadium.flood_lights.level = 1;
                manager.stadium.scoreboard.level = 2;
                manager.stadium.undersoil_heating.level = 0;
                manager.stadium.changing_rooms.level = 1;
                manager.stadium.gymnasium.level = 2;
                manager.stadium.car_park.level = 1;

                for (int i = 0; i < sizeof (manager.stadium.safety_rating); ++i) {
                    manager.stadium.safety_rating[i] = 3;
                }

                for (int i = 0; i < 4; ++i) {
                    manager.stadium.capacity[i].seating = 6250;
                    manager.stadium.capacity[i].terraces = 0;
                    manager.stadium.conversion[i].level = 1;
                    manager.stadium.area_covering[i].level = 2;
                }

                club.bank_account -= (5000000 - currentStadiumValue);

                std::string result = "\"The new 25,000 seat stadium is ready! The fans are going to love it!\" - Assistant Manager\n\n"
                                     "\"We'll buy your old stadium for £" + std::to_string(currentStadiumValue) +
                                     " to tear down and turn into flats\" - Local Council";
                Application::addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
            }},
            {"BUILD NEW 50k SEAT STADIUM   (£10,000,000)", 10, [this] {
                resetTextBlocks();
                struct gamea::manager &manager = gamea.manager[0];
                struct gameb::club &club = get_club(manager.club_idx);

                int currentStadiumValue = calculateStadiumValue(manager);
                club.seating_max = 50000;

                manager.stadium.ground_facilities.level = 2;
                manager.stadium.supporters_club.level = 2;
                manager.stadium.flood_lights.level = 2;
                manager.stadium.scoreboard.level = 2;
                manager.stadium.undersoil_heating.level = 1;
                manager.stadium.changing_rooms.level = 2;
                manager.stadium.gymnasium.level = 2;
                manager.stadium.car_park.level = 2;

                for (int i = 0; i < sizeof (manager.stadium.safety_rating); ++i) {
                    manager.stadium.safety_rating[i] = 4;
                }

                for (int i = 0; i < 4; ++i) {
                    manager.stadium.capacity[i].seating = 12500;
                    manager.stadium.capacity[i].terraces = 0;
                    manager.stadium.conversion[i].level = 2;
                    manager.stadium.area_covering[i].level = 3;
                }

                club.bank_account -= (10000000 - currentStadiumValue);

                std::string result = "\"The new 50,000 seat stadium is ready! It's incredible!\" - Assistant Manager\n\n"
                                     "\"We'll buy your old stadium for £" + std::to_string(currentStadiumValue) +
                                     " for a nearby school to use\" - Local Council";
                Application::addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
            }},
            {"BUILD NEW 100k SEAT STADIUM  (£15,000,000)", 11, [this] {
                resetTextBlocks();
                struct gamea::manager &manager = gamea.manager[0];
                struct gameb::club &club = get_club(manager.club_idx);

                int currentStadiumValue = calculateStadiumValue(manager);

                club.seating_max = 100000;

                manager.stadium.ground_facilities.level = 3;
                manager.stadium.supporters_club.level = 3;
                manager.stadium.flood_lights.level = 2;
                manager.stadium.scoreboard.level = 3;
                manager.stadium.undersoil_heating.level = 1;
                manager.stadium.changing_rooms.level = 2;
                manager.stadium.gymnasium.level = 3;
                manager.stadium.car_park.level = 2;

                for (int i = 0; i < sizeof (manager.stadium.safety_rating); ++i) {
                    manager.stadium.safety_rating[i] = 4;
                }

                for (int i = 0; i < 4; ++i) {
                    manager.stadium.capacity[i].seating = 25000;
                    manager.stadium.capacity[i].terraces = 0;
                    manager.stadium.conversion[i].level = 2;
                    manager.stadium.area_covering[i].level = 3;
                }

                club.bank_account -= (15000000 - currentStadiumValue);

                std::string result = "\"The new 100,000 seat stadium is ready! It's so nice, I bought my mum a season ticket!\" - Assistant Manager\n\n"
                                     "\"We'll buy your old stadium for £" + std::to_string(currentStadiumValue) +
                                     " to turn into a sports center\" - Local Council";
                Application::addTextBlock(result.c_str(), 400, 75, 200, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr);
            }},
    };

// Iterate through menu items and apply the ternary operator
    for (const auto& item : menuItems) {
        Application::writeTextSmall(
                item.text.c_str(),
                item.line,
                attachClickCallbacks ? std::function<void(void)>{ item.callback } : nullptr,
                0
        );
    }
}

int Application::calculateStadiumValue(struct gamea::manager &manager) {

    int total = 0;

    switch (manager.stadium.ground_facilities.level) {
        case 3: total += 150000;
        case 2: total += 50000;
        case 1: total += 10000;
    }

    switch (manager.stadium.supporters_club.level) {
        case 3: total += 300000;
        case 2: total += 75000;
        case 1: total += 10000;
    }

    switch (manager.stadium.flood_lights.level) {
        case 2: total += 25000;
        case 1: total += 15000;
    }

    switch (manager.stadium.scoreboard.level) {
        case 3: total += 20000;
        case 2: total += 12000;
        case 1: total += 8000;
    }

    switch (manager.stadium.undersoil_heating.level) {
        case 1: total += 500000; break;
    }

    switch (manager.stadium.changing_rooms.level) {
        case 2: total += 60000;
        case 1: total += 25000;
    }

    switch (manager.stadium.gymnasium.level) {
        case 3: total += 50000;
        case 2: total += 25000;
        case 1: total += 250000;
    }

    switch (manager.stadium.car_park.level) {
        case 2: total += 1000000;
        case 1: total += 400000;
    }

    switch (manager.stadium.safety_rating[0]) {
        case 4: total += 1000000;
        case 3: total += 350000;
        case 2: total += 150000;
        case 1: total += 50000;
    }

    for (int i = 0; i < 4; ++i) {
        switch (manager.stadium.area_covering[i].level) {
            case 3: total += 100000;
            case 2: total += 40000;
            case 1: total += 15000;
        }

        if (manager.stadium.capacity[i].terraces == 0) {
            total += manager.stadium.capacity[i].seating * 25;
        } else {
            switch (manager.stadium.conversion[i].level) {
                case 2:
                    total += manager.stadium.capacity[i].seating * 100;
                    total += manager.stadium.capacity[i].seating * 100;
                case 1:
                    total += manager.stadium.capacity[i].seating * 75;
                case 0:
                    total += manager.stadium.capacity[i].seating * 50;
            }
        }
    }

    return total;
}

void Application::testFontScreen([[maybe_unused]] bool attachClickCallbacks) {

    for (int x = 0; x < 29; ++x) {
        char a = static_cast<char>(x + 1);
        char b = static_cast<char>(x + 30);
        char c = static_cast<char>(x + 59);
        char d = static_cast<char>(x + 88);
        char e = static_cast<char>(x + 117);
        char f = static_cast<char>(x + 146);
        char g = static_cast<char>(x + 175);
        char h = static_cast<char>(x + 204);
        char i = static_cast<char>(x + 233);

        std::string testText = std::to_string(x) + ": " + a + "  " +
                               std::to_string(x + 29) + ": " + b + "  " +
                               std::to_string(x + 58) + ": " + c + "  " +
                               std::to_string(x + 87) + ": " + d + "  " +
                               std::to_string(x + 116) + ": " + e + "  " +
                               std::to_string(x + 145) + ": " + f + "  " +
                               std::to_string(x + 175) + ": " + g + "  " +
                               std::to_string(x + 204) + ": " + h + "  " +
                               std::to_string(x + 233) + ": " + i + "  ";

        int textLine = (x % 29) + 2;

        Application::writeText(
                testText.c_str(),
                textLine,
                getDefaultTextColor(textLine),
                TEXT_TYPE_PLAYER,
                nullptr, 0);
    }
}

void Application::loadPrefs() {
    if (!std::filesystem::exists(PREFS_PATH)) {
        return;
    }
    std::ifstream in(PREFS_PATH, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open the file for reading.\n";
        return;
    }
    settings.deserialize(in);
    in.close();
}

void Application::savePrefs() {
    std::ofstream out(PREFS_PATH, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open the file for writing.\n";
        return;
    }
    settings.serialize(out);
    out.close();
}

void Application::drawTopDetails() {
    struct gamea::manager &manager = gamea.manager[0];
    struct gameb::club &club = get_club(manager.club_idx);

    char line1[55];

    char managerName[17];
    snprintf(managerName, sizeof(managerName), "%16.16s", manager.name);

    char clubName[17];
    snprintf(clubName, sizeof(clubName), "%16.16s", club.name);

    char divisionName[18];
    snprintf(divisionName, sizeof(divisionName), "%17.17s", division[manager.division]);

    snprintf(line1, sizeof(line1), "%s %s %s", managerName, clubName, divisionName);

    writeText(line1, -2, Colors::TEXT_TOP_DETAILS, TEXT_TYPE_PLAYER, nullptr, 0);

    std::string line2;
    std::string contract;
    std::string bankBalance;

    contract = std::to_string(manager.contract_length);
    bankBalance = std::to_string(club.bank_account);

    line2 = "CONTRACT: " + contract + " £" + bankBalance;

    writeText(line2.c_str(), -1, Colors::TEXT_TOP_DETAILS, TEXT_TYPE_PLAYER, nullptr, 0);
}

void Application::drawPagination(bool attachClickCallbacks) {
    std::string pagination = "Page " + std::to_string(currentPage) + " of " + std::to_string(totalPages);
    pagination.resize(17, ' ');

    if (currentPage == 1) {
        pagination += "         Next »";
    } else if (currentPage == totalPages) {
        pagination += "« Prev";
    } else {
        pagination += "« Prev | Next »";
    }

    sprintf(footer, "%s", pagination.c_str());

    if (attachClickCallbacks) {
        addClickableArea(171, 306, 50, 12, [this] { if (currentPage > 1) currentPage--; },
                         TRANSIENT_CLICKABLE_AREA_TYPE);
        addClickableArea(242, 306, 50, 12, [this] { if (currentPage < totalPages) currentPage++; },
                         TRANSIENT_CLICKABLE_AREA_TYPE);
    }
}

void Application::toggleWindowed() {
    windowed = !windowed;
    windowed ? SDL_SetWindowFullscreen(window, 0) : SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
}

void Application::resetTextBlocks() {
    textBlocks = {};
}

bool Application::backupSaveFile(int gameNumber) {
    for (char c = 'A'; c <= 'C'; ++c) {
        std::filesystem::path saveGamePath = construct_save_file_path(settings.gamePath, gameNumber, c);

        try {
            if (!std::filesystem::exists(saveGamePath)) {
                return true;
            }

            std::filesystem::path backupPath = construct_saves_folder_path(settings.gamePath) / BACKUP_SAVE_PATH;

            if (!std::filesystem::exists(backupPath)) {
                if (!std::filesystem::create_directories(backupPath)) {
                    std::cerr << "Failed to create backup directory: " << backupPath << std::endl;
                    return false;
                }
            }

            std::filesystem::path dest = backupPath / saveGamePath.filename();
            std::filesystem::copy(saveGamePath, dest, std::filesystem::copy_options::overwrite_existing);
        } catch (const std::filesystem::filesystem_error &e) {
            std::cerr << "Error copying file: " << e.what() << std::endl;
            return false;
        }
    }

    return true;
}

void Application::convertPlayerToCoach(struct gamea::manager &manager, struct gameb::club &club, int8_t clubPlayerIdx) {

    struct gamec::player &player = get_player(club.player_index[clubPlayerIdx]);

    std::unordered_map<char, int> playerTypeToEmployeePosition = {
            {'G', 8}, {'D', 9}, {'M', 10}, {'A', 11}
    };

    char playerType = determine_player_type(player);
    int8_t playerRating = determine_player_rating(player);

    struct gamea::manager::employee &employee = manager.employee[playerTypeToEmployeePosition[playerType]];
    strncpy(employee.name, player.name, 12);
    employee.skill = playerRating;
    employee.age = 0;

    player.hn = player.hn / 2;
    player.tk = player.tk / 2;
    player.ps = player.ps / 2;
    player.sh = player.sh / 2;
    player.hd = player.hd / 2;
    player.cr = player.cr / 2;

    player.morl = 5;
    player.aggr = 1 + (std::rand() % 9);
    player.ins = 0;
    player.age = 16 + (std::rand() % 4);
    player.foot = (std::rand() % 2);
    player.dpts = 0;
    player.played = 0;
    player.scored = 0;
    player.unk2 = 0;
    player.wage = 50 + (std::rand() % 500);
    player.ins_cost = 0;
    player.period = 0;
    player.period_type = 0;
    player.contract = 1;
    player.unk5 = 192;
    player.u23 = 0;
    player.u25 = 0;

    club.player_index[clubPlayerIdx] = -1;

    struct gameb::club &new_club = get_club(92 + (std::rand() % (113 - 92 + 1)));

    new_club.player_index[23] = clubPlayerIdx;

    strcpy(footer, "CONVERTED TO A COACH");
}

int main() {
    Application app;
    app.run();
    return 0;
}
