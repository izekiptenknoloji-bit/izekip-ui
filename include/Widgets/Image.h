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
    std::string m_imagePath;
    bool m_preserveAspect = true;
    mutable std::optional<Rect> m_cachedImageDims;
};

} // namespace IzekipUI::Widgets
