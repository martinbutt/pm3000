// IO/persistence helpers for saves, metadata, and prefs.
#include "io.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#include "nfd.h"
#include "config/constants.h"
#include "input.h"
#include "pm3_data.h"

static std::string gPm3LastError;

template <typename T>
static bool load_binary_file(const std::string &filepath, T &data) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        gPm3LastError = "Missing file: " + filepath;
        return false;
    }
    file.read(reinterpret_cast<char*>(&data), sizeof(T));
    return true;
}

template <typename T>
static void save_binary_file(const std::string &filepath, const T &data) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Could not open file for writing: " + filepath);
    }
    file.write(reinterpret_cast<const char*>(&data), sizeof(T));
}

namespace io {

void loadBinaries(int game_nr, const std::string &game_path, gamea &game_data, gameb &club_data, gamec &player_data) {
    load_binary_file(constructSaveFilePath(game_path, game_nr, 'A'), game_data);
    load_binary_file(constructSaveFilePath(game_path, game_nr, 'B'), club_data);
    load_binary_file(constructSaveFilePath(game_path, game_nr, 'C'), player_data);
}

void loadDefaultGamedata(const std::string &game_path, gamea &game_data) {
    load_binary_file(constructGameFilePath(game_path, std::string{kGameDataFile}), game_data);
}

void loadDefaultClubdata(const std::string &game_path, gameb &club_data) {
    load_binary_file(constructGameFilePath(game_path, std::string{kClubDataFile}), club_data);
}

void loadDefaultPlaydata(const std::string &game_path, gamec &player_data) {
    load_binary_file(constructGameFilePath(game_path, std::string{kPlayDataFile}), player_data);
}

bool loadMetadata(const std::string &game_path, saves &saves_dir_data, prefs &prefs_data) {
    Pm3GameType game_type = getPm3GameType(game_path.c_str());
    const char* saves_folder = getSavesFolder(game_type);
    if (saves_folder == nullptr) {
        gPm3LastError = "Invalid PM3 folder: could not find PM3 executable in " + game_path;
        return false;
    }

    std::filesystem::path full_path = std::filesystem::path(game_path) / saves_folder;
    bool saves_ok = load_binary_file(full_path / std::string{kSavesDirFile}, saves_dir_data);
    bool prefs_ok = load_binary_file(full_path / std::string{kPrefsFile}, prefs_data);

    if (!saves_ok || !prefs_ok) {
        gPm3LastError = "Could not load PM3 metadata (SAVES.DIR/PREFS) in " + full_path.string() + ". "
                           "Set the PM3 folder in Settings.";
        return false;
    }

    gPm3LastError.clear();
    return true;
}

void saveBinaries(int game_nr, const std::string &game_path, gamea &game_data, gameb &club_data, gamec &player_data) {
    save_binary_file(constructSaveFilePath(game_path, game_nr, 'A'), game_data);
    save_binary_file(constructSaveFilePath(game_path, game_nr, 'B'), club_data);
    save_binary_file(constructSaveFilePath(game_path, game_nr, 'C'), player_data);
}

void saveMetadata(const std::string &game_path, saves &saves_dir_data, prefs &prefs_data) {
    save_binary_file(constructGameFilePath(game_path, std::string{kSavesDirFile}), saves_dir_data);
    save_binary_file(constructGameFilePath(game_path, std::string{kPrefsFile}), prefs_data);
}

void updateMetadata(int game_nr, const std::string &game_path) {
    savesDir.game[game_nr - 1].turn = gameData.turn;
    savesDir.game[game_nr - 1].year = gameData.year;

    saveMetadata(std::filesystem::path(game_path).parent_path(), savesDir, preferences);
}

std::filesystem::path constructSavesFolderPath(const std::string& game_path) {
    Pm3GameType game_type = getPm3GameType(game_path.c_str());
    const char *savesFolder = getSavesFolder(game_type);
    if (savesFolder == nullptr) {
        gPm3LastError = "Invalid PM3 folder: could not find PM3 executable in " + game_path;
        return {};
    }

    return std::filesystem::path(game_path) / savesFolder;
}

std::filesystem::path constructSaveFilePath(const std::string& game_path, int gameNumber, char gameLetter) {
    return constructSavesFolderPath(game_path) / (std::string{kGameFilePrefix} + std::to_string(gameNumber) + gameLetter);
}

std::filesystem::path constructGameFilePath(const std::string &game_path, const std::string &file_name) {
    return std::filesystem::path(game_path) / file_name;
}

Pm3GameType getPm3GameType(const char *game_path) {
    std::filesystem::path full_path;
    full_path = std::filesystem::path(game_path) / kExeStandardFilename;
    if (std::filesystem::exists(full_path)) {
        return Pm3GameType::Standard;
    }

    full_path = std::filesystem::path(game_path) / kExeDeluxeFilename;
    if (std::filesystem::exists(full_path)) {
        return Pm3GameType::Deluxe;
    }

    return Pm3GameType::Unknown;
}

const char* getSavesFolder(Pm3GameType game_type) {
    switch (game_type) {
        case Pm3GameType::Standard:
            return kStandardSavesPath.data();
        case Pm3GameType::Deluxe:
            return kDeluxeSavesPath.data();
        default:
            return nullptr;
    }
}

const std::string& pm3LastError() {
    return gPm3LastError;
}

namespace {
const int saveGameSizes[3] = {29554, 139080, 157280};
}

void loadPrefs(Settings &settings) {
    if (!std::filesystem::exists(PREFS_PATH)) {
        return;
    }
    std::ifstream in(PREFS_PATH, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open the file for reading.\n";
        return;
    }
    settings.deserialize(in);
}

void savePrefs(const Settings &settings) {
    std::ofstream out(PREFS_PATH, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open the file for writing.\n";
        return;
    }
    settings.serialize(out);
}

bool checkSaveFileExists(const Settings &settings, int gameNumber, char gameLetter) {
    return std::filesystem::exists(constructSaveFilePath(settings.gamePath, gameNumber, gameLetter));
}

void memoizeSaveFiles(const Settings &settings, std::bitset<8> &saveFiles) {
    for (int i = 1; i <= 8; i++) {
        saveFiles.set(i - 1,
                      checkSaveFileExists(settings, i, 'A') &&
                      checkSaveFileExists(settings, i, 'B') &&
                      checkSaveFileExists(settings, i, 'C'));
    }
}

bool ensureMetadataLoaded(const Settings &settings, int currentGame, std::bitset<8> &saveFiles, char *footer,
                          size_t footerSize, bool attachClickCallbacks) {
    if (!attachClickCallbacks) {
        return true;
    }

    memoizeSaveFiles(settings, saveFiles);
    if (currentGame == 0) {
        loadDefaultClubdata(settings.gamePath);
    }

    if (!loadMetadata(settings.gamePath)) {
        snprintf(footer, footerSize, "%.64s", pm3LastError().c_str());
        return false;
    }

    return true;
}

bool backupSaveFile(const Settings &settings, int gameNumber) {
    for (char c = 'A'; c <= 'C'; ++c) {
        std::filesystem::path saveGamePath = constructSaveFilePath(settings.gamePath, gameNumber, c);

        try {
            if (!std::filesystem::exists(saveGamePath)) {
                return true;
            }

            std::filesystem::path backupPath = constructSavesFolderPath(settings.gamePath) / BACKUP_SAVE_PATH;

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

bool loadGame(const Settings &settings, int gameNumber, char *footer, size_t footerSize) {
    std::string gameaName = constructSaveFilePath(settings.gamePath, gameNumber, 'A');
    std::string gamebName = constructSaveFilePath(settings.gamePath, gameNumber, 'B');
    std::string gamecName = constructSaveFilePath(settings.gamePath, gameNumber, 'C');

    if (std::filesystem::file_size(gameaName) != static_cast<uintmax_t>(saveGameSizes[0])) {
        snprintf(footer, footerSize, "INVALID %s FILESIZE", gameaName.c_str());
        return false;
    } else if (std::filesystem::file_size(gamebName) != static_cast<uintmax_t>(saveGameSizes[1])) {
        snprintf(footer, footerSize, "INVALID %s FILESIZE", gamebName.c_str());
        return false;
    } else if (std::filesystem::file_size(gamecName) != static_cast<uintmax_t>(saveGameSizes[2])) {
        snprintf(footer, footerSize, "INVALID %s FILESIZE", gamecName.c_str());
        return false;
    }

    loadBinaries(gameNumber, settings.gamePath);
    return true;
}

bool saveGame(const Settings &settings, int gameNumber, char *footer, size_t footerSize) {
    if (backupSaveFile(settings, gameNumber)) {
        updateMetadata(gameNumber, settings.gamePath.string());
        saveBinaries(gameNumber, settings.gamePath);
        saveMetadata(settings.gamePath);
        snprintf(footer, footerSize, "GAME %d SAVED", gameNumber);
        return true;
    }

    snprintf(footer, footerSize, "ERROR SAVING: COULDN'T BACKUP SAVE GAME %d", gameNumber);
    return false;
}

void choosePm3Folder(Settings &settings, std::bitset<8> &saveFiles) {
    NFD_Init();

    nfdchar_t *outPath;

    nfdresult_t result = NFD_PickFolder(&outPath, settings.gamePath.empty() ? nullptr : settings.gamePath.c_str());
    if (result == NFD_OKAY && outPath) {
        settings.gameType = getPm3GameType(outPath);
        settings.gamePath = outPath;
        NFD_FreePath(outPath);
        savePrefs(settings);
        memoizeSaveFiles(settings, saveFiles);
    } else if (result == NFD_ERROR) {
        std::string errorMessage = NFD_GetError() ? NFD_GetError() : "Unknown NFD error";
        NFD_Quit();
        throw std::runtime_error("Unable to select folder\nNFD Error: " + errorMessage);
    }

    NFD_Quit();
}

void loadGameConfirm(InputHandler &input, Settings &settings, int gameNumber, int &currentGame, char *footer,
                     size_t footerSize) {
    snprintf(footer, footerSize, "Load Game %d: Are you sure?", gameNumber);

    auto clearFooterAndCallbacks = [&input, footer]() {
        footer[0] = '\0';
        input.resetKeyPressCallbacks();
    };

    auto loadCallback = [&input, &settings, gameNumber, &currentGame, footer, footerSize]() {
        if (loadGame(settings, gameNumber, footer, footerSize)) {
            currentGame = gameNumber;
            snprintf(footer, footerSize, "GAME %d LOADED", gameNumber);
        }
        input.resetKeyPressCallbacks();
    };

    input.addKeyPressCallback('y', loadCallback);
    input.addKeyPressCallback('Y', loadCallback);
    input.addKeyPressCallback('n', clearFooterAndCallbacks);
    input.addKeyPressCallback('N', clearFooterAndCallbacks);
}

void saveGameConfirm(InputHandler &input, const Settings &settings, int gameNumber, char *footer, size_t footerSize) {
    snprintf(footer, footerSize, "Save Game %d: Are you sure?", gameNumber);

    auto saveCallback = [&input, &settings, gameNumber, footer, footerSize]() {
        saveGame(settings, gameNumber, footer, footerSize);
        input.resetKeyPressCallbacks();
    };

    auto cancelCallback = [&input, footer]() {
        footer[0] = '\0';
        input.resetKeyPressCallbacks();
    };

    input.addKeyPressCallback('y', saveCallback);
    input.addKeyPressCallback('Y', saveCallback);
    input.addKeyPressCallback('n', cancelCallback);
    input.addKeyPressCallback('N', cancelCallback);
}

void formatSaveGameLabel(int i, char *gameLabel, size_t gameLabelSize) {
    snprintf(gameLabel, gameLabelSize, "GAME %1.1d %16.16s %16.16s %3.3s Week %2.2d %4.4d", i,
             savesDir.game[i - 1].manager[0].name, getClub(savesDir.game[i - 1].manager[0].club_idx).name,
             dayNames[savesDir.game[i - 1].turn % 3],
             (savesDir.game[i - 1].turn / 3) + 1, savesDir.game[i - 1].year);
}

} // namespace io
