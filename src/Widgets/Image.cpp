#include "Widgets/Image.h"

#include "Renderer/IRenderer.h"
#include "Renderer/ImageDecoder.h"
#include "Core/StyleProperties.h"

#include <algorithm>

namespace IzekipUI::Widgets {

namespace {

// Largest rectangle with the image's aspect ratio that fits inside `box`, centered in it.
Rect ContainRect(const Rect& box, int imageWidth, int imageHeight) {
    const float scale = std::min(box.width / static_cast<float>(imageWidth),
                                 box.height / static_cast<float>(imageHeight));
    const float width = imageWidth * scale;
    const float height = imageHeight * scale;
    return Rect{box.x + (box.width - width) * 0.5f, box.y + (box.height - height) * 0.5f, width, height};
}

} // namespace

Image::Image() : Widget("Image") {
    // Images are sized by CSS width/height, not intrinsic size
}

void Image::SetImagePath(const std::string& path) {
    m_imagePath = path;
    m_probed = false;
    m_probeOk = false;
    m_imageWidth = 0;
    m_imageHeight = 0;
}

bool Image::ProbeImageSize(int& width, int& height) const {
    if (!m_probed) {
        m_probed = true;
        Render::ImageInfo info;
        m_probeOk = Render::ReadImageInfo(m_imagePath, info);
        m_imageWidth = info.width;
        m_imageHeight = info.height;
    }
    width = m_imageWidth;
    height = m_imageHeight;
    return m_probeOk;
}

void Image::OnPaint(Render::IRenderer& renderer) {
    // Paint background/border first, then image on top
    Widget::OnPaint(renderer);

    if (m_imagePath.empty()) {
        return;
    }

    Rect rect = GetVisualRect();
    const float opacity = GetEffectiveOpacity();

    // With an unreadable file the box is left as is; the renderer then draws nothing.
    int imageWidth = 0, imageHeight = 0;
    if (m_preserveAspect && ProbeImageSize(imageWidth, imageHeight) && rect.width > 0.0f && rect.height > 0.0f) {
        rect = ContainRect(rect, imageWidth, imageHeight);
    }

    renderer.DrawImage(m_imagePath, rect, opacity);
}

} // namespace IzekipUI::Widgets
