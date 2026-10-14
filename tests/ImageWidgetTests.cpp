#undef NDEBUG // tests must check their assertions in every configuration, including Release
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

#include "BmpFixtures.h"
#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Core/Widget.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Image.h"

using namespace IzekipUI;

namespace {

// Records every DrawImage call; everything else is a no-op.
class ImageCallRecorder : public Render::IRenderer {
public:
    bool Initialize(const Render::RendererCreateInfo&) override { return true; }
    void Shutdown() override {}
    void Resize(uint32_t, uint32_t) override {}
    void BeginFrame() override {}
    void EndFrame() override {}
    void Present() override {}
    void DrawRect(const Rect&, const Render::RectPaint&) override {}
    void DrawTextQuad(const Rect&, const Render::UVRect&, const Color&, float, Render::FontId) override {}
    void DrawBackdropBlurRect(const Rect&, const Render::BackdropBlurPaint&) override {}
    void DrawImage(const std::string& path, const Rect& rect, float) override {
        ++calls;
        lastPath = path;
        lastRect = rect;
    }
    void SetWorldTransform(const Transform2D&) override {}
    Render::RendererBackend GetBackend() const override { return Render::RendererBackend::D3D11; }

    int calls = 0;
    std::string lastPath;
    Rect lastRect;
};

// The widget reads only the header, so the fixture's pixel values do not matter here.
void WriteTwoByTwoBmp(const std::string& name) {
    IzekipUI::TestFixtures::WriteFile(name, IzekipUI::TestFixtures::TwoByTwoBmp());
}

// Lays out an Image with the given CSS, points it at `path`, and returns what was drawn.
ImageCallRecorder PaintImage(const std::string& css, const std::string& path, bool preserveAspect) {
    auto image = std::make_shared<Widgets::Image>();
    image->SetImagePath(path);
    image->SetPreserveAspect(preserveAspect);

    const Css::StyleSheet sheet = Css::StyleSheet::LoadFromString(css);
    Css::LexborBridge bridge;
    bridge.Build(*image);
    Css::StyleApplier::Apply(bridge, sheet, *image);
    image->CalculateLayout(400.0f, 600.0f);

    ImageCallRecorder renderer;
    image->OnPaint(renderer);
    return renderer;
}

void TestContainCentersImageInsideBox() {
    WriteTwoByTwoBmp("image_widget_2x2.bmp");
    // 200x100 box, 2:2 image: it scales to 100x100 and sits in the middle horizontally.
    auto r = PaintImage("Image { width: 200px; height: 100px; }", "image_widget_2x2.bmp", true);
    assert(r.calls == 1);
    assert(r.lastRect.width == 100.0f && r.lastRect.height == 100.0f);
    assert(r.lastRect.x == 50.0f && r.lastRect.y == 0.0f);
    std::cout << "TestContainCentersImageInsideBox passed\n";
}

void TestStretchUsesWholeBox() {
    WriteTwoByTwoBmp("image_widget_2x2.bmp");
    auto r = PaintImage("Image { width: 200px; height: 100px; }", "image_widget_2x2.bmp", false);
    assert(r.calls == 1);
    assert(r.lastRect.width == 200.0f && r.lastRect.height == 100.0f);
    assert(r.lastRect.x == 0.0f && r.lastRect.y == 0.0f);
    std::cout << "TestStretchUsesWholeBox passed\n";
}

void TestMissingFileKeepsBoxAndStillCallsRenderer() {
    // The renderer owns the silent failure; the widget must not invent a size for it.
    auto r = PaintImage("Image { width: 200px; height: 100px; }", "no_such_image.png", true);
    assert(r.calls == 1);
    assert(r.lastRect.width == 200.0f && r.lastRect.height == 100.0f);
    std::cout << "TestMissingFileKeepsBoxAndStillCallsRenderer passed\n";
}

void TestEmptyPathDrawsNothing() {
    auto r = PaintImage("Image { width: 200px; height: 100px; }", "", true);
    assert(r.calls == 0);
    std::cout << "TestEmptyPathDrawsNothing passed\n";
}

} // namespace

int main() {
    TestContainCentersImageInsideBox();
    TestStretchUsesWholeBox();
    TestMissingFileKeepsBoxAndStillCallsRenderer();
    TestEmptyPathDrawsNothing();
    std::cout << "ImageWidgetTests: all passed\n";
    return 0;
}
