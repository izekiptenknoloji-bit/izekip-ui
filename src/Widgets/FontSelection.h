#pragma once

#include <optional>

#include "Core/StyleProperties.h"
#include "Renderer/IRenderer.h"

namespace IzekipUI::Widgets::FontSelect {

// CSS font-family wins when set; otherwise the widget's own FontId (SetFontFamily) applies.
inline Render::FontId ResolveFontId(const std::optional<CssFontFamily>& css, Render::FontId fallback) {
    if (!css) {
        return fallback;
    }
    switch (*css) {
        case CssFontFamily::Proportional: return Render::FontId::Proportional;
        case CssFontFamily::Monospace: return Render::FontId::Monospace;
        case CssFontFamily::Arial: return Render::FontId::Arial;
        case CssFontFamily::Serif: return Render::FontId::Serif;
        case CssFontFamily::Courier: return Render::FontId::Courier;
    }
    return fallback;
}

} // namespace IzekipUI::Widgets::FontSelect
