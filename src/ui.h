// Shared UI helpers for common menus and pagination pieces.
#pragma once

#include "input.h"
#include "screens/screen.h"

namespace ui {

void addIcon(Graphics &gfx, InputHandler &input, const char *iconImagePath, int iconPosition,
             const std::function<void(void)> &clickCallback);
bool drawIcons(Graphics &gfx);

void writeDivisionsMenu(ScreenContext &context, int &selectedDivision, int &selectedClub,
                        const char *heading, bool attachClickCallbacks);

void writeClubMenu(ScreenContext &context, int &selectedClub, int selectedDivision,
                   const char *heading, bool attachClickCallbacks);

void drawTopDetails(ScreenContext &context);

void drawPagination(InputHandler &input, int &currentPage, int totalPages,
                    char *footer, size_t footerSize, bool attachClickCallbacks);

} // namespace ui
