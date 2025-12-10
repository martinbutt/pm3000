#include <iostream>
#include <SDL.h>
#include <functional>
#include <unistd.h>
#include <map>
#include <filesystem>
#include <bitset>
#include <random>
#include <memory>
#include <exception>
#include <cstdlib>
#include <cstring>
#include <string>
#include "config/constants.h"
#include "text.h"
#include "gfx.h"
#include "input.h"
#include "io.h"
#include "game_utils.h"
#include "settings.h"
#include "swos_import.h"
#include "nfd.h"
#include "ui.h"
#include "screens/screen.h"
#include "screens/loading_screen.h"
#include "screens/first_time_screen.h"
#include "screens/must_load_game_screen.h"
#include "screens/test_font_screen.h"
#include "screens/settings_screen.h"
#include "screens/load_game_screen.h"
#include "screens/save_game_screen.h"
#include "screens/free_players_screen.h"
#include "screens/my_team_screen.h"
#include "screens/scout_screen.h"
#include "screens/change_team_screen.h"
#include "screens/telephone_screen.h"
#include "screens/convert_coach_screen.h"

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

class Application {
public:
    Application() : input(gfx) {
        initializeSDL();
    };

    ~Application();

    void run();

private:
    Graphics gfx;
    InputHandler input;

    bool quit = false;

    std::unique_ptr<TextRenderer> textRenderer;
    ScreenContext screenContext{};
    std::map<screen, std::unique_ptr<Screen>> screens;

    std::bitset<8> saveFiles{};

    std::vector<club_player> freePlayers{};

    Settings settings{};

    bool clickableAreasConfigured = false;

    screen currentScreen = LOADING_SCREEN;

    int currentGame = 0;

    int currentPage = 0;
    int totalPages = 0;

    int selectedDivision = -1;
    int selectedClub = -1;

    using screenCallback = std::function<void(bool)>;

    std::map<int, screenCallback> screenCallbacks = {};

    char footer[70]{};

    void initializeSDL();
    void initializeScreens();

    void changeScreen(screen newScreen);

    void drawCurrentScreen();

    void toggleWindowed();

    void importSwosTeams();

    [[noreturn]] static void exitError(const std::string &errorMessage);

};

bool windowed = true;

Application::~Application() {
    gfx.cleanup();
}

void Application::initializeSDL() {
    try {
        gfx.initialize();
    } catch (const std::exception &ex) {
        exitError("Could not init SDL\n" + std::string(ex.what()));
    }
    SDL_SetRelativeMouseMode(SDL_TRUE);

    io::loadPrefs(settings);
    settings.gameType = io::getPm3GameType(settings.gamePath);

#if defined linux && SDL_VERSION_ATLEAST(2, 0, 8)
    // Disable compositor bypass
    if (!SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0")) {
        exitError("SDL can not disable compositor bypass!");
    }
#endif

    try {
        gfx.createWindowAndRenderer("Premier Manager 3000", SCREEN_WIDTH, SCREEN_HEIGHT);
    } catch (const std::exception &ex) {
        exitError(ex.what());
    }

    textRenderer = std::make_unique<TextRenderer>(
            gfx.getRenderer(), [this](int x, int y, int w, int h, const std::function<void(void)> &callback) {
                input.addClickableArea(x, y, w, h, callback, ClickableAreaType::Transient);
            });

    try {
        text_utils::loadFont(*textRenderer, HEADER_FONT_PATH, TEXT_TYPE_HEADER);
        text_utils::loadFont(*textRenderer, TALL_FONT_PATH, TEXT_TYPE_LARGE);
        text_utils::loadFont(*textRenderer, TALL_FONT_PATH, TEXT_TYPE_SMALL);
        text_utils::loadFont(*textRenderer, SHORT_FONT_PATH, TEXT_TYPE_PLAYER);
    } catch (const std::exception &ex) {
        exitError(ex.what());
    }
}

