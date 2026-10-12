#undef NDEBUG // tests must check their assertions in every configuration, including Release
#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Core/Widget.h"
#include "Renderer/IRenderer.h"
#include "Widgets/TextArea.h"

using namespace IzekipUI;

namespace {

// Counts glyph quads; every other call is a no-op. Enough to observe what TextArea::OnPaint emits.
class GlyphCountingRenderer : public Render::IRenderer {
public:
    bool Initialize(const Render::RendererCreateInfo&) override { return true; }
    void Shutdown() override {}
    void Resize(uint32_t, uint32_t) override {}
    void BeginFrame() override {}
    void EndFrame() override {}
    void Present() override {}
    void DrawRect(const Rect&, const Render::RectPaint&) override {}
    void DrawTextQuad(const Rect&, const Render::UVRect&, const Color&, float, Render::FontId) override {
        ++glyphQuads;
    }
    void DrawBackdropBlurRect(const Rect&, const Render::BackdropBlurPaint&) override {}
    void DrawImage(const std::string&, const Rect&, float) override {}
    void SetWorldTransform(const Transform2D&) override {}
    Render::RendererBackend GetBackend() const override { return Render::RendererBackend::D3D11; }

    int glyphQuads = 0;
};

// Lays out a TextArea of the given CSS height as its own root and returns the glyph quads painted.
int PaintedGlyphsFor(const std::string& heightCss, const std::string& text) {
    auto area = std::make_shared<Widgets::TextArea>();
    area->GetBuffer().SetTextUtf8(text);

    const std::string css = "TextArea { width: 200px; height: " + heightCss + "; font-size: 16px; }";
    const Css::StyleSheet sheet = Css::StyleSheet::LoadFromString(css);
    Css::LexborBridge bridge;
    bridge.Build(*area);
    Css::StyleApplier::Apply(bridge, sheet, *area);
    area->CalculateLayout(400.0f, 600.0f);

    GlyphCountingRenderer renderer;
    area->OnPaint(renderer);
    return renderer.glyphQuads;
}

// Lines outside the widget's box are culled: a 60px box shows a few of the 200 lines, a tall box
// shows all of them. Each line is a single "x" glyph, so the glyph count equals the visible lines.
void TestLinesOutsideTheBoxAreCulled() {
    std::string text;
    for (int i = 0; i < 200; ++i) {
        text += "x";
        if (i + 1 < 200) text += '\n';
    }

    const int shortBox = PaintedGlyphsFor("60px", text);
    assert(shortBox > 0);
    assert(shortBox < 10); // ~60px / one 16px line height

    const int tallBox = PaintedGlyphsFor("4000px", text);
    assert(tallBox == 200);

    std::cout << "TestLinesOutsideTheBoxAreCulled passed\n";
}

} // namespace

int main() {
    TestLinesOutsideTheBoxAreCulled();
    std::cout << "All TextAreaTests passed.\n";
    return 0;
}
