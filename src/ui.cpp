// Shared UI helpers for menus.
#include "ui.h"

#include <SDL.h>
#include <SDL_image.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "config/constants.h"
#include "gfx.h"
#include "text.h"
#include "pm3_data.h"
#include "game_utils.h"

namespace ui {
namespace {
struct IconEntry {
    SDL_Rect rect{};
    SDL_Texture *texture = nullptr;
};

std::array<IconEntry, 9> icons{};
int iconsIdx = 0;
} // namespace

void addIcon(Graphics &gfx, InputHandler &input, const char *iconImagePath, int iconPosition,
             const std::function<void(void)> &clickCallback) {
    if (iconPosition > 9) {
        throw std::runtime_error("Unable to draw icon\nMaximum icon position 9. Got " + std::to_string(iconPosition) + "!");
    }

    SDL_Texture *iconTexture = IMG_LoadTexture(gfx.getRenderer(), iconImagePath);
    if (!iconTexture) {
        throw std::runtime_error("Unable to load image '" + std::string(iconImagePath) + "'\nSDL_image Error: " +
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

    iconsIdx++;

    input.addClickableArea(iconRect.x, iconRect.y, iconRect.w, iconRect.h, clickCallback,
                           ClickableAreaType::Persistent);
}

bool drawIcons(Graphics &gfx) {
    for (int i = 0; i < iconsIdx; i++) {
        SDL_RenderCopy(gfx.getRenderer(), icons[i].texture, nullptr, &icons[i].rect);
    }

    return true;
}

void writeDivisionsMenu(ScreenContext &context, int &selectedDivision, int &selectedClub,
                        const char *heading, bool attachClickCallbacks) {
    context.writeSubHeader(heading, 1, nullptr);

    auto resetAreas = context.resetClickableAreas;
    auto setConfigured = context.setClickableAreasConfigured;

    for (size_t i = 0; i < std::size(divisionNames); i++) {
    context.writeText(
            divisionNames[i],
            static_cast<int>(i) + 3,
            context.defaultTextColor(static_cast<int>(i) + 3),
            TEXT_TYPE_SMALL,
                attachClickCallbacks
                ? std::function<void(void)>{ [&selectedDivision, &selectedClub, resetAreas, setConfigured, i] {
                    selectedDivision = static_cast<int>(i);
                    selectedClub = -1;
                    if (resetAreas) {
                        resetAreas();
                    }
                    if (setConfigured) {
                        setConfigured(false);
                    }
                }}
                : nullptr,
                0
        );
    }
}

void writeClubMenu(ScreenContext &context, int &selectedClub, int selectedDivision,
                   const char *heading, bool attachClickCallbacks) {
    context.writeSubHeader(heading, 1, nullptr);

    auto resetAreas = context.resetClickableAreas;
    auto setConfigured = context.setClickableAreasConfigured;

    int textLine = 3;
    int offsetLeft = 0;

    std::vector<int> clubs;

    for (int i = 0; i < 114; ++i) {
        ClubRecord &club = getClub(i);
        if (club.league == divisionHex[selectedDivision]) {
            clubs.push_back(i);
        }
    }

    std::sort(clubs.begin(), clubs.end(), [](int a, int b) {
        return strcmp(getClub(a).name, getClub(b).name) < 0;
    });

    for (auto club_idx: clubs) {
        if (textLine == 15) {
            offsetLeft = SCREEN_WIDTH / 2;
            textLine = 3;
        }

        char clubName[17];
        snprintf(clubName, sizeof(clubName), "%16.16s", getClub(club_idx).name);

        context.writeText(
                clubName,
                textLine,
                context.defaultTextColor(textLine),
                TEXT_TYPE_SMALL,
                attachClickCallbacks
                ? std::function<void(void)>{ [&selectedClub, resetAreas, setConfigured, club_idx] {
                    selectedClub = club_idx;
                    if (resetAreas) {
                        resetAreas();
                    }
                    if (setConfigured) {
                        setConfigured(false);
                    }
                }}
                : nullptr,
                offsetLeft
        );

        textLine++;
    }

    context.writeText(
            "« Back",
            16,
            context.defaultTextColor(16),
            TEXT_TYPE_SMALL,
            attachClickCallbacks
            ? std::function<void(void)>{ [&selectedClub, resetAreas, setConfigured] {
                selectedClub = -1;
                if (resetAreas) {
                    resetAreas();
                }
                if (setConfigured) {
                    setConfigured(false);
                }
            }}
            : nullptr,
            0
    );
}

void drawTopDetails(ScreenContext &context) {
    struct gamea::ManagerRecord &manager = gameData.manager[0];
    ClubRecord &club = getClub(manager.club_idx);

    char line1[55];

    char managerName[17];
    snprintf(managerName, sizeof(managerName), "%16.16s", manager.name);

    char clubName[17];
    snprintf(clubName, sizeof(clubName), "%16.16s", club.name);

    char divisionName[18];
    snprintf(divisionName, sizeof(divisionName), "%17.17s", divisionNames[manager.division]);

    snprintf(line1, sizeof(line1), "%s %s %s", managerName, clubName, divisionName);

    context.writeText(line1, -2, Colors::TEXT_TOP_DETAILS, TEXT_TYPE_PLAYER, nullptr, 0);

    std::string line2 = "CONTRACT: " + std::to_string(manager.contract_length) + " £" + std::to_string(club.bank_account);

    context.writeText(line2.c_str(), -1, Colors::TEXT_TOP_DETAILS, TEXT_TYPE_PLAYER, nullptr, 0);
}

void drawPagination(InputHandler &input, int &currentPage, int totalPages,
                    char *footer, size_t footerSize, bool attachClickCallbacks) {
    if (totalPages <= 1) {
        return;
    }

    std::string pagination = "Page " + std::to_string(currentPage) + " of " + std::to_string(totalPages);
    pagination.resize(17, ' ');

    if (currentPage == 1) {
        pagination += "         Next »";
    } else if (currentPage == totalPages) {
        pagination += "« Prev";
    } else {
        pagination += "« Prev | Next »";
    }

    snprintf(footer, footerSize, "%s", pagination.c_str());

    if (!attachClickCallbacks) {
        return;
    }

    input.addClickableArea(171, 306, 50, 12, [&currentPage] { if (currentPage > 1) --currentPage; },
                           ClickableAreaType::Transient);
    input.addClickableArea(242, 306, 50, 12, [&currentPage, totalPages] { if (currentPage < totalPages) ++currentPage; },
                           ClickableAreaType::Transient);
}

} // namespace ui
