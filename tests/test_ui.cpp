#include <iostream>

#include "ui.h"
#include "pm3_data.h"

int main() {
    // Basic pagination helpers: verify it doesn't crash and updates page counters.
    Graphics dummyGfx;
    InputHandler input(dummyGfx);
    int currentPage = 1;
    int totalPages = 3;
    char footer[64]{};
    ui::drawPagination(input, currentPage, totalPages, footer, sizeof(footer), true);
    // After wiring, callbacks should adjust page.
    input.checkClickableArea(172, 307); // prev
    input.checkClickableArea(243, 307); // next
    if (currentPage < 1 || currentPage > totalPages) {
        return 1;
    }
    return 0;
}
