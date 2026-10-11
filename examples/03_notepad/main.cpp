#include <chrono>
#include <iostream>
#include <memory>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Label.h"
#include "Widgets/Panel.h"
#include "Widgets/TextArea.h"

// Notepad demo: real keyboard input + a multi-line editable TextArea, plus Turkish
// character support -- the first feature in this engine that needs a focus concept and
// GLFW key/char plumbing (see Platform/Window.h's new Key/Char callbacks). TextArea itself
// stays Platform-unaware (see its header); this file is the only place Platform::Key gets
// translated into TextBuffer calls.

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
    windowDesc.title = "IzekipUI Notepad";
    windowDesc.width = 900;
    windowDesc.height = 700;
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

    auto toolbar = std::make_shared<Widgets::Panel>();
    toolbar->AddClass("toolbar");

    auto toolbarTitle = std::make_shared<Widgets::Label>();
    toolbarTitle->AddClass("toolbar-title");
    toolbarTitle->SetText("IzekipUI Notepad");
    toolbar->AddChild(toolbarTitle);

    auto toolbarHint = std::make_shared<Widgets::Label>();
    toolbarHint->AddClass("toolbar-hint");
    toolbarHint->SetText("Click the box below and type -- Turkish characters included");
    toolbar->AddChild(toolbarHint);

    root->AddChild(toolbar);

    auto textArea = std::make_shared<Widgets::TextArea>();
    textArea->AddClass("notepad-area");
    textArea->GetBuffer().SetTextUtf8(
        "Günaydın!\n"
        "Türkçe karakterler: ş ğ ı ö ü ç İ Ş Ğ Ö Ü Ç\n"
        "\n"
        "Buraya tıklayıp yazabilirsin.\n");
    root->AddChild(textArea);

    window.SetCharCallback([&](char32_t codepoint) {
        if (textArea->IsFocused()) {
            textArea->OnCharInput(codepoint);
        }
    });

    window.SetKeyCallback([&](Platform::Key key, Platform::KeyAction action) {
        if (!textArea->IsFocused() || action == Platform::KeyAction::Release) {
            return;
        }
        TextBuffer& buffer = textArea->GetBuffer();
        switch (key) {
            case Platform::Key::Backspace: buffer.Backspace(); break;
            case Platform::Key::Delete: buffer.DeleteForward(); break;
            case Platform::Key::Enter: buffer.InsertNewline(); break;
            case Platform::Key::Left: buffer.MoveLeft(); break;
            case Platform::Key::Right: buffer.MoveRight(); break;
            case Platform::Key::Up: buffer.MoveUp(); break;
            case Platform::Key::Down: buffer.MoveDown(); break;
            case Platform::Key::Home: buffer.MoveHome(); break;
            case Platform::Key::End: buffer.MoveEnd(); break;
            default: break; // Tab/Escape/Unknown: received, intentionally no-op (named scope cut)
        }
    });

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

                // Rising edge (mouse press): decide focus. Click-to-focus only -- cursor
                // jumps to end-of-document rather than the clicked character (named scope
                // cut, see TextArea.h).
                if (hoveredWidget == textArea.get()) {
                    if (!textArea->IsFocused()) {
                        ClearFocusedRecursive(*root);
                        textArea->FocusAndMoveCursorToEnd();
                    }
                } else if (textArea->IsFocused()) {
                    ClearFocusedRecursive(*root); // clicked away: blur
                }
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

        textArea->Tick(deltaSeconds);

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
