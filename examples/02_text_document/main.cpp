#include <chrono>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Button.h"
#include "Widgets/Label.h"
#include "Widgets/Panel.h"

// Text Document demo: an app-shaped stress test of the public API as it exists TODAY --
// no new engine features were added to build this. It is meant to answer "how far does
// what we already have go for something that isn't a property-by-property showcase?"
// Two things here haven't been exercised by example 01 before:
//   1. A Button with a child Label (caption text) instead of an empty styled rectangle.
//   2. backdrop-filter pulled over REAL painted text (via negative margin + paint order,
//      the only overlap mechanism available without absolute positioning/z-index).

using namespace IzekipUI;

namespace {

void MarkHoveredChain(Widget* widget, bool hovered) {
    for (Widget* w = widget; w != nullptr; w = w->GetParent()) {
        w->SetHovered(hovered);
    }
}

void MarkActiveChain(Widget* widget, bool active) {
    for (Widget* w = widget; w != nullptr; w = w->GetParent()) {
        w->SetActive(active);
    }
}

std::shared_ptr<Widgets::Button> MakeToolbarButton(const std::string& captionText,
                                                    const std::string& variantClass) {
    auto button = std::make_shared<Widgets::Button>();
    button->AddClass("toolbar-btn");
    button->AddClass(variantClass);

    auto caption = std::make_shared<Widgets::Label>();
    caption->AddClass("btn-label");
    caption->SetText(captionText);
    button->AddChild(caption);

    return button;
}

int CountWords(const std::string& text) {
    std::istringstream iss(text);
    std::string word;
    int count = 0;
    while (iss >> word) {
        ++count;
    }
    return count;
}

} // namespace