void Application::initializeScreens() {
    screenContext.drawBackground = [this](const char *path) {
        try {
            gfx.drawBackground(path, SCREEN_WIDTH, SCREEN_HEIGHT);
        } catch (const std::exception &ex) {
            exitError(ex.what());
        }
    };
    screenContext.writeTextLarge = [this](const char *text, int line, const std::function<void(void)> &cb) {
        if (textRenderer) {
            text_utils::writeTextLarge(*textRenderer, text, line, cb);
        }
    };
    screenContext.writeText = [this](const char *text, int line, SDL_Color color, int textType,
                                     const std::function<void(void)> &cb, int offsetLeft) {
        if (!textRenderer) {
            return;
        }
        try {
            text_utils::writeText(*textRenderer, text, line, color, textType, cb, offsetLeft);
        } catch (const std::exception &ex) {
            exitError(ex.what());
        }
    };
    screenContext.defaultTextColor = [this](int line) {
        if (!textRenderer) {
            return Colors::TEXT_1;
        }
        return text_utils::defaultTextColor(*textRenderer, line);
    };
    screenContext.currentGame = [this]() { return currentGame; };
    screenContext.currentPage = [this]() { return currentPage; };
    screenContext.setPagination = [this](int page, int total) { currentPage = page; totalPages = total; };
    screenContext.gamePath = [this]() -> const std::filesystem::path & { return settings.gamePath; };
    screenContext.gameType = [this]() { return settings.gameType; };
    screenContext.choosePm3Folder = [this]() {
        try {
            io::choosePm3Folder(settings, saveFiles);
        } catch (const std::exception &ex) {
            exitError(ex.what());
        }
    };
    screenContext.importSwosTeams = [this]() {
        importSwosTeams();
    };
    screenContext.levelAggression = [this]() { levelAggression(); };
    screenContext.setFooter = [this](const char *text) { strncpy(footer, text, sizeof(footer) - 1); footer[sizeof(footer)-1] = '\0'; };
    screenContext.ensureMetadataLoaded = [this](bool attach) {
        return io::ensureMetadataLoaded(settings, currentGame, saveFiles, footer, sizeof(footer), attach);
    };
    screenContext.formatSaveGameLabel = [](int i, char *label, size_t size) { io::formatSaveGameLabel(i, label, size); };
    screenContext.saveFiles = [this]() -> const std::bitset<8> & { return saveFiles; };
    screenContext.loadGameConfirm = [this](int gameNumber) {
        io::loadGameConfirm(input, settings, gameNumber, currentGame, footer, sizeof(footer));
    };
    screenContext.saveGameConfirm = [this](int gameNumber) {
        io::saveGameConfirm(input, settings, gameNumber, footer, sizeof(footer));
    };
    screenContext.writeHeader = [this](const char *text, int /*line*/, const std::function<void(void)> &cb) {
        if (textRenderer) {
            text_utils::writeHeader(*textRenderer, text, cb);
        }
    };
    screenContext.writeSubHeader = [this](const char *text, int /*line*/, const std::function<void(void)> &cb) {
        if (textRenderer) {
            text_utils::writeSubHeader(*textRenderer, text, cb);
        }
    };
    screenContext.freePlayersRef = [this]() -> std::vector<club_player> & { return freePlayers; };
    screenContext.refreshFreePlayers = [this]() { freePlayers = findFreePlayers(); };
    screenContext.writePlayers = [this](std::vector<club_player> &players, int &line,
                                        const std::function<void(const club_player &)> &cb) {
        if (!textRenderer) {
            return line;
        }
        return text_utils::writePlayers(*textRenderer, players, line, cb);
    };
    screenContext.setFooterLine = [this](const char *text) { snprintf(footer, sizeof(footer), "%s", text); };
    screenContext.selectedDivision = [this]() { return selectedDivision; };
    screenContext.selectedClub = [this]() { return selectedClub; };
    screenContext.resetSelection = [this]() { selectedDivision = -1; selectedClub = -1; };
    screenContext.resetClickableAreas = [this]() { input.resetTransientClickableAreas(); };
    screenContext.setClickableAreasConfigured = [this](bool v) { clickableAreasConfigured = v; };
    screenContext.addKeyPressCallback = [this](SDL_Keycode key, const std::function<void(void)> &cb) {
        input.addKeyPressCallback(key, cb);
    };
    screenContext.resetKeyPressCallbacks = [this]() { input.resetKeyPressCallbacks(); };
    screenContext.startReadingTextInput = [this](std::function<void(void)> cb) {
        input.startReadingTextInput(std::move(cb));
    };
    screenContext.endReadingTextInput = [this]() { input.endReadingTextInput(); };
    screenContext.currentTextInput = [this]() -> const char * { return input.getTextInput(); };
    screenContext.makeOffer = [this](const club_player &playerInfo) {
        game_utils::beginOffer(input, footer, sizeof(footer), playerInfo, currentGame);
    };
    screenContext.writeDivisionsMenu = [this](const char *heading, bool attach) {
        ui::writeDivisionsMenu(screenContext, selectedDivision, selectedClub, heading, attach);
    };
    screenContext.writeClubMenu = [this](const char *heading, bool attach) {
        ui::writeClubMenu(screenContext, selectedClub, selectedDivision, heading, attach);
    };
    screenContext.convertPlayerToCoach = [this](struct gamea::ManagerRecord &manager, ClubRecord &club, int8_t idx) {
        game_utils::convertPlayerToCoach(manager, club, idx, footer, sizeof(footer));
    };
    screenContext.writePlayer = [this](const char *text, char position, int line, const std::function<void(void)> &cb) {
        if (textRenderer) {
            text_utils::writePlayer(*textRenderer, text, position, line, cb);
        }
    };
    screenContext.addTextBlock = [this](const char *text, int x, int y, int w, SDL_Color color, int textType,
                                        const std::function<void(void)> &cb) {
        if (!textRenderer) {
            return;
        }
        text_utils::addTextBlock(*textRenderer, text, x, y, w, color, textType, cb);
    };
    screenContext.resetTextBlocks = [this]() {
        if (textRenderer) {
            text_utils::resetTextBlocks(*textRenderer);
        }
    };


    screens[LOADING_SCREEN] = std::make_unique<LoadingScreen>(screenContext);
    screens[FIRST_TIME_GAME_SCREEN] = std::make_unique<FirstTimeScreen>(screenContext);
    screens[MUST_LOAD_GAME_SCREEN] = std::make_unique<MustLoadGameScreen>(screenContext);
    screens[TEST_SCREEN] = std::make_unique<TestFontScreen>(screenContext);
    screens[SETTINGS_SCREEN] = std::make_unique<SettingsScreen>(screenContext);
    screens[LOAD_GAME_SCREEN] = std::make_unique<LoadGameScreen>(screenContext);
    screens[SAVE_GAME_SCREEN] = std::make_unique<SaveGameScreen>(screenContext);
    screens[FREE_PLAYERS_SCREEN] = std::make_unique<FreePlayersScreen>(screenContext);
    screens[MY_TEAM_SCREEN] = std::make_unique<MyTeamScreen>(screenContext);
    screens[SCOUT_SCREEN] = std::make_unique<ScoutScreen>(screenContext);
    screens[CHANGE_TEAM_SCREEN] = std::make_unique<ChangeTeamScreen>(screenContext);
    screens[TELEPHONE_SCREEN] = std::make_unique<TelephoneScreen>(screenContext);
    screens[CONVERT_COACH_SCREEN] = std::make_unique<ConvertCoachScreen>(screenContext);
}

