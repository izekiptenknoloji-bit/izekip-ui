#include "Core/Interpolate.h"

namespace IzekipUI {

float Lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

Color LerpColor(const Color& a, const Color& b, float t) {
    return Color{Lerp(a.r, b.r, t), Lerp(a.g, b.g, t), Lerp(a.b, b.b, t), Lerp(a.a, b.a, t)};
}

BoxEdges LerpBoxEdges(const BoxEdges& a, const BoxEdges& b, float t) {
    return BoxEdges{Lerp(a.top, b.top, t), Lerp(a.right, b.right, t), Lerp(a.bottom, b.bottom, t),
                     Lerp(a.left, b.left, t)};
}

} // namespace IzekipUI
