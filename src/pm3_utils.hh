#ifndef PM3000_PM3_UTILS_HH
#define PM3000_PM3_UTILS_HH

#include <filesystem>
#include <vector>
#include <cstdint>
#include <string>

#include "pm3_defs.hh"

extern gamea gameData;
extern gameb clubData;
extern gamec playerData;
extern saves savesDir;
extern prefs preferences;

extern char *gameaPath;
extern char *gamebPath;
extern char *gamecPath;
extern char *savesPath;
extern char *prefsPath;

extern FILE *gameaFile;
extern FILE *gamebFile;
extern FILE *gamecFile;
extern FILE *savesFile;
extern FILE *prefsFile;

ClubRecord& getClub(int idx);
PlayerRecord& getPlayer(int16_t idx);

char determinePlayerType(PlayerRecord &player);
uint8_t determinePlayerRating(PlayerRecord &player);
int determinePlayerPrice(const PlayerRecord &player, const ClubRecord &club, int squadSlot);
int determinePlayerImportance(const PlayerRecord &player, const ClubRecord &club);

std::vector<club_player> findFreePlayers();
std::vector<club_player> getMyPlayers(int player);
void levelAggression();
void changeClub(int16_t newClubIdx, const char* gamePath, int player=0);

std::filesystem::path constructSavesFolderPath(const std::string& gamePath);
std::filesystem::path constructSaveFilePath(const std::string& gamePath, int gameNumber, char gameLetter);
std::filesystem::path constructGameFilePath(const std::string &gamePath, const std::string &fileName);
Pm3GameType getPm3GameType(const char *gamePath);
const char* getSavesFolder(Pm3GameType gameType);

void loadBinaries(int gameNumber, const std::string &gamePath, gamea &gameDataOut=gameData, gameb &clubDataOut=clubData, gamec &playerDataOut=playerData);
void loadDefaultGamedata(const std::string &gamePath, gamea &gameDataOut=gameData);
void loadDefaultClubdata(const std::string &gamePath, gameb &clubDataOut=clubData);
void loadDefaultPlaydata(const std::string &gamePath, gamec &playerDataOut=playerData);
bool loadMetadata(const std::string &gamePath, saves &savesDirOut=savesDir, prefs &prefsOut=preferences);
void saveBinaries(int gameNumber, const std::string &gamePath, gamea &gameDataOut=gameData, gameb &clubDataOut=clubData, gamec &playerDataOut=playerData);
void saveMetadata(const std::string &gamePath, saves &savesDirOut=savesDir, prefs &prefsOut=preferences);
void updateMetadata(int gameNumber);

const std::string& pm3LastError();

#endif // PM3000_PM3_UTILS_HH
