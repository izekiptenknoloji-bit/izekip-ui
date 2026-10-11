#pragma once

#include <string>
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

    // Selects the fallback font when CSS does not set font-family.
    void SetFontFamily(Render::FontId fontFamily);

    void OnPaint(Render::IRenderer& renderer) override;

private:
    static YGSize MeasureFunc(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height,
                               YGMeasureMode heightMode);
    Render::GlyphAtlas& GetAtlas() const;

    std::string m_text; // as given to SetText (UTF-8)
    // m_text decoded from UTF-8 to codepoints, then split on '\n' -- no automatic word-wrap,
    // no CRLF normalization (named scope cut). Decoded once in SetText so MeasureFunc/OnPaint
    // never re-decode per call. Codepoints (not bytes) so Turkish characters and the rest of
    // the glyph atlas's Latin-1 + Latin Extended-A coverage render correctly.
    std::vector<std::u32string> m_lines;
    Render::FontId m_fontFamily = Render::FontId::Proportional;
};

} // namespace IzekipUI::Widgets
