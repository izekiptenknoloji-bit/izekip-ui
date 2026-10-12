#pragma once

#include "Core/TextBuffer.h"
#include "Core/Widget.h"
#include "Renderer/IRenderer.h"

namespace IzekipUI::Render {
class GlyphAtlas;
}

namespace IzekipUI::Widgets {

// Multi-line editable text area (notepad-style). Deliberately has no dependency on
// Platform/Window.h -- it only knows char32_t and its own TextBuffer. Translating
// Platform::Key into TextBuffer calls (Backspace/Delete/Enter/arrows/...) is the example
// app's job (see examples/03_notepad/main.cpp), the same way Label/Button/Panel stay
// Platform-unaware and main.cpp owns all input-to-widget glue.
//
// Sizing: a fixed CSS width/height box, like Panel -- no intrinsic Yoga MeasureFunc like
// Label has. There is no scroll/clip mechanism anywhere in this engine; text overflowing
// the box is not drawn: OnPaint culls lines and glyphs outside the widget's rect. Scrolling is
// still absent, a named, inherited scope cut.
class TextArea : public Widget {
public:
    TextArea();

    TextBuffer& GetBuffer() { return m_buffer; }
    const TextBuffer& GetBuffer() const { return m_buffer; }

    // Inserts a typed codepoint and makes the caret visible again. Control codepoints
    // (<0x20) are ignored -- Enter/Backspace/Delete/arrows already reach GetBuffer()
    // through the example app's Key callback, so a stray control codepoint reaching here
    // (e.g. Tab as 0x09 via WM_CHAR on Windows) would otherwise insert an invisible,
    // non-rendering character into the buffer.
    void OnCharInput(char32_t codepoint);

    // SetFocused(true) + cursor jumps to end-of-document. v1 click-to-focus only -- no
    // pixel-accurate click-to-nearest-character positioning (named scope cut).
    void FocusAndMoveCursorToEnd();

    // Caret blink timing. No virtual per-frame hook exists on Widget for this (TickAnimations
    // is not virtual) -- call this explicitly once per frame from the example app's main
    // loop, the same way it already explicitly drives hover/active/StyleApplier::Apply.
    void Tick(float deltaSeconds);

    void OnPaint(Render::IRenderer& renderer) override;

private:
    Render::GlyphAtlas& GetAtlas() const;

    TextBuffer m_buffer;
    float m_caretBlinkSeconds = 0.0f;
    bool m_caretVisible = true;
};

} // namespace IzekipUI::Widgets
