#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "Core/Color.h"
#include "Core/Rect.h"
#include "Core/Transform2D.h"

namespace IzekipUI::Render {

enum class RendererBackend {
    D3D11,
    Vulkan,
};

struct RendererCreateInfo {
    void* nativeWindowHandle = nullptr; // HWND on Windows; used by the D3D11 backend only
    void* glfwWindow = nullptr;         // GLFWwindow*; used by the Vulkan backend only
    uint32_t initialWidth = 0;
    uint32_t initialHeight = 0;
};

// IRenderer's own copy of this vocabulary -- deliberately not IzekipUI::GradientDirection.
// IRenderer is a "paint this shape" concept, unaware of the CSS cascade; Widget::OnPaint is
// the one seam that translates between the two.
enum class GradientDirection { ToRight, ToLeft, ToTop, ToBottom };

struct RectShadow {
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    float blurRadius = 0.0f;
    float spreadRadius = 0.0f;
    Color color;
};

// A single gradient stop with its position already resolved to [0,1] along the gradient
// axis (CSS percent/auto-spacing resolution happens in Widget::OnPaint, the translation
// seam -- IRenderer only ever sees normalized positions).
struct GradientStop {
    Color color;
    float position = 0.0f;
};

struct RectFill {
    Color color;                                          // flat fill color
    std::optional<GradientDirection> gradientDirection;    // nullopt => flat fill
    // 2 or 3 resolved stops (more are accepted by CSS but capped to 3 before reaching
    // here -- see Widget::OnPaint's named scope cut). Ignored when gradientDirection is
    // nullopt.
    std::vector<GradientStop> gradientStops;
};

struct RectPaint {
    RectFill fill;
    float cornerRadius = 0.0f;
    std::optional<RectShadow> shadow;
    float opacity = 1.0f; // [0,1] alpha multiplier applied to the entire rect
};

// Normalized [0,1] UV rect into the font atlas texture -- IRenderer's own copy of the
// concept GlyphAtlas::GlyphInfo carries, so IRenderer stays unaware of GlyphAtlas/
// stb_truetype the same way it stays unaware of the CSS cascade. Widget::OnPaint (here,
// Label::OnPaint) is the translation seam.
struct UVRect {
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
};

// Which baked glyph atlas (and GPU texture/sampler slot) a DrawTextQuad call samples from.
// Font selection is deliberately limited to the two atlases bundled with the renderer.
// Index into the font atlas array in both backends: the value is the array slice the shader
// samples. Keep this order in sync with GlyphAtlas's font getters and the renderers' uploads.
enum class FontId { Proportional = 0, Monospace = 1, Arial = 2, Serif = 3, Courier = 4 };
inline constexpr int kFontCount = 5;

// backdrop-filter parameters, already defaulted to "no-op" for any field the CSS didn't
// set (see StyleProperties::BackdropFilter, the CSS-side equivalent -- Widget::OnPaint is
// the translation seam that fills in these defaults).
struct BackdropFilterParams {
    float blurRadiusPx = 0.0f;
    float brightness = 1.0f;
    float contrast = 1.0f;
    float saturate = 1.0f;
    float grayscale = 0.0f;
    float hueRotateRadians = 0.0f;
};

struct BackdropBlurPaint {
    float cornerRadius = 0.0f;
    BackdropFilterParams filter;
};

// Backend-agnostic rendering surface. A widget tree calls DrawRect without knowing
// whether D3D11 or Vulkan is behind it.
class IRenderer {
public:
    virtual ~IRenderer() = default;

    virtual bool Initialize(const RendererCreateInfo& info) = 0;
    virtual void Shutdown() = 0;

    virtual void Resize(uint32_t width, uint32_t height) = 0;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;

    virtual void DrawRect(const Rect& rect, const RectPaint& paint) = 0;

    // One glyph quad, sampled from the font atlas texture and tinted by color. Called
    // once per visible character by Label::OnPaint. glyphScale is (requestedFontSize /
    // GlyphAtlas::GetBakedPixelHeight()) -- the SDF anti-aliasing ramp width must widen
    // or narrow with it so the on-screen transition stays ~1 screen pixel regardless of
    // how far the baked atlas is being up- or down-scaled.
    virtual void DrawTextQuad(const Rect& rect, const UVRect& atlasUV, const Color& color,
                               float glyphScale, FontId fontId = FontId::Proportional) = 0;

    // Samples whatever has already been painted BEHIND `rect` (a GPU-to-GPU copy of the
    // render target so far), blurs/color-adjusts it per `paint.filter`, and draws the
    // result into `rect` -- the actual backdrop-filter effect. Must be called before the
    // widget's own (usually translucent) fill is drawn on top; see Widget::OnPaint.
    virtual void DrawBackdropBlurRect(const Rect& rect, const BackdropBlurPaint& paint) = 0;

    // Draw image from file path. If image cannot be loaded, does nothing (silent fail).
    virtual void DrawImage(const std::string& imagePath, const Rect& rect, float opacity) = 0;

    // Applies to every subsequent DrawRect / DrawTextQuad / DrawBackdropBlurRect until changed:
    // rect and glyph coordinates are layout-space, and this maps them to screen pixels.
    // Widget::OnPaint sets it to the widget's world transform before drawing its own content.
    virtual void SetWorldTransform(const Transform2D& transform) = 0;

    // Test-only hooks for device-loss recovery: the next Present() rebuilds the device. Not
    // covered by the API stability policy. Backends without recovery leave the defaults.
    virtual void SimulateDeviceLossForTesting() {}
    virtual bool IsDeviceLostForTesting() const { return false; }

    virtual RendererBackend GetBackend() const = 0;
};

std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend);

// The backend this library was actually compiled with (IZEKIP_RENDER_BACKEND at configure
// time). Lets consumers avoid hardcoding a RendererBackend when calling CreateRenderer.
RendererBackend GetCompiledInBackend();

} // namespace IzekipUI::Render
