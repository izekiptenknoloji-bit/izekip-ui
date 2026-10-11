#include "Widgets/TextArea.h"

#include "Renderer/GlyphAtlas.h"

namespace IzekipUI::Widgets {

namespace {
constexpr float kBlinkIntervalSeconds = 0.5f;
constexpr float kDefaultFontSize = 16.0f;
} // namespace

TextArea::TextArea() : Widget("TextArea") {
    // No YGNodeSetMeasureFunc call -- fixed CSS width/height box, like Panel, not an
    // intrinsically-measured leaf like Label. See the header's doc comment.
    SetFocusable(true);
}

Render::GlyphAtlas& TextArea::GetAtlas() const {
    return Render::GlyphAtlas::GetDefault();
}

void TextArea::OnCharInput(char32_t codepoint) {
    if (codepoint < 0x20) {
        return;
    }
    m_buffer.InsertCodepoint(codepoint);
    m_caretVisible = true;
    m_caretBlinkSeconds = 0.0f;
}

void TextArea::FocusAndMoveCursorToEnd() {
    SetFocused(true);
    m_buffer.MoveDocumentEnd();
    m_caretVisible = true;
    m_caretBlinkSeconds = 0.0f;
}

void TextArea::Tick(float deltaSeconds) {
    m_caretBlinkSeconds += deltaSeconds;
    if (m_caretBlinkSeconds >= kBlinkIntervalSeconds) {
        m_caretBlinkSeconds = 0.0f;
        m_caretVisible = !m_caretVisible;
    }
}

void TextArea::OnPaint(Render::IRenderer& renderer) {
    Widget::OnPaint(renderer); // background/border/shadow/children first, glyphs on top

    const float fontSize = m_style.fontSize.value_or(kDefaultFontSize);
    const Color color = m_style.textColor.value_or(Color{0.0f, 0.0f, 0.0f, 1.0f});
    auto& atlas = GetAtlas();
    const float scale = fontSize / atlas.GetBakedPixelHeight();
    const float lineHeight = atlas.GetLineHeight() * scale;

    const auto& lines = m_buffer.Lines();
    for (size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
        const std::u32string& line = lines[lineIndex];
        float penX = m_rect.x;
        const float baselineY =
            m_rect.y + atlas.GetAscent() * scale + lineHeight * static_cast<float>(lineIndex);
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
                renderer.DrawTextQuad(dst, uv, color, scale, Render::FontId::Proportional);
            }

            penX += glyph->advance * scale;
            prev = c;
        }
    }

    if (IsFocused() && m_caretVisible) {
        const TextBuffer::Position cursor = m_buffer.GetCursor();
        const std::u32string& cursorLine = lines[cursor.line];
        const float caretX =
            m_rect.x + atlas.MeasureTextWidth(cursorLine.substr(0, cursor.column), fontSize);
        const float caretY = m_rect.y + lineHeight * static_cast<float>(cursor.line);

        Render::RectPaint caretPaint;
        caretPaint.fill.color = color;
        renderer.DrawRect(Rect{caretX, caretY, 2.0f, lineHeight}, caretPaint);
    }
}

} // namespace IzekipUI::Widgets
