#include "save_game_screen.h"

#include "config/constants.h"
#include "text.h"
#include "io.h"

void SaveGameScreen::draw(bool attachClickCallbacks) {
    if (!context.ensureMetadataLoaded(attachClickCallbacks)) {
        context.writeHeader("Save Game", 1, nullptr);
        context.writeText(io::pm3LastError().c_str(), 4, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr, 0);
        return;
    }

    context.writeHeader("Save Game", 1, nullptr);

    const auto &saveFiles = context.saveFiles();

    if (saveFiles.none()) {
        context.writeText("No valid save files found", 2, Colors::TEXT_1, TEXT_TYPE_SMALL, nullptr, 0);
        return;
    }

    context.writeText("Choose game to save", 2, Colors::TEXT_SUB_HEADING, TEXT_TYPE_SMALL, nullptr, 0);

    for (int i = 1; i <= 8; i++) {
        if (!saveFiles.test(i - 1)) {
            continue;
        }

        char gameLabel[62];
        context.formatSaveGameLabel(i, gameLabel, sizeof(gameLabel));

        SDL_Color rowColor = context.defaultTextColor(i + 2);
        context.writeText(
                gameLabel,
                i + 2,
                rowColor,
                TEXT_TYPE_SMALL,
                attachClickCallbacks ? std::function<void(void)>{[this, i] { context.saveGameConfirm(i); }}
                                     : nullptr,
                0
        );
    }
}
