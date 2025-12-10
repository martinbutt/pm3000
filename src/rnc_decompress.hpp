// Minimal Rob Northen Compression (RNC) unpacker, embedded from swos_extract.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rnc {

bool decompress(const std::vector<uint8_t> &input,
                std::vector<uint8_t> &output,
                std::string &error);

} // namespace rnc
