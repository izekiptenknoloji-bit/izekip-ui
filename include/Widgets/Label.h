#pragma once

#include <vector>

#include "Core/Widget.h"
#include "Renderer/IRenderer.h"

namespace IzekipUI::Render {
class GlyphAtlas;
}

namespace IzekipUI::Widgets {

class Label : public Widget {
public:
    Label();

    void SetText(std::string text);
    const std::string& GetText() const { return m_text; }

    // C++-only font selection -- there is no CSS `font-family` property (see IRenderer.h's
    // FontId doc comment for why). Default: Render::FontId::Proportional.
    void SetFontFamily(Render::FontId fontFamily);

    void OnPaint(Render::IRenderer& renderer) override;

private:
    static YGSize MeasureFunc(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height,
                               YGMeasureMode heightMode);
    Render::GlyphAtlas& GetAtlas() const;

    std::string m_text;
    // Lines split from m_text on literal '\n' only -- no automatic word-wrap, no CRLF
    // normalization (named scope cut, see Phase 7 plan). Recomputed in SetText so
    // MeasureFunc/OnPaint never re-split per call.
    std::vector<std::string> m_lines;
    Render::FontId m_fontFamily = Render::FontId::Proportional;
};

} // namespace IzekipUI::Widgets
