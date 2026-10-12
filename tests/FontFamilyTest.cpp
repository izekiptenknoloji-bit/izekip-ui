// GPU-dependent: shows one label per CSS font family for a few seconds so the faces can be
// checked by eye (Inter, Arimo/Arial, Tinos/Times, SpaceMono, Cousine/Courier). Exits 0 if the
// renderer draws every frame without throwing.

#include <cstdio>
#include <memory>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Core/Frame.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Label.h"
#include "Widgets/Panel.h"

using namespace IzekipUI;

namespace {
constexpr int kFrames = 600;
}

int RunFontFamilyTest() {
    Platform::Window window({.title = "IzekipUI font families", .width = 720, .height = 420});

    auto renderer = Render::CreateRenderer(Render::GetCompiledInBackend());
    Render::RendererCreateInfo info{
        .nativeWindowHandle = window.GetNativeHandle(),
        .glfwWindow = window.GetGLFWHandle(),
        .initialWidth = 720,
        .initialHeight = 420,
    };
    if (!renderer->Initialize(info)) {
        std::fprintf(stderr, "renderer initialization failed\n");
        return 1;
    }

    auto root = std::make_shared<Widgets::Panel>();
    root->SetId("root");
    const char* samples[][2] = {
        {"sans", "Sans-serif (Inter): Merhaba Türkçe ğüşıöç 0123"},
        {"arial", "Arial (Arimo): Merhaba Türkçe ğüşıöç 0123"},
        {"times", "Times (Tinos): Merhaba Türkçe ğüşıöç 0123"},
        {"mono", "Monospace (SpaceMono): Merhaba Türkçe ğüşıöç 0123"},
        {"courier", "Courier (Cousine): Merhaba Türkçe ğüşıöç 0123"},
    };
    for (const auto& sample : samples) {
        auto label = std::make_shared<Widgets::Label>();
        label->SetText(sample[1]);
        label->AddClass(sample[0]);
        label->AddClass("line");
        root->AddChild(label);
    }

    Css::StyleSheet sheet = Css::StyleSheet::LoadFromString(
        "#root { width: 720px; height: 420px; background-color: #14161A; padding: 24px; } "
        ".line { color: #E6E9EF; font-size: 22px; margin: 0px 0px 14px 0px; } "
        ".sans { font-family: sans-serif; } "
        ".arial { font-family: Arial; } "
        ".times { font-family: Times New Roman; } "
        ".mono { font-family: monospace; } "
        ".courier { font-family: Courier New; }");
    Css::LexborBridge bridge;
    bridge.Build(*root);
    Css::StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(720.0f, 420.0f);

    for (int frame = 0; frame < kFrames && !window.ShouldClose(); ++frame) {
        window.PollEvents();
        PaintFrame(*renderer, *root);
    }

    renderer->Shutdown();
    std::printf("FontFamilyTest drew %d frames\n", kFrames);
    return 0;
}

int main() {
    try {
        return RunFontFamilyTest();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FontFamilyTest: %s\n", error.what());
        return 1;
    }
}
