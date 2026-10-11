#pragma once

#include <cmath>

namespace IzekipUI {

// Uniform-scale + rotation + translation as one complex-affine map z -> (a + i*s)*z + (tx + i*ty).
// Only similarities are representable on purpose: nested CSS `scale`/`rotate` stay similar
// under composition, so one Transform2D per widget is exact (non-uniform scale would skew).
struct Transform2D {
    float a = 1.0f;
    float s = 0.0f;
    float tx = 0.0f;
    float ty = 0.0f;

    // this ∘ inner: applies `inner` first, then `this`.
    Transform2D Compose(const Transform2D& inner) const {
        return Transform2D{a * inner.a - s * inner.s,
                           a * inner.s + s * inner.a,
                           a * inner.tx - s * inner.ty + tx,
                           a * inner.ty + s * inner.tx + ty};
    }

    void Apply(float x, float y, float& outX, float& outY) const {
        outX = a * x - s * y + tx;
        outY = s * x + a * y + ty;
    }

    void ApplyInverse(float x, float y, float& outX, float& outY) const {
        const float determinant = a * a + s * s;
        const float dx = x - tx;
        const float dy = y - ty;
        outX = (dx * a + dy * s) / determinant;
        outY = (dy * a - dx * s) / determinant;
    }

    // Scale and rotate about the pivot (px, py), the way CSS transform-origin does.
    static Transform2D AboutPivot(float scale, float radians, float px, float py) {
        const float ca = scale * std::cos(radians);
        const float sa = scale * std::sin(radians);
        return Transform2D{ca, sa, px - (ca * px - sa * py), py - (sa * px + ca * py)};
    }
};

} // namespace IzekipUI
