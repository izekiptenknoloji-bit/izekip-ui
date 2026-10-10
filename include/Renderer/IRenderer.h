#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "Core/Color.h"
#include "Core/Rect.h"

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
    Color color;
};

struct RectFill {
    Color color;                                          // flat fill, or gradient start stop
    std::optional<GradientDirection> gradientDirection;    // nullopt => flat fill
    std::optional<Color> gradientEndColor;
};

struct RectPaint {
    RectFill fill;
    float cornerRadius = 0.0f;
    std::optional<RectShadow> shadow;
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

    virtual RendererBackend GetBackend() const = 0;
};

std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend);

// The backend this library was actually compiled with (IZEKIP_RENDER_BACKEND at configure
// time). Lets consumers avoid hardcoding a RendererBackend when calling CreateRenderer.
RendererBackend GetCompiledInBackend();

} // namespace IzekipUI::Render
