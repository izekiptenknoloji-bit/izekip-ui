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
    // Shadow shape: the element's own bounds, offset and inflated by spreadRadius on all
    // four sides (spread=0 reduces to the old "same size as the element" shape).
    Rect shadowRect{};
    const bool hasShadow = paint.shadow.has_value();
    if (hasShadow) {
        const float spread = paint.shadow->spreadRadius;
        shadowRect = Rect{rect.x + paint.shadow->offsetX - spread, rect.y + paint.shadow->offsetY - spread,
                           rect.width + 2.0f * spread, rect.height + 2.0f * spread};
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
    Color fillColor = paint.fill.color;
    fillColor.a *= paint.opacity;
    ps.fillColorStart = ColorToFloat4(fillColor);
    ps.fillColorEnd = ColorToFloat4(fillColor);
    ps.fillColorMid = ColorToFloat4(fillColor);
    ps.gradientPositions = Float4{0.0f, 1.0f, 1.0f, 2.0f};
    if (paint.fill.gradientDirection && paint.fill.gradientStops.size() >= 2) {
        const auto& stops = paint.fill.gradientStops; // 2 or 3 entries, capped by Widget::OnPaint
        Color stop0Color = stops[0].color;
        stop0Color.a *= paint.opacity;
        ps.fillColorStart = ColorToFloat4(stop0Color);
        if (stops.size() >= 3) {
            Color stop1Color = stops[1].color;
            stop1Color.a *= paint.opacity;
            Color stop2Color = stops[2].color;
            stop2Color.a *= paint.opacity;
            ps.fillColorMid = ColorToFloat4(stop1Color);
            ps.fillColorEnd = ColorToFloat4(stop2Color);
            ps.gradientPositions = Float4{stops[0].position, stops[1].position, stops[2].position, 3.0f};
        } else {
            Color stop1Color = stops[1].color;
            stop1Color.a *= paint.opacity;
            ps.fillColorEnd = ColorToFloat4(stop1Color);
            ps.gradientPositions = Float4{stops[0].position, stops[1].position, 0.0f, 2.0f};
        }
    }
    ps.elementRectCenterHalfSize = Float4{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f,
                                           rect.width * 0.5f, rect.height * 0.5f};
    if (hasShadow) {
        ps.shadowRectCenterHalfSize =
            Float4{shadowRect.x + shadowRect.width * 0.5f, shadowRect.y + shadowRect.height * 0.5f,
                   shadowRect.width * 0.5f, shadowRect.height * 0.5f};
        Color shadowColor = paint.shadow->color;
        shadowColor.a *= paint.opacity;
        ps.shadowColor = ColorToFloat4(shadowColor);
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
    ps.textParams = Float4{};

    return data;
}

RectDrawData ComputeTextQuadDrawData(const Rect& rect, const UVRect& atlasUV, const Color& color,
                                      float glyphScale, FontId fontId, float viewportWidth,
                                      float viewportHeight) {
    RectDrawData data;
    data.vs = VSConstants{rect.x,       rect.y,       rect.width,  rect.height,
                           viewportWidth, viewportHeight, 0.0f,        0.0f,
                           atlasUV.u0,  atlasUV.v0,   atlasUV.u1,  atlasUV.v1};

    PSConstants& ps = data.ps;
    ps.fillColorStart = ColorToFloat4(color);

    // onEdgeValue/255 and kAtlasDistancePerPixel must match GlyphAtlas's bake parameters
    // (see GlyphAtlas.cpp's kOnEdgeValue=180, kPadding=4 -> pixelDistScale=45): each atlas
    // texel of signed distance is worth 45/255 of the normalized [0,1] texture-sample
    // range. A screen-space AA ramp of ~1 pixel therefore needs
    // (1 / glyphScale) atlas pixels of ramp width -- e.g. at glyphScale=0.4 (a 20px label
    // drawn from a 48px-baked atlas), 1 screen pixel covers 2.5 atlas pixels, so the ramp
    // must be proportionally WIDER in atlas-distance units, or the transition collapses
    // into a near-binary threshold and the glyph edges look jagged instead of smooth.
    constexpr float kOnEdgeValueNormalized = 180.0f / 255.0f;
    constexpr float kAtlasDistancePerPixel = 45.0f / 255.0f;
    const float safeScale = glyphScale > 0.01f ? glyphScale : 0.01f;
    const float aaWidth = kAtlasDistancePerPixel / safeScale;
    const float fontIndex = (fontId == FontId::Monospace) ? 1.0f : 0.0f;
    ps.textParams = Float4{1.0f, kOnEdgeValueNormalized, aaWidth, fontIndex};

    return data;
}

RectDrawData ComputeBackdropBlurDrawData(const Rect& rect, const BackdropBlurPaint& paint,
                                          float viewportWidth, float viewportHeight) {
    // Draw quad: just the element's own bounds plus the standard AA padding (same as
    // ComputeRectDrawData's non-shadow case) -- CSS backdrop-filter is clipped to the
    // element's own box, the blurred result never bleeds past it. The GPU-to-GPU copy
    // that fills the scratch texture this shader samples from is a SEPARATE, larger
    // region (rect padded by the blur kernel's radius, computed in each backend's
    // DrawBackdropBlurRect where the copy actually happens) -- without that extra copy
    // padding, sampling near the element's edges would read whatever garbage sits outside
    // the copied region instead of real previously-drawn pixels.
    constexpr float kAaPadding = 2.0f;
    const float quadMinX = rect.x - kAaPadding;
    const float quadMinY = rect.y - kAaPadding;
    const float quadMaxX = rect.x + rect.width + kAaPadding;
    const float quadMaxY = rect.y + rect.height + kAaPadding;

    RectDrawData data;
    data.vs = VSConstants{quadMinX,
                           quadMinY,
                           quadMaxX - quadMinX,
                           quadMaxY - quadMinY,
                           viewportWidth,
                           viewportHeight,
                           0.0f,
                           0.0f,
                           0.0f, 0.0f, 1.0f, 1.0f};

    // Clamped so the single-pass NxN convolution below (src/Renderer/Shaders/RectShader.hlsl)
    // stays bounded: kernel side length is (2*ceil(radius)+1), so 20px -> 41x41 = 1681
    // taps/pixel worst case. A real 2-pass separable blur would remove this cap but needs
    // render-to-texture infra this engine doesn't have yet (named scope cut).
    constexpr float kMaxBackdropBlurRadiusPx = 20.0f;
    const float clampedBlurRadius = std::min(paint.filter.blurRadiusPx, kMaxBackdropBlurRadiusPx);

    PSConstants& ps = data.ps;
    ps.elementRectCenterHalfSize = Float4{rect.x + rect.width * 0.5f, rect.y + rect.height * 0.5f,
                                           rect.width * 0.5f, rect.height * 0.5f};
    ps.params = Float4{paint.cornerRadius, 0.0f, 0.0f, 0.0f};
    ps.backdropParams = Float4{1.0f, clampedBlurRadius, viewportWidth, viewportHeight};
    ps.backdropColorAdjust = Float4{paint.filter.brightness, paint.filter.contrast,
                                     paint.filter.saturate, paint.filter.grayscale};
    ps.backdropHueAdjust = Float4{paint.filter.hueRotateRadians, 0.0f, 0.0f, 0.0f};
    return data;
}

} // namespace IzekipUI::Render::detail
