// IO/persistence helpers for saves, metadata, and prefs.
#pragma once

#include <bitset>
#include <filesystem>

#include "settings.h"
#include "pm3_defs.hh"
#include "pm3_data.h"

class InputHandler;

namespace io {

void loadPrefs(Settings &settings);
void savePrefs(const Settings &settings);

bool checkSaveFileExists(const Settings &settings, int gameNumber, char gameLetter);
void memoizeSaveFiles(const Settings &settings, std::bitset<8> &saveFiles);

bool ensureMetadataLoaded(const Settings &settings, int currentGame, std::bitset<8> &saveFiles, char *footer,
                          size_t footerSize, bool attachClickCallbacks);

bool backupSaveFile(const Settings &settings, int gameNumber);
bool loadGame(const Settings &settings, int gameNumber, char *footer, size_t footerSize);
bool saveGame(const Settings &settings, int gameNumber, char *footer, size_t footerSize);

void choosePm3Folder(Settings &settings, std::bitset<8> &saveFiles);
void loadGameConfirm(InputHandler &input, Settings &settings, int gameNumber, int &currentGame, char *footer,
                     size_t footerSize);
void saveGameConfirm(InputHandler &input, const Settings &settings, int gameNumber, char *footer, size_t footerSize);
void formatSaveGameLabel(int i, char *gameLabel, size_t gameLabelSize);

void loadBinaries(int gameNumber, const std::string &gamePath, gamea &gameDataOut=gameData, gameb &clubDataOut=clubData, gamec &playerDataOut=playerData);
void loadDefaultGamedata(const std::string &gamePath, gamea &gameDataOut=gameData);
void loadDefaultClubdata(const std::string &gamePath, gameb &clubDataOut=clubData);
void loadDefaultPlaydata(const std::string &gamePath, gamec &playerDataOut=playerData);
bool loadMetadata(const std::string &gamePath, saves &savesDirOut=savesDir, prefs &prefsOut=preferences);
void saveBinaries(int gameNumber, const std::string &gamePath, gamea &gameDataOut=gameData, gameb &clubDataOut=clubData, gamec &playerDataOut=playerData);
void saveMetadata(const std::string &gamePath, saves &savesDirOut=savesDir, prefs &prefsOut=preferences);
void updateMetadata(int gameNumber, const std::string &gamePath);
std::filesystem::path constructSavesFolderPath(const std::string& gamePath);
std::filesystem::path constructSaveFilePath(const std::string& gamePath, int gameNumber, char gameLetter);
std::filesystem::path constructGameFilePath(const std::string &gamePath, const std::string &fileName);
Pm3GameType getPm3GameType(const char *gamePath);
const char* getSavesFolder(Pm3GameType gameType);
const std::string& pm3LastError();

} // namespace io
