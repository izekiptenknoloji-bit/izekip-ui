#include "Core/Easing.h"

#include <cmath>

namespace IzekipUI {

namespace {

// WebKit's classic cubic-bezier UnitBezier formulation: P0=(0,0), P3=(1,1).
float SampleCurve(float a, float b, float c, float t) {
    return ((a * t + b) * t + c) * t;
}

float SampleCurveDerivative(float a, float b, float c, float t) {
    return (3.0f * a * t + 2.0f * b) * t + c;
}

} // namespace

float CubicBezier::Evaluate(float x) const {
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;

    const float cx = 3.0f * x1;
    const float bx = 3.0f * (x2 - x1) - cx;
    const float ax = 1.0f - cx - bx;

    const float cy = 3.0f * y1;
    const float by = 3.0f * (y2 - y1) - cy;
    const float ay = 1.0f - cy - by;

    // Newton-Raphson to solve sampleCurveX(t) == x.
    float t = x;
    constexpr int kMaxNewtonIterations = 8;
    constexpr float kEpsilon = 1e-5f;
    for (int i = 0; i < kMaxNewtonIterations; ++i) {
        const float currentX = SampleCurve(ax, bx, cx, t) - x;
        if (std::fabs(currentX) < kEpsilon) {
            return SampleCurve(ay, by, cy, t);
        }
        const float derivative = SampleCurveDerivative(ax, bx, cx, t);
        if (std::fabs(derivative) < kEpsilon) {
            break;
        }
        t -= currentX / derivative;
    }

    // Bisection fallback if Newton-Raphson didn't converge.
    float lo = 0.0f, hi = 1.0f;
    t = x;
    while (lo < hi) {
        const float currentX = SampleCurve(ax, bx, cx, t);
        if (std::fabs(currentX - x) < kEpsilon) {
            break;
        }
        if (currentX < x) {
            lo = t;
        } else {
            hi = t;
        }
        t = (lo + hi) * 0.5f;
    }
    return SampleCurve(ay, by, cy, t);
}

bool TryGetNamedEasing(std::string_view keyword, CubicBezier& out) {
    if (keyword == "linear") {
        out = CubicBezier{0.0f, 0.0f, 1.0f, 1.0f};
        return true;
    }
    if (keyword == "ease") {
        out = CubicBezier{0.25f, 0.1f, 0.25f, 1.0f};
        return true;
    }
    if (keyword == "ease-in") {
        out = CubicBezier{0.42f, 0.0f, 1.0f, 1.0f};
        return true;
    }
    if (keyword == "ease-out") {
        out = CubicBezier{0.0f, 0.0f, 0.58f, 1.0f};
        return true;
    }
    if (keyword == "ease-in-out") {
        out = CubicBezier{0.42f, 0.0f, 0.58f, 1.0f};
        return true;
    }
    return false;
}

} // namespace IzekipUI
