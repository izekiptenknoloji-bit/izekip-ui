#pragma once

#include <memory>
#include <string>
#include <optional>

#include "Core/Widget.h"
#include "Core/Rect.h"

namespace IzekipUI::Render {
class IRenderer;
}

namespace IzekipUI::Widgets {

class Image : public Widget {
public:
    Image();
    ~Image() = default;

    void SetImagePath(const std::string& path);
    const std::string& GetImagePath() const { return m_imagePath; }

    void SetPreserveAspect(bool preserve) { m_preserveAspect = preserve; }
    bool IsPreservingAspect() const { return m_preserveAspect; }

    void OnPaint(Render::IRenderer& renderer) override;

private:
    // Reads the file header once per path and remembers a failure too, so a missing file is
    // not probed again every frame. Returns false when the image cannot be read.
    bool ProbeImageSize(int& width, int& height) const;

    std::string m_imagePath;
    bool m_preserveAspect = true;
    mutable bool m_probed = false;
    mutable bool m_probeOk = false;
    mutable int m_imageWidth = 0;
    mutable int m_imageHeight = 0;
};

} // namespace IzekipUI::Widgets
