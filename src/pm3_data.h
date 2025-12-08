// PM3 global state accessors and shared data.
#pragma once

#include <cstdint>
#include <cstdio>

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
