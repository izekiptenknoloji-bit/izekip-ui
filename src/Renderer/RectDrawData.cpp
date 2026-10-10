#include "Renderer/RectDrawData.h"

#include <algorithm>

namespace IzekipUI::Render::detail {

namespace {
Float4 ColorToFloat4(const Color& c) {
    return Float4{c.r, c.g, c.b, c.a};
}
} // namespace

RectDrawData ComputeRectDrawData(const Rect& rect, const RectPaint& paint, float viewportWidth,
                                  float viewportHeight) {
    // Shadow shape: same size as the element, just offset -- that's what makes it read
    // as a "shadow" rather than an arbitrary blob.
    Rect shadowRect{};
    const bool hasShadow = paint.shadow.has_value();
    if (hasShadow) {
        shadowRect = Rect{rect.x + paint.shadow->offsetX, rect.y + paint.shadow->offsetY, rect.width,
                           rect.height};
    }

    // Quad geometry: the element's own bounds padded by a small constant for the AA ramp,
    // unioned with the shadow's bounds inflated by its blur radius (our smoothstep falloff
    // has compact support, so exactly blurRadius of padding is enough -- no extra margin).
    constexpr float kAaPadding = 2.0f;
    float quadMinX = rect.x - kAaPadding;
    float quadMinY = rect.y - kAaPadding;
    float quadMaxX = rect.x + rect.width + kAaPadding;
    float quadMaxY = rect.y + rect.height + kAaPadding;

    if (hasShadow) {
        const float blur = paint.shadow->blurRadius;
        quadMinX = std::min(quadMinX, shadowRect.x - blur);
        quadMinY = std::min(quadMinY, shadowRect.y - blur);
        quadMaxX = std::max(quadMaxX, shadowRect.x + shadowRect.width + blur);
        quadMaxY = std::max(quadMaxY, shadowRect.y + shadowRect.height + blur);
    }

    RectDrawData data;
    data.vs = VSConstants{quadMinX,
                           quadMinY,
                           quadMaxX - quadMinX,
                           quadMaxY - quadMinY,
                           viewportWidth,
                           viewportHeight,
                           0.0f,
                           0.0f};

    PSConstants& ps = data.ps;
    ps.fillColorStart = ColorToFloat4(paint.fill.color);
    ps.fillColorEnd =
        ColorToFloat4(paint.fill.gradientEndColor ? *paint.fill.gradientEndColor : paint.fill.color);
    ps.elementRectCenterHalfSize = Float4{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f,
                                           rect.width * 0.5f, rect.height * 0.5f};
    if (hasShadow) {
        ps.shadowRectCenterHalfSize =
            Float4{shadowRect.x + shadowRect.width * 0.5f, shadowRect.y + shadowRect.height * 0.5f,
                   shadowRect.width * 0.5f, shadowRect.height * 0.5f};
        ps.shadowColor = ColorToFloat4(paint.shadow->color);
    }

    float gradientMode = 0.0f;
    if (paint.fill.gradientDirection) {
        switch (*paint.fill.gradientDirection) {
            case GradientDirection::ToRight: gradientMode = 1.0f; break;
            case GradientDirection::ToLeft: gradientMode = 2.0f; break;
            case GradientDirection::ToTop: gradientMode = 3.0f; break;
            case GradientDirection::ToBottom: gradientMode = 4.0f; break;
        }
    }
    ps.params = Float4{paint.cornerRadius, hasShadow ? paint.shadow->blurRadius : 0.0f, gradientMode,
                        hasShadow ? 1.0f : 0.0f};

    return data;
}

} // namespace IzekipUI::Render::detail
