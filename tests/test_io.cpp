#include <filesystem>
#include <iostream>

#include "io.h"

int main() {
    // Ensure constructSavesFolderPath handles missing path gracefully.
    try {
        auto path = io::constructSavesFolderPath("");
        std::cout << "Constructed path: " << path << "\n";
    } catch (...) {
        std::cerr << "constructSavesFolderPath threw\n";
        return 1;
    }

    // loadMetadata on an invalid path should fail and set pm3LastError.
    saves savesData{};
    prefs prefsData{};
    if (io::loadMetadata("", savesData, prefsData)) {
        std::cerr << "loadMetadata unexpectedly succeeded on empty path\n";
        return 1;
    }
    if (io::pm3LastError().empty()) {
        std::cerr << "pm3LastError not populated on failure\n";
        return 1;
    }

    return 0;
}
