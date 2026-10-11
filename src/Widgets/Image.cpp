#include "Widgets/Image.h"

#include "Renderer/IRenderer.h"
#include "Core/StyleProperties.h"

namespace IzekipUI::Widgets {

Image::Image() : Widget("Image") {
    // Images are sized by CSS width/height, not intrinsic size
}

void Image::SetImagePath(const std::string& path) {
    m_imagePath = path;
    m_cachedImageDims.reset();
}

void Image::OnPaint(Render::IRenderer& renderer) {
    // Paint background/border first, then image on top
    Widget::OnPaint(renderer);

    if (m_imagePath.empty()) {
        return;
    }

    const Rect rect = GetVisualRect();
    float opacity = GetEffectiveOpacity();

    // Request renderer to draw image from file
    // For now, renderer logs warning if image not loaded
    renderer.DrawImage(m_imagePath, rect, opacity);
}

} // namespace IzekipUI::Widgets
