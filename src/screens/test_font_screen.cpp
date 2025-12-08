#include "test_font_screen.h"

#include "config/constants.h"
#include "text.h"

#include <string>

void TestFontScreen::draw([[maybe_unused]] bool attachClickCallbacks) {
    for (int x = 0; x < 29; ++x) {
        char a = static_cast<char>(x + 1);
        char b = static_cast<char>(x + 30);
        char c = static_cast<char>(x + 59);
        char d = static_cast<char>(x + 88);
        char e = static_cast<char>(x + 117);
        char f = static_cast<char>(x + 146);
        char g = static_cast<char>(x + 175);
        char h = static_cast<char>(x + 204);
        char i = static_cast<char>(x + 233);

        std::string testText = std::to_string(x) + ": " + a + "  " +
                               std::to_string(x + 29) + ": " + b + "  " +
                               std::to_string(x + 58) + ": " + c + "  " +
                               std::to_string(x + 87) + ": " + d + "  " +
                               std::to_string(x + 116) + ": " + e + "  " +
                               std::to_string(x + 145) + ": " + f + "  " +
                               std::to_string(x + 175) + ": " + g + "  " +
                               std::to_string(x + 204) + ": " + h + "  " +
                               std::to_string(x + 233) + ": " + i + "  ";

        int textLine = (x % 29) + 2;

        context.writeText(
                testText.c_str(),
                textLine,
                context.defaultTextColor(textLine),
                TEXT_TYPE_PLAYER,
                nullptr, 0);
    }
}