void Application::run() {
    SDL_Event event;
    SDL_Renderer *renderer = gfx.getRenderer();

    initializeScreens();
    if (screens.count(LOADING_SCREEN)) {
        screens[LOADING_SCREEN]->draw(false);
    }
    SDL_RenderPresent(renderer);
    sleep(3);

    std::random_device rd;
    std::srand(rd());

    if (settings.gamePath.empty()) {
        Application::changeScreen(FIRST_TIME_GAME_SCREEN);
    } else {
        Application::changeScreen(MUST_LOAD_GAME_SCREEN);
    }

    try {
        gfx.configureCursors(CURSOR_STANDARD_IMAGE_PATH, CURSOR_CLICK_LEFT_IMAGE_PATH,
                              CURSOR_CLICK_RIGHT_IMAGE_PATH);
    } catch (const std::exception &ex) {
        exitError(ex.what());
    }
    input.addClickableArea(572, 358, 48, 25, [this] { quit = true; }, ClickableAreaType::Persistent);
    try {
        ui::addIcon(gfx, input, ICON_LOAD_IMAGE_PATH, 1, [this] { changeScreen(LOAD_GAME_SCREEN); });
        ui::addIcon(gfx, input, ICON_SAVE_IMAGE_PATH, 2, [this] { changeScreen(SAVE_GAME_SCREEN); });
        ui::addIcon(gfx, input, ICON_CHANGE_TEAM_IMAGE_PATH, 3, [this] { changeScreen(CHANGE_TEAM_SCREEN); });
        ui::addIcon(gfx, input, ICON_MY_TEAM_IMAGE_PATH, 4, [this] { changeScreen(MY_TEAM_SCREEN); });
        ui::addIcon(gfx, input, ICON_SCOUT_IMAGE_PATH, 5, [this] { changeScreen(SCOUT_SCREEN); });
        ui::addIcon(gfx, input, ICON_FREE_PLAYERS_IMAGE_PATH, 6, [this] { changeScreen(FREE_PLAYERS_SCREEN); });
        ui::addIcon(gfx, input, ICON_CONVERT_COACH_IMAGE_PATH, 7, [this] { changeScreen(CONVERT_COACH_SCREEN); });
        ui::addIcon(gfx, input, ICON_TELEPHONE_IMAGE_PATH, 8, [this] { changeScreen(TELEPHONE_SCREEN); });
        ui::addIcon(gfx, input, ICON_SETTINGS_IMAGE_PATH, 9, [this] { changeScreen(SETTINGS_SCREEN); });
    } catch (const std::exception &ex) {
        exitError(ex.what());
    }

    SDL_RenderPresent(renderer);

    SDL_Texture *texTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET,
                                               SCREEN_WIDTH, SCREEN_HEIGHT);

    while (!quit) {
        // Event handling loop
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || this->quit) {
                quit = true;
            } else if (input.handleTextInputEvent(event)) {
                // handled in input module
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == 1) {
                    gfx.setLeftClickCursor();
                } else {
                    gfx.setRightClickCursor();
                }
                input.checkClickableArea(event.button.x, event.button.y);
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                gfx.setStandardCursor();
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                    case SDLK_f:
                        toggleWindowed();
                        break;
                    case SDLK_q:
                        quit = true;
                        break;
                    default:
                        input.checkKeyPressCallback(event.key.keysym.sym);
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

