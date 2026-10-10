#include "Core/Color.h"

#include <cstdlib>
#include <string>

namespace IzekipUI {

namespace {
int HexPairToInt(std::string_view hex, size_t offset) {
    return static_cast<int>(strtol(std::string(hex.substr(offset, 2)).c_str(), nullptr, 16));
}
} // namespace

Color Color::FromHex(std::string_view hex) {
    if (!hex.empty() && hex.front() == '#') {
        hex.remove_prefix(1);
    }

    Color color;
    if (hex.size() == 6 || hex.size() == 8) {
        color.r = HexPairToInt(hex, 0) / 255.0f;
        color.g = HexPairToInt(hex, 2) / 255.0f;
        color.b = HexPairToInt(hex, 4) / 255.0f;
        color.a = hex.size() == 8 ? HexPairToInt(hex, 6) / 255.0f : 1.0f;
    }
    return color;
}

} // namespace IzekipUI
