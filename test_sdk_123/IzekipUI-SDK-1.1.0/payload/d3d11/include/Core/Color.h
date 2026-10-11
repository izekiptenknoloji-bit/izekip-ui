#pragma once

#include <string_view>

namespace IzekipUI {

struct Color {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;

    // Accepts "#RRGGBB" or "#RRGGBBAA".
    static Color FromHex(std::string_view hex);

    // Exact compare is safe here: both sides always come from the same deterministic
    // hex-parse or lerp, never from independent floating-point computation.
    bool operator==(const Color& other) const {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    bool operator!=(const Color& other) const { return !(*this == other); }
};

} // namespace IzekipUI