[[noreturn]] void Application::exitError(const std::string &errorMessage) {
    std::cout << "An error occurred: " << errorMessage << std::endl;
    exit(1);
}

void Application::changeScreen(screen newScreen) {
    if (currentGame == 0 && settings.gamePath.empty() && newScreen != SETTINGS_SCREEN) {
        newScreen = FIRST_TIME_GAME_SCREEN;
    } else if (currentGame == 0 && newScreen != LOAD_GAME_SCREEN && newScreen != SETTINGS_SCREEN) {
        newScreen = MUST_LOAD_GAME_SCREEN;
    }
    if (newScreen != currentScreen) {
        input.resetTransientClickableAreas();
        input.resetKeyPressCallbacks();
        if (textRenderer) {
            text_utils::resetTextBlocks(*textRenderer);
        }
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
    try {
        gfx.drawBackground(SCREEN_IMAGE_PATH, SCREEN_WIDTH, SCREEN_HEIGHT);
    } catch (const std::exception &ex) {
        exitError(ex.what());
    }
    ui::drawIcons(gfx);
    if (currentGame) {
        ui::drawTopDetails(screenContext);
    }

    auto screenIt = screens.find(currentScreen);
    if (screenIt != screens.end()) {
        screenIt->second->draw(!clickableAreasConfigured);
    } else {
        auto cbIt = screenCallbacks.find(currentScreen);
        if (cbIt != screenCallbacks.end()) {
            cbIt->second(!clickableAreasConfigured);
        }
    }

    ui::drawPagination(input, currentPage, totalPages, footer, sizeof(footer), !clickableAreasConfigured);

    if (textRenderer) {
        text_utils::drawTextBlocks(*textRenderer, !clickableAreasConfigured);
    }

    if (strlen(footer) && textRenderer) {
        text_utils::writeTextSmall(*textRenderer, footer, 16, nullptr, 0);
    }

    clickableAreasConfigured = true;
}

void Application::importSwosTeams() {
    if (settings.gamePath.empty()) {
        snprintf(footer, sizeof(footer), "Select PM3 folder before importing.");
        return;
    }

    if (!io::backupPm3Files(settings.gamePath)) {
        snprintf(footer, sizeof(footer), "Backup failed: %.64s", io::pm3LastError().c_str());
        return;
    }

    try {
        io::loadDefaultGamedata(settings.gamePath, gameData);
        io::loadDefaultClubdata(settings.gamePath, clubData);
        io::loadDefaultPlaydata(settings.gamePath, playerData);
    } catch (const std::exception &ex) {
        snprintf(footer, sizeof(footer), "Load failed: %.64s", ex.what());
        return;
    }

    NFD_Init();
    nfdchar_t *teamPathRaw = nullptr;
    std::string defaultPathUtf8 = settings.gamePath.u8string();
    const char *defaultPathPtr = defaultPathUtf8.empty() ? nullptr : defaultPathUtf8.c_str();
    nfdresult_t result = NFD_OpenDialog(&teamPathRaw, nullptr, 0, defaultPathPtr);
    std::string message;

    if (result == NFD_OKAY && teamPathRaw) {
        std::filesystem::path teamPath(teamPathRaw);
        NFD_FreePath(teamPathRaw);
        try {
            std::string pm3PathUtf8 = settings.gamePath.u8string();
            auto report = swos_import::importTeamsFromFile(teamPath.u8string(), pm3PathUtf8);
            io::saveDefaultGamedata(settings.gamePath, gameData);
            io::saveDefaultClubdata(settings.gamePath, clubData);
            io::saveDefaultPlaydata(settings.gamePath, playerData);
            message = "SWOS import: matched " + std::to_string(report.teams_matched) +
                      ", created " + std::to_string(report.teams_created) +
                      ", unplaced " + std::to_string(report.teams_unplaced) +
                      ", renamed " + std::to_string(report.players_renamed) + " players.";
        } catch (const std::exception &ex) {
            message = "Import failed: ";
            message += ex.what();
        }
    } else if (result == NFD_CANCEL) {
        message = "SWOS import canceled";
    } else {
        const char *err = NFD_GetError();
        message = std::string("Dialog error: ") + (err ? err : "Unknown");
    }

    NFD_Quit();
    snprintf(footer, sizeof(footer), "%s", message.c_str());
}

void Application::toggleWindowed() {
    windowed = !windowed;
    SDL_Window *window = gfx.getWindow();
    if (window) {
        windowed ? SDL_SetWindowFullscreen(window, 0)
                 : SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}

int main() {
    Application app;
    app.run();
    return 0;
}
