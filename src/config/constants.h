// Shared constants for screen layout, assets, fonts, and save paths.
#pragma once

// Text types
inline constexpr int TEXT_TYPE_HEADER = 0;
inline constexpr int TEXT_TYPE_LARGE = 1;
inline constexpr int TEXT_TYPE_SMALL = 2;
inline constexpr int TEXT_TYPE_PLAYER = 3;

inline constexpr int TEXT_LINE_SPACING = 2;

// Screen geometry
inline constexpr int SCREEN_WIDTH = 640;
inline constexpr int SCREEN_HEIGHT = 400;
inline constexpr int MARGIN_LEFT = 40;
inline constexpr int MARGIN_TOP = 19;

// Icon layout
inline constexpr int ICON_WIDTH = 48;
inline constexpr int ICON_HEIGHT = 37;
inline constexpr int ICON_LEFT_MARGIN = 23;
inline constexpr int ICON_TOP_MARGIN = 355;
inline constexpr int ICON_SPACING = 13;

// Asset paths
inline constexpr const char *SCREEN_IMAGE_PATH = "assets/screen.png";
inline constexpr const char *LOADING_SCREEN_IMAGE_PATH = "assets/loading.png";
inline constexpr const char *CURSOR_STANDARD_IMAGE_PATH = "assets/cursor.png";
inline constexpr const char *CURSOR_CLICK_LEFT_IMAGE_PATH = "assets/cursor-click-left.png";
inline constexpr const char *CURSOR_CLICK_RIGHT_IMAGE_PATH = "assets/cursor-click-right.png";
inline constexpr const char *ICON_LOAD_IMAGE_PATH = "assets/icon-load.png";
inline constexpr const char *ICON_SAVE_IMAGE_PATH = "assets/icon-save.png";
inline constexpr const char *ICON_FREE_PLAYERS_IMAGE_PATH = "assets/icon-free-players.png";
inline constexpr const char *ICON_MY_TEAM_IMAGE_PATH = "assets/icon-my-team.png";
inline constexpr const char *ICON_SCOUT_IMAGE_PATH = "assets/icon-scout.png";
inline constexpr const char *ICON_CHANGE_TEAM_IMAGE_PATH = "assets/icon-change-team.png";
inline constexpr const char *ICON_TELEPHONE_IMAGE_PATH = "assets/icon-telephone.png";
inline constexpr const char *ICON_CONVERT_COACH_IMAGE_PATH = "assets/icon-convert-coach.png";
inline constexpr const char *ICON_SETTINGS_IMAGE_PATH = "assets/icon-settings.png";

// Fonts
inline constexpr const char *HEADER_FONT_PATH = "assets/acknowtt.ttf";
inline constexpr const char *TALL_FONT_PATH = "assets/unscii-16.ttf";
inline constexpr const char *SHORT_FONT_PATH = "assets/unscii-8.ttf";

// Prefs/save locations
inline constexpr const char *PREFS_PATH = "PM3000.PREFS";
inline constexpr const char *BACKUP_SAVE_PATH = "PM3000";
