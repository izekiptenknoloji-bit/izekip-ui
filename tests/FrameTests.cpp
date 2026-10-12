#undef NDEBUG // tests must check their assertions in every configuration, including Release
#include <cassert>
#include <iostream>
#include <memory>
#include <new>

#include "Core/Frame.h"
#include "Core/Widget.h"
#include "Platform/Diagnostics.h"
#include "Renderer/IRenderer.h"

using namespace IzekipUI;

namespace {

// A renderer that draws nothing; it only records the frame calls so the frame helper can be tested
// without a GPU.
class RecordingRenderer : public Render::IRenderer {
public:
    bool Initialize(const Render::RendererCreateInfo&) override { return true; }
    void Shutdown() override {}
    void Resize(uint32_t, uint32_t) override {}
    void BeginFrame() override { ++begins; }
    void EndFrame() override { ++ends; }
    void Present() override { ++presents; }
    void DrawRect(const Rect&, const Render::RectPaint&) override {}
    void DrawTextQuad(const Rect&, const Render::UVRect&, const Color&, float, Render::FontId) override {}
    void DrawBackdropBlurRect(const Rect&, const Render::BackdropBlurPaint&) override { ++blurs; }
    void DrawImage(const std::string&, const Rect&, float) override {}
    void SetWorldTransform(const Transform2D&) override {}
    Render::RendererBackend GetBackend() const override { return Render::RendererBackend::D3D11; }

    int begins = 0;
    int ends = 0;
    int presents = 0;
    int blurs = 0;
};

class FailingWidget : public Widget {
public:
    FailingWidget() : Widget("Panel") {}
    bool fail = false;

protected:
    void OnPaint(Render::IRenderer& renderer) override {
        if (fail) {
            throw std::bad_alloc();
        }
        Widget::OnPaint(renderer);
    }
};

void TestHealthyFrameIsPresented() {
    RecordingRenderer renderer;
    FailingWidget root;
    assert(PaintFrame(renderer, root) == FrameResult::Presented);
    assert(renderer.begins == 1 && renderer.ends == 1 && renderer.presents == 1);
    std::cout << "TestHealthyFrameIsPresented passed\n";
}

void TestOutOfMemoryFrameIsClosedAndDegraded() {
    using Platform::Diagnostics::Counter;
    using Platform::Diagnostics::Get;
    const std::uint64_t oomBefore = Get(Counter::OutOfMemory);

    RecordingRenderer renderer;
    FailingWidget root;
    root.fail = true;
    assert(PaintFrame(renderer, root) == FrameResult::OutOfMemory);
    // The partial frame is still closed out and presented, so renderer state stays consistent.
    assert(renderer.ends == 1 && renderer.presents == 1);
    assert(Platform::Diagnostics::IsDegraded());
    assert(Get(Counter::OutOfMemory) == oomBefore + 1);
    std::cout << "TestOutOfMemoryFrameIsClosedAndDegraded passed\n";
}

void TestDegradedModeEndsAfterHealthyFrames() {
    RecordingRenderer renderer;
    FailingWidget root;
    root.fail = true;
    PaintFrame(renderer, root);
    assert(Platform::Diagnostics::IsDegraded());

    root.fail = false;
    for (int frame = 0; frame < 600; ++frame) {
        PaintFrame(renderer, root);
    }
    assert(!Platform::Diagnostics::IsDegraded());
    std::cout << "TestDegradedModeEndsAfterHealthyFrames passed\n";
}

} // namespace

int main() {
    TestHealthyFrameIsPresented();
    TestOutOfMemoryFrameIsClosedAndDegraded();
    TestDegradedModeEndsAfterHealthyFrames();
    std::cout << "All FrameTests passed.\n";
    return 0;
}
