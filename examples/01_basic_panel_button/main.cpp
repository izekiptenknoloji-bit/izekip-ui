#include <cstdio>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Button.h"
#include "Widgets/Label.h"
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

// Monospace code text shown to the right of a demo widget. The actual glyph atlas
// selection is a C++ call (SetFontFamily), not CSS -- see style.css's .code-label comment.
std::shared_ptr<Widgets::Label> MakeCodeLabel(const std::string& code) {
    auto label = std::make_shared<Widgets::Label>();
    label->AddClass("code-label");
    label->SetFontFamily(Render::FontId::Monospace);
    label->SetText(code);
    return label;
}

// One showcase row: a Panel (class .showcase-row, flex-direction: row) holding one or
// more live demo widgets followed by the literal CSS text that produced them.
std::shared_ptr<Widgets::Panel> MakeShowcaseRow(const std::string& id,
                                                 std::vector<std::shared_ptr<Widget>> demoWidgets,
                                                 const std::string& code) {
    auto row = std::make_shared<Widgets::Panel>();
    row->SetId(id);
    row->AddClass("showcase-row");
    for (auto& widget : demoWidgets) {
        row->AddChild(widget);
    }
    row->AddChild(MakeCodeLabel(code));
    return row;
}

} // namespace

int main() {
    Platform::WindowDesc windowDesc;
    windowDesc.title = "IzekipUI NativeCSS Showcase";
    windowDesc.width = 1250;
    windowDesc.height = 1660;
    Platform::Window window(windowDesc);

    auto renderer = Render::CreateRenderer(Render::GetCompiledInBackend());

    Render::RendererCreateInfo rendererInfo;
    rendererInfo.nativeWindowHandle = window.GetNativeHandle();
    rendererInfo.glfwWindow = window.GetGLFWHandle();
    rendererInfo.initialWidth = window.GetWidth();
    rendererInfo.initialHeight = window.GetHeight();
    if (!renderer->Initialize(rendererInfo)) {
        std::fprintf(stderr, "renderer initialization failed\n");
        return 1;
    }

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
    root->SetId("showcaseRoot");
    root->AddClass("showcase-root");

    // Block 1: background-color + fixed width/height, via the bare `Panel {}` type
    // selector -- no id/class on this demo widget, so it matches nothing else.
    auto typeSelectorDemo = std::make_shared<Widgets::Panel>();
    root->AddChild(MakeShowcaseRow("row1", {typeSelectorDemo}, R"(Panel {
    background-color: #CCCCCC;
    width: 400px;
    height: 300px;
})"));

    // Block 2: margin shorthand, single value (#mainPanel) and two-value (.primary).
    auto marginSingleDemo = std::make_shared<Widgets::Panel>();
    marginSingleDemo->SetId("mainPanel");
    marginSingleDemo->AddClass("compact-demo");
    auto marginDoubleDemo = std::make_shared<Widgets::Button>();
    marginDoubleDemo->AddClass("primary");
    root->AddChild(MakeShowcaseRow("row2", {marginSingleDemo, marginDoubleDemo}, R"(#mainPanel {
    margin: 50px;
}

.primary {
    margin: 10px 30px;
})"));

    const char* kButtonRuleText = R"(Button {
    background: linear-gradient(to right, #3366FF, #5588FF);
    border-radius: 8px;
    box-shadow: 0px 4px 12px #00000055;
    width: 120px;
    height: 40px;
    transition: all 300ms ease-in-out;
})";

    // Blocks 3-5: border-radius, box-shadow, and the linear-gradient background all come
    // from the same `Button {}` rule -- each row gets its own live button instance so
    // hover/active still respond per-row, but the demoed CSS text is identical.
    auto radiusDemo = std::make_shared<Widgets::Button>();
    radiusDemo->AddClass("primary");
    root->AddChild(MakeShowcaseRow("row3", {radiusDemo}, kButtonRuleText));

    auto shadowDemo = std::make_shared<Widgets::Button>();
    shadowDemo->AddClass("primary");
    root->AddChild(MakeShowcaseRow("row4", {shadowDemo}, kButtonRuleText));

    auto gradientDemo = std::make_shared<Widgets::Button>();
    gradientDemo->AddClass("primary");
    root->AddChild(MakeShowcaseRow("row5", {gradientDemo}, kButtonRuleText));

    // Block 6: :hover + transition -- a real, independently-hoverable button.
    auto hoverDemo = std::make_shared<Widgets::Button>();
    hoverDemo->AddClass("primary");
    root->AddChild(MakeShowcaseRow("row6", {hoverDemo}, R"(.primary:hover {
    width: 135px;
    height: 44px;
    box-shadow: 0px 6px 18px #00000070;
}
/* fareyi butonun üzerine getirin */)"));

    // Block 7: :active -- a real, independently-pressable button.
    auto activeDemo = std::make_shared<Widgets::Button>();
    activeDemo->AddClass("primary");
    root->AddChild(MakeShowcaseRow("row7", {activeDemo}, R"(.primary:active {
    box-shadow: 0px 2px 6px #00000055;
}
/* butona basılı tutun */)"));

    // Block 8: Label color/font-size. The descendant-combinator rule (Panel Button) has
    // no visual effect yet (no border drawing), so it's shown as a comment-only note.
    auto labelDemo = std::make_shared<Widgets::Label>();
    labelDemo->SetText("Hello, IzekipUI!");
    root->AddChild(MakeShowcaseRow("row8", {labelDemo}, R"(Label {
    color: #222222;
    font-size: 20px;
}

/* Panel Button { border-color: #FF0000; } */
/* descendant selector eşleşiyor, henüz border çizimi yok */)"));

    // Deneme: "glassmorphism" -- rgba() arka plan, border, box-shadow hepsi render
    // ediliyor; backdrop-filter gerçek CSS'tir (lexbor sorunsuz parse ediyor) ama
    // IzekipUI'de framebuffer-örnekleyen bir blur geçişi yok, bu yüzden sessizce
    // yutuluyor (crash yok, sadece etkisiz). border rengi de henüz çizilmiyor (yukarıdaki
    // Panel Button notuyla aynı sınırlama).
    auto glassDemo = std::make_shared<Widgets::Panel>();
    glassDemo->AddClass("glass-test");
    root->AddChild(MakeShowcaseRow("row9", {glassDemo}, R"(.glass-test {
    background-color: rgba(255, 255, 255, 0.15);
    border: 1px solid #FFFFFF66;
    border-radius: 16px;
    box-shadow: 0px 8px 32px #00000080;
    backdrop-filter: blur(10px);
}
/* backdrop-filter parse ediliyor ama render edilmiyor (framebuffer blur yok) */)"));

    // Block 10: opacity property -- animates smoothly via CSS transitions.
    auto opacityDemo = std::make_shared<Widgets::Panel>();
    opacityDemo->AddClass("opacity-demo");
    root->AddChild(MakeShowcaseRow("row10", {opacityDemo}, R"(.opacity-demo {
    background-color: #FF6B6B;
    opacity: 1.0;
    transition: opacity 300ms ease-in-out;
}

.opacity-demo:hover {
    opacity: 0.5;
}
/* Hover Üzerine Getin */)"));

    // Block 11: Low opacity initial value.
    auto opacityLowDemo = std::make_shared<Widgets::Panel>();
    opacityLowDemo->AddClass("opacity-demo-low");
    root->AddChild(MakeShowcaseRow("row11", {opacityLowDemo}, R"(.opacity-demo-low {
    background-color: #4ECDC4;
    opacity: 0.3;
})"));

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

        // Hit-test against the latest known mouse position and update :hover/:active.
        // Hovering a child also counts as hovering its ancestors, matching real CSS.
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
