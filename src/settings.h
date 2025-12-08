// Shared settings model for persistence and game configuration.
#pragma once

#include <filesystem>
#include <string>

#include "pm3_defs.hh"

struct Settings {
    std::filesystem::path gamePath = "";
    Pm3GameType gameType = Pm3GameType::Unknown;

    void serialize(std::ostream &out) const {
        std::string pathStr = gamePath.string();
        size_t length = pathStr.size();
        out.write(reinterpret_cast<const char *>(&length), sizeof(length));
        out.write(pathStr.c_str(), length);
    }

    void deserialize(std::istream &in) {
        size_t length;
        in.read(reinterpret_cast<char *>(&length), sizeof(length));
        std::string pathStr(length, '\0');
        in.read(pathStr.data(), length);
        gamePath = pathStr;
    }
};
