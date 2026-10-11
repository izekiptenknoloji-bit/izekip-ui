#include "Widgets/Label.h"

#include <algorithm>

#include "Core/Utf8.h"
#include "Renderer/GlyphAtlas.h"
#include "Renderer/IRenderer.h"

namespace IzekipUI::Widgets {

Label::Label() : Widget("Label") {
    YGNodeSetContext(m_yogaNode, this);
    YGNodeSetMeasureFunc(m_yogaNode, &Label::MeasureFunc);
}

void Label::SetText(std::string text) {
    m_text = std::move(text);

    // Decode + split on '\n' once here -- see the header's doc comment (no word-wrap, no
    // CRLF normalization). MeasureFunc/OnPaint run every layout pass / every frame.
    const std::u32string decoded = detail::DecodeUtf8(m_text);
    m_lines.clear();
    size_t start = 0;
    while (true) {
        const size_t newlinePos = decoded.find(U'\n', start);
        if (newlinePos == std::u32string::npos) {
            m_lines.push_back(decoded.substr(start));
            break;
        }
        m_lines.push_back(decoded.substr(start, newlinePos - start));
        start = newlinePos + 1;
    }

    YGNodeMarkDirty(m_yogaNode);
}

void Label::SetFontFamily(Render::FontId fontFamily) {
    m_fontFamily = fontFamily;
    YGNodeMarkDirty(m_yogaNode);
}

Render::GlyphAtlas& Label::GetAtlas() const {
    const bool monospace = m_style.fontFamily
                               ? *m_style.fontFamily == CssFontFamily::Monospace
                               : m_fontFamily == Render::FontId::Monospace;
    return monospace ? Render::GlyphAtlas::GetMonospaceDefault()
                     : Render::GlyphAtlas::GetDefault();
}

YGSize Label::MeasureFunc(YGNodeConstRef node, float width, YGMeasureMode widthMode, float height,
                           YGMeasureMode heightMode) {
    auto* label = static_cast<Label*>(YGNodeGetContext(node));
    const float fontSize = label->m_style.fontSize.value_or(16.0f);
    auto& atlas = label->GetAtlas();
    const float scale = fontSize / atlas.GetBakedPixelHeight();

    float maxWidth = 0.0f;
    for (const auto& line : label->m_lines) {
        maxWidth = std::max(maxWidth, atlas.MeasureTextWidth(line, fontSize));
    }

    YGSize result{};
    result.width = maxWidth;
    result.height = atlas.GetLineHeight() * scale * static_cast<float>(label->m_lines.size());

    if (widthMode == YGMeasureModeAtMost) {
        result.width = std::min(result.width, width);
    }
    if (heightMode == YGMeasureModeAtMost) {
        result.height = std::min(result.height, height);
    }
    return result;
}

void Label::OnPaint(Render::IRenderer& renderer) {
    Widget::OnPaint(renderer); // background (if any) + children, painted first; glyphs sit on top
    if (m_text.empty()) {
        return;
    }

    const float fontSize = m_style.fontSize.value_or(16.0f);
    Color color = m_style.textColor.value_or(Color{0.0f, 0.0f, 0.0f, 1.0f});
    color.a *= GetEffectiveOpacity();
    auto& atlas = GetAtlas();
    const float scale = fontSize / atlas.GetBakedPixelHeight();
    const float lineHeight = atlas.GetLineHeight() * scale;
    const Rect visualRect = GetVisualRect();
    const Render::FontId fontId =
        (m_style.fontFamily && *m_style.fontFamily == CssFontFamily::Monospace) ||
                (!m_style.fontFamily && m_fontFamily == Render::FontId::Monospace)
            ? Render::FontId::Monospace
            : Render::FontId::Proportional;

    for (size_t lineIndex = 0; lineIndex < m_lines.size(); ++lineIndex) {
        const std::u32string& line = m_lines[lineIndex];
        float penX = visualRect.x;
        const float baselineY =
            visualRect.y + atlas.GetAscent() * scale + lineHeight * static_cast<float>(lineIndex);
        char32_t prev = 0;

        for (char32_t c : line) {
            const auto* glyph = atlas.FindGlyph(c);
            if (!glyph) {
                prev = 0;
                continue;
            }
            if (prev) {
                penX += atlas.GetKerning(prev, c) * scale;
            }

            if (glyph->width > 0.0f && glyph->height > 0.0f) {
                Rect dst{penX + glyph->bearingX * scale, baselineY + glyph->bearingY * scale,
                          glyph->width * scale, glyph->height * scale};
                Render::UVRect uv{glyph->u0, glyph->v0, glyph->u1, glyph->v1};
                renderer.DrawTextQuad(dst, uv, color, scale, fontId);
            }

            penX += glyph->advance * scale;
            prev = c;
        }
    }
}

} // namespace IzekipUI::Widgets
