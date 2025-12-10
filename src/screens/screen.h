// Screen interface and shared context callbacks.
#pragma once

#include <functional>
#include <filesystem>
#include <bitset>
#include <vector>
#include <SDL.h>
#include "pm3_defs.hh"

struct ScreenContext {
    std::function<void(const char *)> drawBackground;
    std::function<void(const char *, int, const std::function<void(void)> &)> writeTextLarge;
    std::function<void(const char *, int, SDL_Color, int, const std::function<void(void)> &, int)> writeText;
    std::function<void(const char *, int, int, int, SDL_Color, int, const std::function<void(void)> &)> addTextBlock;
    std::function<SDL_Color(int)> defaultTextColor;
    std::function<int()> currentGame;
    std::function<int()> currentPage;
    std::function<void(int, int)> setPagination;
    std::function<const std::filesystem::path &()> gamePath;
    std::function<Pm3GameType()> gameType;
    std::function<void()> choosePm3Folder;
    std::function<void()> importSwosTeams;
    std::function<void()> levelAggression;
    std::function<void(const char *)> setFooter;
    std::function<bool(bool)> ensureMetadataLoaded;
    std::function<void(int, char *, size_t)> formatSaveGameLabel;
    std::function<const std::bitset<8> &()> saveFiles;
    std::function<void(int)> loadGameConfirm;
    std::function<void(int)> saveGameConfirm;
    std::function<void(const char *, int, const std::function<void(void)> &)> writeHeader;
    std::function<void(const char *, int, const std::function<void(void)> &)> writeSubHeader;
    std::function<std::vector<club_player>&()> freePlayersRef;
    std::function<void()> refreshFreePlayers;
    std::function<int(std::vector<club_player> &, int &, const std::function<void(const club_player &)> &)> writePlayers;
    std::function<void(const char *)> setFooterLine;
    std::function<void()> resetTextBlocks;
    std::function<int()> selectedDivision;
    std::function<int()> selectedClub;
    std::function<void()> resetSelection;
    std::function<void()> resetClickableAreas;
    std::function<void(bool)> setClickableAreasConfigured;
    std::function<void(SDL_Keycode, const std::function<void(void)> &)> addKeyPressCallback;
    std::function<void()> resetKeyPressCallbacks;
    std::function<void(std::function<void(void)>)> startReadingTextInput;
    std::function<void()> endReadingTextInput;
    std::function<const char *()> currentTextInput;
    std::function<void(const club_player &)> makeOffer;
    std::function<void(const char *, bool)> writeDivisionsMenu;
    std::function<void(const char *, bool)> writeClubMenu;
    std::function<void(struct gamea::ManagerRecord &, ClubRecord &, int8_t)> convertPlayerToCoach;
    std::function<void(const char *, char, int, const std::function<void(void)> &)> writePlayer;
};

class Screen {
public:
    virtual ~Screen() = default;
    virtual void draw(bool attachClickCallbacks) = 0;
};
