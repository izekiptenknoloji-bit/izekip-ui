#include <chrono>
#include <iostream>
#include <memory>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Button.h"
#include "Widgets/Panel.h"

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

} // namespace

int main() {
    Platform::WindowDesc windowDesc;
    windowDesc.title = "Izekip Example";
    windowDesc.width = 1280;
    windowDesc.height = 720;
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

    auto panel = std::make_shared<Widgets::Panel>();
    panel->SetId("mainPanel");

    auto button = std::make_shared<Widgets::Button>();
    button->AddClass("primary");
    panel->AddChild(button);

    Css::StyleSheet sheet = Css::StyleSheet::LoadFromFile("style.css");
    std::cout << "[IzekipUI] Parsed " << sheet.RuleCount() << " CSS rule(s) from style.css\n";

    Css::LexborBridge bridge;
    bridge.Build(*panel);

    Css::StyleApplier::Apply(bridge, sheet, *panel);
    panel->CalculateLayout(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));

    Widget* hoveredWidget = nullptr;
    bool activeState = false;
    auto lastFrameTime = std::chrono::steady_clock::now();

    while (!window.ShouldClose()) {
        window.PollEvents();

        // Hit-test against the latest known mouse position and update :hover/:active.
        // Hovering a child also counts as hovering its ancestors, matching real CSS.
        Widget* hitWidget = panel->HitTest(static_cast<float>(mouseX), static_cast<float>(mouseY));
        bool needsReapply = false;

        if (hitWidget != hoveredWidget) {
            ClearHoveredRecursive(*panel);
            if (hitWidget) {
                MarkHoveredChain(hitWidget, true);
            }
            hoveredWidget = hitWidget;
            needsReapply = true;
        }

        const bool wantActive = leftButtonDown && hoveredWidget != nullptr;
        if (wantActive != activeState) {
            ClearActiveRecursive(*panel);
            if (wantActive) {
                MarkActiveChain(hoveredWidget, true);
            }
            activeState = wantActive;
            needsReapply = true;
        }

        if (needsReapply) {
            Css::StyleApplier::Apply(bridge, sheet, *panel);
        }

        const auto now = std::chrono::steady_clock::now();
        const float deltaSeconds = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        const AnimationTickResult tickResult = TickWidgetTreeAnimations(*panel, deltaSeconds);
        if (needsReapply || tickResult.layoutAffected) {
            panel->CalculateLayout(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));
        }

        renderer->BeginFrame();
        panel->OnPaint(*renderer);
        renderer->EndFrame();
        renderer->Present();
    }

    renderer->Shutdown();
    return 0;
}
