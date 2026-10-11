#pragma once

#include <string_view>

namespace IzekipUI {

// CSS-standard cubic-bezier timing function: control points P1=(x1,y1), P2=(x2,y2),
// with fixed endpoints P0=(0,0) and P3=(1,1). Evaluate(x) solves sampleCurveX(t) == x
// for t via Newton-Raphson (bisection fallback if the derivative is ~0), then returns
// sampleCurveY(t) -- the same approach browsers use for CSS cubic-bezier() easing.
struct CubicBezier {
    float x1 = 0.0f;
    float y1 = 0.0f;
    float x2 = 1.0f;
    float y2 = 1.0f;

    float Evaluate(float x) const;
};

// Maps the five CSS-standard easing keywords to their canonical control points.
// Returns false (leaving out untouched) if keyword isn't one of those five -- custom
// cubic-bezier(x1,y1,x2,y2) syntax is a later pass, not supported here.
bool TryGetNamedEasing(std::string_view keyword, CubicBezier& out);

} // namespace IzekipUI
