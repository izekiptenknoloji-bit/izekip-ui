#include <cstdio>
#include <chrono>
#include <memory>
#include <string>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Button.h"
#include "Widgets/Label.h"
#include "Widgets/Panel.h"

// Glassmorphism demo built entirely on the existing public API: a gradient background, a
// row of solid gradient "blobs", and glass cards (translucent rgba + backdrop-filter blur)
// pulled up over the blobs via a negative top margin -- see assets/style.css for why that's
// the only way to overlap in this engine.

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

std::shared_ptr<Widgets::Label> MakeLabel(const std::string& text, const std::string& cssClass) {
    auto label = std::make_shared<Widgets::Label>();
    label->AddClass(cssClass);
    label->SetText(text);
    return label;
}

std::shared_ptr<Widgets::Panel> MakeCard(const std::string& title, const std::string& body,
                                         const std::string& buttonText) {
    auto card = std::make_shared<Widgets::Panel>();
    card->AddClass("card");
    card->AddChild(MakeLabel(title, "card-title"));
    card->AddChild(MakeLabel(body, "card-body"));

    auto button = std::make_shared<Widgets::Button>();
    button->AddClass("glass-btn");
    button->AddChild(MakeLabel(buttonText, "btn-label"));
    card->AddChild(button);
    return card;
}

std::shared_ptr<Widgets::Panel> MakeBlob(const std::string& colorClass) {
    auto blob = std::make_shared<Widgets::Panel>();
    blob->AddClass("blob");
    blob->AddClass(colorClass);
    return blob;
}

} // namespace

int main() {
    Platform::WindowDesc windowDesc;
    windowDesc.title = "IzekipUI - Glassmorphism";
    windowDesc.width = 1000;
    windowDesc.height = 720;
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
    root->SetId("root");

    // Navbar
    auto navbar = std::make_shared<Widgets::Panel>();
    navbar->AddClass("navbar");
    navbar->AddChild(MakeLabel("IzekipUI Glass", "brand"));
    auto navLinks = std::make_shared<Widgets::Panel>();
    navLinks->AddClass("nav-links");
    navLinks->AddChild(MakeLabel("Ana Sayfa", "nav-link"));
    navLinks->AddChild(MakeLabel("Bileşenler", "nav-link"));
    navLinks->AddChild(MakeLabel("Dökümanlar", "nav-link"));
    navbar->AddChild(navLinks);
    root->AddChild(navbar);

    // Blobs -- added BEFORE the cards so they paint first and the cards' backdrop-filter
    // samples them.
    auto blobRow = std::make_shared<Widgets::Panel>();
    blobRow->AddClass("blob-row");
    blobRow->AddChild(MakeBlob("blob-pink"));
    blobRow->AddChild(MakeBlob("blob-orange"));
    blobRow->AddChild(MakeBlob("blob-green"));
    root->AddChild(blobRow);

    // Glass cards
    auto cardRow = std::make_shared<Widgets::Panel>();
    cardRow->AddClass("card-row");
    cardRow->AddChild(MakeCard("Blur", "Real backdrop-filter:\nGPU framebuffer copy +\nGaussian convolution.", "Explore"));
    cardRow->AddChild(MakeCard("CSS", "Styled with real CSS,\nparsed by lexbor,\nlaid out by Yoga.", "Learn more"));
    cardRow->AddChild(MakeCard("Native", "D3D11 and Vulkan,\none HLSL shader,\nzero web runtime.", "Get started"));
    root->AddChild(cardRow);

    root->AddChild(MakeLabel("Kartların ve butonların üzerine gel -- buradaki her efekt düz CSS.\n"
                             "Türkçe karakter testi: ş ğ ı ö ü ç İ Ş Ğ Ö Ü Ç",
                             "footer"));

    Css::StyleSheet sheet = Css::StyleSheet::LoadFromFile("style.css");
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
