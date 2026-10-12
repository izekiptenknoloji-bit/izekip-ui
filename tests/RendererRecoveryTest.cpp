// GPU-dependent: needs a display and a working D3D11 or Vulkan driver. Simulates device loss
// at fixed frames and checks that the renderer rebuilds itself and keeps presenting frames.

#include <cstdio>
#include <memory>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Panel.h"

using namespace IzekipUI;

namespace {
constexpr int kFrames = 300;
constexpr int kLossFrames[] = {60, 180};
}

int RunRecoveryTest() {
    Platform::Window window({.title = "IzekipUI recovery test", .width = 640, .height = 360});

    auto renderer = Render::CreateRenderer(Render::GetCompiledInBackend());
    Render::RendererCreateInfo info{
        .nativeWindowHandle = window.GetNativeHandle(),
        .glfwWindow = window.GetGLFWHandle(),
        .initialWidth = 640,
        .initialHeight = 360,
    };
    if (!renderer->Initialize(info)) {
        std::fprintf(stderr, "renderer initialization failed\n");
        return 1;
    }

    auto root = std::make_shared<Widgets::Panel>();
    root->SetId("root");
    auto box = std::make_shared<Widgets::Panel>();
    box->AddClass("box");
    root->AddChild(box);

    Css::StyleSheet sheet = Css::StyleSheet::LoadFromString(
        "#root { width: 640px; height: 360px; background-color: #14161A; } "
        ".box { width: 200px; height: 100px; margin: 20px; background-color: #3366FF; }");
    Css::LexborBridge bridge;
    bridge.Build(*root);
    Css::StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(640.0f, 360.0f);

    int lossesSimulated = 0;
    for (int frame = 0; frame < kFrames && !window.ShouldClose(); ++frame) {
        window.PollEvents();
        for (int lossFrame : kLossFrames) {
            if (frame == lossFrame) {
                renderer->SimulateDeviceLossForTesting();
                ++lossesSimulated;
            }
        }

        renderer->BeginFrame();
        root->OnPaint(*renderer);
        renderer->EndFrame();
        renderer->Present();

        if (renderer->IsDeviceLostForTesting()) {
            std::fprintf(stderr, "frame %d: device still lost after Present(); recovery failed\n", frame);
            renderer->Shutdown();
            return 1;
        }
    }

    renderer->Shutdown();
    if (lossesSimulated != static_cast<int>(sizeof(kLossFrames) / sizeof(kLossFrames[0]))) {
        std::fprintf(stderr, "run ended before every simulated loss (%d)\n", lossesSimulated);
        return 1;
    }
    std::printf("RendererRecoveryTest passed: %d simulated losses recovered\n", lossesSimulated);
    return 0;
}

int main() {
    try {
        return RunRecoveryTest();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "RendererRecoveryTest: %s\n", error.what());
        return 1;
    }
}
