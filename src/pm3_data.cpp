#include "pm3_data.h"

gamea gameData;
gameb clubData;
gamec playerData;
saves savesDir;
prefs preferences;

ClubRecord& getClub(int idx) {
    return clubData.club[idx];
}

PlayerRecord& getPlayer(int16_t idx) {
    return playerData.player[idx];
}
