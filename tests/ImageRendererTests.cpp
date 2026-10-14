// GPU-dependent: needs a display and a working D3D11 or Vulkan driver. Draws real image files,
// including an unreadable one, and simulates device loss so the image texture must be
// re-uploaded. Pixel contents are not read back; this test checks that drawing neither crashes
// nor leaves the renderer lost.

#include <cstdio>
#include <memory>

#include "BmpFixtures.h"
#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Core/Frame.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Image.h"
#include "Widgets/Panel.h"

using namespace IzekipUI;

namespace {
constexpr int kFrames = 120;
constexpr int kLossFrame = 40;
}

int RunImageRendererTest() {
    TestFixtures::WriteFile("gpu_image_2x2.bmp", TestFixtures::TwoByTwoBmp());

    Platform::Window window({.title = "IzekipUI image test", .width = 640, .height = 360});

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
    auto photo = std::make_shared<Widgets::Image>();
    photo->SetId("photo");
    photo->SetImagePath("gpu_image_2x2.bmp");
    auto broken = std::make_shared<Widgets::Image>();
    broken->SetId("broken");
    broken->SetImagePath("missing_gpu_image.png");
    root->AddChild(photo);
    root->AddChild(broken);

    Css::StyleSheet sheet = Css::StyleSheet::LoadFromString(
        "#root { width: 640px; height: 360px; background-color: #14161A; } "
        "#photo { width: 200px; height: 100px; margin: 20px; } "
        "#broken { width: 200px; height: 100px; margin: 20px; }");
    Css::LexborBridge bridge;
    bridge.Build(*root);
    Css::StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(640.0f, 360.0f);

    for (int frame = 0; frame < kFrames && !window.ShouldClose(); ++frame) {
        window.PollEvents();
        if (frame == kLossFrame) {
            renderer->SimulateDeviceLossForTesting();
        }

        PaintFrame(*renderer, *root);

        if (renderer->IsDeviceLostForTesting()) {
            std::fprintf(stderr, "frame %d: device still lost after Present(); recovery failed\n", frame);
            renderer->Shutdown();
            return 1;
        }
    }

    renderer->Shutdown();
    std::printf("ImageRendererTest passed: image drawn across a simulated device loss\n");
    return 0;
}

int main() {
    try {
        return RunImageRendererTest();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ImageRendererTest: %s\n", error.what());
        return 1;
    }
}