int main() {
    Platform::WindowDesc windowDesc;
    windowDesc.title = "IzekipUI - Text Document Demo";
    windowDesc.width = 1000;
    windowDesc.height = 780;
    Platform::Window window(windowDesc);

    auto renderer = Render::CreateRenderer(Render::GetCompiledInBackend());

    Render::RendererCreateInfo rendererInfo;
    rendererInfo.nativeWindowHandle = window.GetNativeHandle();
    rendererInfo.glfwWindow = window.GetGLFWHandle();
    rendererInfo.initialWidth = window.GetWidth();
    rendererInfo.initialHeight = window.GetHeight();
    renderer->Initialize(rendererInfo);

    window.SetResizeCallback([&](uint32_t width, uint32_t height) { renderer->Resize(width, height); });

    double mouseX = 0.0;
    double mouseY = 0.0;
    window.SetCursorPosCallback([&](double x, double y) {
        mouseX = x;
        mouseY = y;
    });

    bool leftButtonDown = false;
    window.SetMouseButtonCallback([&](Platform::MouseButton button, Platform::MouseAction action) {
        if (button == Platform::MouseButton::Left) {
            leftButtonDown = (action == Platform::MouseAction::Press);
        }
    });

    auto root = std::make_shared<Widgets::Panel>();
    root->SetId("root");

    // ---- Toolbar ----
    auto toolbar = std::make_shared<Widgets::Panel>();
    toolbar->AddClass("toolbar");

    auto toolbarTitle = std::make_shared<Widgets::Label>();
    toolbarTitle->AddClass("toolbar-title");
    toolbarTitle->SetText("IzekipUI Text Document");
    toolbar->AddChild(toolbarTitle);

    auto toolbarActions = std::make_shared<Widgets::Panel>();
    toolbarActions->AddClass("toolbar-actions");
    toolbarActions->AddChild(MakeToolbarButton("Save", "toolbar-btn-primary"));
    toolbarActions->AddChild(MakeToolbarButton("Print", "toolbar-btn-secondary"));
    toolbar->AddChild(toolbarActions);

    root->AddChild(toolbar);

    // ---- Page ----
    auto page = std::make_shared<Widgets::Panel>();
    page->AddClass("page");

    auto docTitle = std::make_shared<Widgets::Label>();
    docTitle->AddClass("doc-title");
    docTitle->SetText("Project Notes: IzekipUI Phase 7");
    page->AddChild(docTitle);

    auto docMeta = std::make_shared<Widgets::Label>();
    docMeta->AddClass("doc-meta");
    docMeta->SetText("Prepared by Izekip Teknoloji - Draft v0.3");
    page->AddChild(docMeta);

    auto dividerTop = std::make_shared<Widgets::Panel>();
    dividerTop->AddClass("divider");
    page->AddChild(dividerTop);

    const std::string paragraph1 =
        "This release adds real backdrop-filter rendering to the compositor. Previously\n"
        "the property parsed correctly but never painted anything, since the engine had\n"
        "no pass that reads back pixels already drawn to the frame. Blur, brightness,\n"
        "contrast, saturate and grayscale are now implemented end to end on both the\n"
        "D3D11 and Vulkan backends, sharing one HLSL shader and one CPU-side draw data\n"
        "function.";
    auto paragraph1Label = std::make_shared<Widgets::Label>();
    paragraph1Label->AddClass("doc-paragraph");
    paragraph1Label->SetText(paragraph1);
    page->AddChild(paragraph1Label);

    // Glass note box: pulled upward by a negative top margin so it visually overlaps the
    // last line of paragraph1 above it. Its backdrop-filter blur therefore samples real
    // paragraph text already painted this frame, not a flat background.
    auto callout = std::make_shared<Widgets::Panel>();
    callout->AddClass("callout");
    auto calloutText = std::make_shared<Widgets::Label>();
    calloutText->AddClass("callout-text");
    calloutText->SetText(
        "Note: this panel uses backdrop-filter to blur the real paragraph text\n"
        "painted behind it -- a live test of the feature outside the original showcase row.");
    callout->AddChild(calloutText);
    page->AddChild(callout);

    const std::string paragraph2 =
        "Finding a real rendering bug was a side effect of this work: on Vulkan, the\n"
        "window's reported size could diverge from the swapchain's actual surface\n"
        "extent whenever the OS clamped the requested window height. That mismatch has\n"
        "now been fixed at its source, inside the platform window wrapper itself,\n"
        "rather than only around the one code path that first exposed it.";
    auto paragraph2Label = std::make_shared<Widgets::Label>();
    paragraph2Label->AddClass("doc-paragraph");
    paragraph2Label->SetText(paragraph2);
    page->AddChild(paragraph2Label);

    auto dividerBottom = std::make_shared<Widgets::Panel>();
    dividerBottom->AddClass("divider-flat");
    page->AddChild(dividerBottom);

    // Real (not hardcoded) computation feeding back into the widget tree -- a small test
    // of the C++ <-> Label data flow outside of pure CSS-driven styling.
    const int wordCount = CountWords(paragraph1) + CountWords(paragraph2);
    auto footer = std::make_shared<Widgets::Label>();
    footer->AddClass("doc-footer");
    footer->SetText("~" + std::to_string(wordCount) + " words - counted in C++, not hardcoded");
    page->AddChild(footer);

    root->AddChild(page);

    Css::StyleSheet sheet = Css::StyleSheet::LoadFromFile("style.css");
    std::cout << "[IzekipUI] Parsed " << sheet.RuleCount() << " CSS rule(s) from style.css\n";

    Css::LexborBridge bridge;
    bridge.Build(*root);

    Css::StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));

    Widget* hoveredWidget = nullptr;
    bool activeState = false;
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (!window.ShouldClose()) {
        window.PollEvents();

        Widget* hitWidget = root->HitTest(static_cast<float>(mouseX), static_cast<float>(mouseY));
        bool needsReapply = false;

        if (hitWidget != hoveredWidget) {
            ClearHoveredRecursive(*root);
            if (hitWidget) {
                MarkHoveredChain(hitWidget, true);
            }
            hoveredWidget = hitWidget;
            needsReapply = true;
        }

        const bool wantActive = leftButtonDown && hoveredWidget != nullptr;
        if (wantActive != activeState) {
            ClearActiveRecursive(*root);
            if (wantActive) {
                MarkActiveChain(hoveredWidget, true);
            }
            activeState = wantActive;
            needsReapply = true;
        }

        if (needsReapply) {
            Css::StyleApplier::Apply(bridge, sheet, *root);
        }

        const auto now = std::chrono::steady_clock::now();
        const float deltaSeconds = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        const AnimationTickResult tickResult = TickWidgetTreeAnimations(*root, deltaSeconds);
        if (needsReapply || tickResult.layoutAffected) {
            root->CalculateLayout(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));
        }

        renderer->BeginFrame();
        root->OnPaint(*renderer);
        renderer->EndFrame();
        renderer->Present();
    }

    renderer->Shutdown();
    return 0;
}
