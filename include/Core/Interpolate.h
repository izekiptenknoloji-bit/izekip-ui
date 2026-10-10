#pragma once

#include "Core/Color.h"
#include "Core/Rect.h"

namespace IzekipUI {

float Lerp(float a, float b, float t);
Color LerpColor(const Color& a, const Color& b, float t);
BoxEdges LerpBoxEdges(const BoxEdges& a, const BoxEdges& b, float t);

} // namespace IzekipUI
