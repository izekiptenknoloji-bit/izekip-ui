#include "InstallerApp.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Core/InteractionController.h"
#include "Core/Version.h"
#include "InstallEngine.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "WinSys.h"
#include "Widgets/Button.h"
#include "Widgets/Checkbox.h"
#include "Widgets/Label.h"
#include "Widgets/Panel.h"
#include "Widgets/ProgressBar.h"
#include "Widgets/RadioButton.h"
#include "Widgets/TextArea.h"

namespace fs = std::filesystem;

namespace IzekipSetup {

fs::path DefaultPrefix(Scope scope) {
    const fs::path base = WinSys::KnownFolder(scope == Scope::AllUsers ? WinSys::Folder::ProgramFiles
                                                                       : WinSys::Folder::LocalAppData);
    return base / "IzekipUI";
}

namespace {

using namespace IzekipUI;

constexpr float kWidth = 760.0f;
constexpr float kHeight = 520.0f;

enum class Page { Welcome, Location, Options, Progress, Done, UninstallConfirm };

struct UiButton {
    std::shared_ptr<Widgets::Button> widget;
    std::shared_ptr<Widgets::Label> label;
};

std::shared_ptr<Widgets::Label> MakeLabel(const std::string& text, const std::string& cssClass) {
    auto label = std::make_shared<Widgets::Label>();
    label->AddClass(cssClass);
    label->SetText(text);
    return label;
}

UiButton MakeButton(const std::string& text, const std::string& variant) {
    UiButton b;
    b.widget = std::make_shared<Widgets::Button>();
    b.widget->AddClass("btn");
    b.widget->AddClass(variant);
    b.label = MakeLabel(text, "btn-label");
    b.widget->AddChild(b.label);
    return b;
}

std::shared_ptr<Widgets::Panel> MakePanel(const std::string& cssClass) {
    auto panel = std::make_shared<Widgets::Panel>();
    panel->AddClass(cssClass);
    return panel;
}

std::string Trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n')) s.pop_back();
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
    return s.substr(i);
}

class App {
public:
    explicit App(const Args& args) : m_args(args) {
        m_uninstallMode = args.mode == Mode::Uninstall;
        m_payloadRoot = args.payload.value_or(WinSys::ExeDir() / "payload");
        std::error_code ec;
        m_hasVulkan = fs::is_directory(m_payloadRoot / "vulkan", ec);
        m_prefix = args.prefix.value_or(DefaultPrefix(args.scope.value_or(Scope::CurrentUser)));
    }

    int Run();

private:
    void BuildTree();
    void ShowPage(Page page);
    void OnNext();
    void OnBack();
    void StartInstall();
    void StartUninstall();
    void OnEngineFinished();
    void ShowError(Widgets::Label& label, const std::string& message);
    void HideError(Widgets::Label& label);
    void SetPathText(const fs::path& path);
    InstallOptions GatherOptions() const;
    void MarkDirty() { m_dirty = true; }

    Args m_args;
    bool m_uninstallMode = false;
    fs::path m_payloadRoot;
    fs::path m_prefix;
    bool m_hasVulkan = false;
    bool m_pathEdited = false;
    bool m_dirty = true;
    bool m_quit = false;
    bool m_engineActive = false;
    int m_exitCode = 1;
    Page m_page = Page::Welcome;

    std::unique_ptr<Platform::Window> m_window;
    InstallEngine m_engine;
    std::unique_ptr<InteractionController> m_controller;

    std::shared_ptr<Widgets::Panel> m_root;
    std::shared_ptr<Widgets::Label> m_stepLabel;
    std::shared_ptr<Widgets::Panel> m_pageWelcome, m_pageLocation, m_pageOptions, m_pageProgress, m_pageDone,
        m_pageUninstall;

    Widgets::RadioGroup m_scopeGroup;
    Widgets::RadioGroup m_backendGroup;
    std::shared_ptr<Widgets::TextArea> m_pathField;
    std::shared_ptr<Widgets::Label> m_locationError;
    std::shared_ptr<Widgets::Checkbox> m_optPath, m_optEnv, m_optUninstall, m_optShortcut;
    std::shared_ptr<Widgets::Label> m_optionsError;

    std::shared_ptr<Widgets::Label> m_progressTitle;
    std::shared_ptr<Widgets::ProgressBar> m_progressBar;
    std::shared_ptr<Widgets::Label> m_statusLabel;

    std::shared_ptr<Widgets::Label> m_doneTitle;
    std::shared_ptr<Widgets::Label> m_doneDetail;
    UiButton m_openFolder, m_rollback;

    UiButton m_back, m_next, m_cancel;
};

void App::SetPathText(const fs::path& path) {
    m_pathField->GetBuffer().SetTextUtf8(PathToUtf8(path));
    m_pathField->GetBuffer().MoveDocumentEnd();
}

void App::ShowError(Widgets::Label& label, const std::string& message) {
    label.SetText(message);
    label.SetClass("hidden", false);
    MarkDirty();
}

void App::HideError(Widgets::Label& label) {
    label.SetClass("hidden", true);
    MarkDirty();
}

void App::BuildTree() {
    m_root = std::make_shared<Widgets::Panel>();
    m_root->SetId("root");

    // ---- Header ----
    auto header = MakePanel("header");
    header->AddChild(MakeLabel("İzekip UI SDK Kurulumu", "title"));
    m_stepLabel = MakeLabel("", "step");
    header->AddChild(m_stepLabel);
    m_root->AddChild(header);

    auto pages = MakePanel("pages");
    m_root->AddChild(pages);

    // ---- Welcome ----
    m_pageWelcome = MakePanel("page");
    m_pageWelcome->AddChild(MakeLabel("Hoş geldiniz", "page-title"));
    m_pageWelcome->AddChild(MakeLabel(
        "Bu sihirbaz İzekip UI SDK'yı bilgisayarınıza kuracak: header'lar, kütüphaneler,\n"
        "CMake paketi (find_package(IzekipUI)) ve cssfr proje oluşturma aracı.\n\n"
        "Bu kurulum ekranı da İzekip UI ile yazıldı.",
        "body"));
    m_pageWelcome->AddChild(MakeLabel(std::string("Sürüm ") + IZEKIP_VERSION_STRING, "muted"));
    pages->AddChild(m_pageWelcome);

    // ---- Location ----
    m_pageLocation = MakePanel("page");
    m_pageLocation->AddChild(MakeLabel("Kurulum yeri", "page-title"));
    m_pageLocation->AddChild(MakeLabel("Kimin için kurulsun?", "section"));
    auto scopeUser = std::make_shared<Widgets::RadioButton>("Sadece benim için (yönetici izni gerekmez)");
    auto scopeAll = std::make_shared<Widgets::RadioButton>("Tüm kullanıcılar (yönetici izni gerekir)");
    m_pageLocation->AddChild(scopeUser);
    m_pageLocation->AddChild(scopeAll);
    m_scopeGroup.Add(scopeUser);
    m_scopeGroup.Add(scopeAll);
    if (m_args.scope == Scope::AllUsers) {
        m_scopeGroup.Select(1);
    }
    m_scopeGroup.SetOnChanged([this](int index) {
        if (!m_pathEdited) {
            SetPathText(DefaultPrefix(index == 1 ? Scope::AllUsers : Scope::CurrentUser));
        }
        HideError(*m_locationError);
    });

    m_pageLocation->AddChild(MakeLabel("Kurulum klasörü", "section"));
    auto pathRow = MakePanel("path-row");
    auto pathBox = MakePanel("path-box");
    m_pathField = std::make_shared<Widgets::TextArea>();
    m_pathField->AddClass("path-field");
    pathBox->AddChild(m_pathField);
    pathRow->AddChild(pathBox);
    UiButton browse = MakeButton("Gözat...", "btn-secondary");
    browse.widget->SetOnClick([this] {
        const fs::path current = PathFromUtf8(Trim(m_pathField->GetBuffer().GetTextUtf8()));
        if (auto chosen = WinSys::PickFolder(m_window->GetNativeHandle(), current)) {
            fs::path target = *chosen;
            if (target.filename() != "IzekipUI") {
                target /= "IzekipUI";
            }
            SetPathText(target);
            m_pathEdited = true;
            HideError(*m_locationError);
        }
    });
    pathRow->AddChild(browse.widget);
    m_pageLocation->AddChild(pathRow);

    auto backendSection = MakePanel("backend-section");
    backendSection->AddChild(MakeLabel("Render backend'i", "section"));
    auto backendD3D = std::make_shared<Widgets::RadioButton>("Direct3D 11 (önerilen)");
    auto backendVulkan = std::make_shared<Widgets::RadioButton>("Vulkan (Vulkan SDK gerekir)");
    backendSection->AddChild(backendD3D);
    backendSection->AddChild(backendVulkan);
    m_backendGroup.Add(backendD3D);
    m_backendGroup.Add(backendVulkan);
    if (m_args.backend == Backend::Vulkan) {
        m_backendGroup.Select(1);
    }
    if (!m_hasVulkan) {
        backendSection->AddClass("hidden"); // only one payload shipped -- nothing to choose
    }
    m_pageLocation->AddChild(backendSection);

    m_locationError = MakeLabel("", "error");
    m_locationError->AddClass("hidden");
    m_pageLocation->AddChild(m_locationError);
    pages->AddChild(m_pageLocation);

    // ---- Options ----
    m_pageOptions = MakePanel("page");
    m_pageOptions->AddChild(MakeLabel("Seçenekler", "page-title"));
    m_optPath = std::make_shared<Widgets::Checkbox>("bin klasörünü PATH'e ekle (terminalden cssfr çalışsın)");
    m_optEnv = std::make_shared<Widgets::Checkbox>("IzekipUI_DIR ortam değişkenini ayarla (find_package için)");
    m_optUninstall = std::make_shared<Widgets::Checkbox>("Programlar ve Özellikler'e kaldırıcı ekle");
    m_optShortcut = std::make_shared<Widgets::Checkbox>("Başlat menüsü kısayolu oluştur");
    m_optPath->SetChecked(m_args.addToPath.value_or(true));
    m_optEnv->SetChecked(m_args.setCmakeDirEnv.value_or(true));
    m_optUninstall->SetChecked(m_args.registerUninstall.value_or(true));
    m_optShortcut->SetChecked(m_args.startMenuShortcut.value_or(true));
    for (const auto& box : {m_optPath, m_optEnv, m_optUninstall, m_optShortcut}) {
        m_pageOptions->AddChild(box);
    }
    m_optionsError = MakeLabel("", "error");
    m_optionsError->AddClass("hidden");
    m_pageOptions->AddChild(m_optionsError);
    pages->AddChild(m_pageOptions);

    // ---- Progress ----
    m_pageProgress = MakePanel("page");
    m_progressTitle = MakeLabel("Kuruluyor", "page-title");
    m_pageProgress->AddChild(m_progressTitle);
    m_progressBar = std::make_shared<Widgets::ProgressBar>();
    m_pageProgress->AddChild(m_progressBar);
    m_statusLabel = MakeLabel("Hazırlanıyor...", "status");
    m_pageProgress->AddChild(m_statusLabel);
    pages->AddChild(m_pageProgress);

    // ---- Done ----
    m_pageDone = MakePanel("page");
    m_doneTitle = MakeLabel("Tamamlandı", "page-title");
    m_doneDetail = MakeLabel("", "body");
    m_pageDone->AddChild(m_doneTitle);
    m_pageDone->AddChild(m_doneDetail);
    auto doneButtons = MakePanel("done-buttons");
    m_openFolder = MakeButton("Klasörü aç", "btn-secondary");
    m_openFolder.widget->SetOnClick([this] { WinSys::OpenInExplorer(GatherOptions().prefix); });
    m_rollback = MakeButton("Kopyalananları geri al", "btn-secondary");
    m_rollback.widget->SetOnClick([this] {
        m_engine.RollbackCopiedFiles();
        m_doneDetail->SetText("Bu kurulumda kopyalanan dosyalar silindi.");
        m_rollback.widget->AddClass("hidden");
        MarkDirty();
    });
    doneButtons->AddChild(m_openFolder.widget);
    doneButtons->AddChild(m_rollback.widget);
    m_pageDone->AddChild(doneButtons);
    pages->AddChild(m_pageDone);

    // ---- Uninstall confirmation ----
    m_pageUninstall = MakePanel("page");
    m_pageUninstall->AddChild(MakeLabel("İzekip UI SDK kaldırılacak", "page-title"));
    m_pageUninstall->AddChild(MakeLabel(
        "Konum: " + PathToUtf8(m_prefix) +
            "\n\nKurulumun eklediği dosyalar, PATH girdisi, IzekipUI_DIR değişkeni, Başlat menüsü\n"
            "kısayolu ve Programlar ve Özellikler kaydı geri alınacak. Bu klasöre sizin\n"
            "eklediğiniz dosyalara dokunulmaz.",
        "body"));
    pages->AddChild(m_pageUninstall);

    for (const auto& page : {m_pageWelcome, m_pageLocation, m_pageOptions, m_pageProgress, m_pageDone, m_pageUninstall}) {
        page->AddClass("hidden");
    }

    // ---- Footer ----
    auto footer = MakePanel("footer");
    m_back = MakeButton("Geri", "btn-secondary");
    m_next = MakeButton("İleri", "btn-primary");
    m_cancel = MakeButton("İptal", "btn-secondary");
    m_back.widget->SetOnClick([this] { OnBack(); });
    m_next.widget->SetOnClick([this] { OnNext(); });
    m_cancel.widget->SetOnClick([this] { m_quit = true; });
    footer->AddChild(m_back.widget);
    footer->AddChild(m_next.widget);
    footer->AddChild(m_cancel.widget);
    m_root->AddChild(footer);

    SetPathText(m_prefix);
}

void App::ShowPage(Page page) {
    m_page = page;
    m_pageWelcome->SetClass("hidden", page != Page::Welcome);
    m_pageLocation->SetClass("hidden", page != Page::Location);
    m_pageOptions->SetClass("hidden", page != Page::Options);
    m_pageProgress->SetClass("hidden", page != Page::Progress);
    m_pageDone->SetClass("hidden", page != Page::Done);
    m_pageUninstall->SetClass("hidden", page != Page::UninstallConfirm);

    const bool showBack = page == Page::Location || page == Page::Options;
    const bool showNext = page != Page::Progress;
    const bool showCancel = page != Page::Progress && page != Page::Done;
    m_back.widget->SetClass("hidden", !showBack);
    m_next.widget->SetClass("hidden", !showNext);
    m_cancel.widget->SetClass("hidden", !showCancel);

    switch (page) {
        case Page::Welcome: m_next.label->SetText("İleri"); m_stepLabel->SetText("Adım 1/4"); break;
        case Page::Location: m_next.label->SetText("İleri"); m_stepLabel->SetText("Adım 2/4"); break;
        case Page::Options: m_next.label->SetText("Kur"); m_stepLabel->SetText("Adım 3/4"); break;
        case Page::Progress: m_stepLabel->SetText(m_uninstallMode ? "Kaldırma" : "Adım 4/4"); break;
        case Page::Done: m_next.label->SetText("Bitir"); m_stepLabel->SetText(""); break;
        case Page::UninstallConfirm: m_next.label->SetText("Kaldır"); m_stepLabel->SetText("Kaldırma"); break;
    }
    if (m_controller) {
        m_controller->SetFocus(page == Page::Location ? m_pathField.get() : nullptr);
    }
    MarkDirty();
}

InstallOptions App::GatherOptions() const {
    InstallOptions o;
    o.scope = m_scopeGroup.GetSelectedIndex() == 1 ? Scope::AllUsers : Scope::CurrentUser;
    o.prefix = PathFromUtf8(Trim(m_pathField->GetBuffer().GetTextUtf8()));
    o.backend = (m_hasVulkan && m_backendGroup.GetSelectedIndex() == 1) ? Backend::Vulkan : Backend::D3D11;
    o.addToPath = m_optPath->IsChecked();
    o.setCmakeDirEnv = m_optEnv->IsChecked();
    o.registerUninstall = m_optUninstall->IsChecked();
    o.startMenuShortcut = m_optShortcut->IsChecked();
    return o;
}

void App::OnNext() {
    switch (m_page) {
        case Page::Welcome: ShowPage(Page::Location); break;
        case Page::Location: {
            const InstallOptions o = GatherOptions();
            if (o.prefix.empty() || !o.prefix.is_absolute()) {
                ShowError(*m_locationError, "Lütfen tam bir klasör yolu girin (örneğin C:\\Kurulum\\IzekipUI).");
                return;
            }
            HideError(*m_locationError);
            ShowPage(Page::Options);
            break;
        }
        case Page::Options: StartInstall(); break;
        case Page::UninstallConfirm: StartUninstall(); break;
        case Page::Done: m_quit = true; break;
        case Page::Progress: break;
    }
}

void App::OnBack() {
    if (m_page == Page::Location) ShowPage(Page::Welcome);
    else if (m_page == Page::Options) ShowPage(Page::Location);
}

void App::StartInstall() {
    const InstallOptions options = GatherOptions();
    HideError(*m_optionsError);

    if (options.scope == Scope::AllUsers && !WinSys::IsElevated() && !m_args.noSystem) {
        Args child;
        child.mode = Mode::Install;
        child.autoInstall = true;
        child.scope = options.scope;
        child.prefix = options.prefix;
        child.backend = options.backend;
        child.payload = fs::absolute(m_payloadRoot);
        child.addToPath = options.addToPath;
        child.setCmakeDirEnv = options.setCmakeDirEnv;
        child.registerUninstall = options.registerUninstall;
        child.startMenuShortcut = options.startMenuShortcut;
        switch (WinSys::Relaunch(WinSys::ExePath(), SerializeArgs(child), true)) {
            case WinSys::LaunchResult::Ok: m_exitCode = 0; m_quit = true; return;
            case WinSys::LaunchResult::Cancelled:
                ShowPage(Page::Options);
                ShowError(*m_optionsError, "Yönetici izni verilmedi. \"Tüm kullanıcılar\" kurulumu için onay gerekiyor.");
                return;
            case WinSys::LaunchResult::Failed:
                ShowPage(Page::Options);
                ShowError(*m_optionsError, "Kurulum yönetici olarak başlatılamadı.");
                return;
        }
    }

    std::string error;
    if (!m_engine.BeginInstall(options, m_payloadRoot, WinSys::ExeDir(), IZEKIP_VERSION_STRING, m_args.noSystem,
                               &error)) {
        ShowPage(Page::Location);
        ShowError(*m_locationError, error);
        return;
    }
    m_progressTitle->SetText("Kuruluyor");
    m_engineActive = true;
    ShowPage(Page::Progress);
}

void App::StartUninstall() {
    std::string error;
    if (!m_engine.BeginUninstall(m_prefix, m_args.noSystem, &error)) {
        m_doneTitle->SetText("Kaldırma başarısız");
        m_doneDetail->SetText(error);
        m_openFolder.widget->AddClass("hidden");
        m_rollback.widget->AddClass("hidden");
        ShowPage(Page::Done);
        return;
    }
    m_progressTitle->SetText("Kaldırılıyor");
    m_engineActive = true;
    ShowPage(Page::Progress);
}

void App::OnEngineFinished() {
    m_engineActive = false;
    const auto& status = m_engine.GetStatus();
    const bool ok = !status.failed;
    m_exitCode = ok ? 0 : 1;

    if (m_uninstallMode) {
        m_doneTitle->SetText(ok ? "Kaldırma tamamlandı" : "Kaldırma başarısız");
        m_doneDetail->SetText(ok ? "İzekip UI SDK bilgisayarınızdan kaldırıldı." : status.error);
        m_openFolder.widget->AddClass("hidden");
        m_rollback.widget->AddClass("hidden");
    } else if (ok) {
        const InstallOptions o = GatherOptions();
        std::string detail = "İzekip UI SDK kuruldu:\n" + PathToUtf8(o.prefix) + "\n\n";
        detail += "Yeni bir proje başlatmak için: cssfr create projeAdi\n";
        if (o.addToPath || o.setCmakeDirEnv) {
            detail += "PATH / IzekipUI_DIR değişiklikleri yeni açılan terminallerde geçerli olur.";
        }
        m_doneTitle->SetText("Kurulum tamamlandı");
        m_doneDetail->SetText(detail);
        m_openFolder.widget->SetClass("hidden", false);
        m_rollback.widget->AddClass("hidden");
    } else {
        m_doneTitle->SetText("Kurulum başarısız");
        m_doneDetail->SetText(status.error);
        m_openFolder.widget->AddClass("hidden");
        m_rollback.widget->SetClass("hidden", false);
    }
    ShowPage(Page::Done);
}

int App::Run() {
    Platform::WindowDesc desc;
    desc.title = m_uninstallMode ? "İzekip UI SDK Kaldırma" : "İzekip UI SDK Kurulumu";
    desc.width = static_cast<uint32_t>(kWidth);
    desc.height = static_cast<uint32_t>(kHeight);
    desc.resizable = false;
    m_window = std::make_unique<Platform::Window>(desc);

    auto renderer = Render::CreateRenderer(Render::GetCompiledInBackend());
    Render::RendererCreateInfo info;
    info.nativeWindowHandle = m_window->GetNativeHandle();
    info.glfwWindow = m_window->GetGLFWHandle();
    info.initialWidth = m_window->GetWidth();
    info.initialHeight = m_window->GetHeight();
    renderer->Initialize(info);
    m_window->SetResizeCallback([&](uint32_t w, uint32_t h) { renderer->Resize(w, h); });

    BuildTree();
    m_controller = std::make_unique<InteractionController>(*m_root);

    m_window->SetCursorPosCallback([this](double x, double y) {
        m_controller->OnMouseMove(static_cast<float>(x), static_cast<float>(y));
    });
    m_window->SetMouseButtonCallback([this](Platform::MouseButton button, Platform::MouseAction action) {
        if (button == Platform::MouseButton::Left) {
            m_controller->OnMouseButton(action == Platform::MouseAction::Press);
        }
    });
    // The path field is a single-line input: only character input and caret/edit keys are
    // routed to it; Enter/Up/Down/Tab are ignored.
    m_window->SetCharCallback([this](char32_t codepoint) {
        if (m_controller->GetFocused() == m_pathField.get()) {
            m_pathField->OnCharInput(codepoint);
            m_pathEdited = true;
        }
    });
    m_window->SetKeyCallback([this](Platform::Key key, Platform::KeyAction action) {
        if (action == Platform::KeyAction::Release || m_controller->GetFocused() != m_pathField.get()) {
            return;
        }
        TextBuffer& buffer = m_pathField->GetBuffer();
        switch (key) {
            case Platform::Key::Backspace: buffer.Backspace(); m_pathEdited = true; break;
            case Platform::Key::Delete: buffer.DeleteForward(); m_pathEdited = true; break;
            case Platform::Key::Left: buffer.MoveLeft(); break;
            case Platform::Key::Right: buffer.MoveRight(); break;
            case Platform::Key::Home: buffer.MoveHome(); break;
            case Platform::Key::End: buffer.MoveEnd(); break;
            default: break;
        }
    });

    Css::StyleSheet sheet = Css::StyleSheet::LoadFromFile("setup.css");
    Css::LexborBridge bridge;
    bridge.Build(*m_root);

    if (m_uninstallMode) {
        ShowPage(Page::UninstallConfirm);
    } else {
        ShowPage(Page::Welcome);
        if (m_args.autoInstall) {
            StartInstall(); // elevated relaunch: options already populated from args
        }
    }

    auto lastFrame = std::chrono::steady_clock::now();
    while (!m_window->ShouldClose() && !m_quit) {
        m_window->PollEvents();

        bool restyle = m_controller->Update();
        restyle = restyle || m_dirty;
        m_dirty = false;
        bool relayout = restyle;

        if (m_engineActive) {
            const auto& status = m_engine.Step(std::chrono::milliseconds(12));
            m_progressBar->SetValue(status.progress);
            m_statusLabel->SetText(status.message);
            relayout = true;
            if (status.done) {
                OnEngineFinished();
                restyle = true;
                m_dirty = false;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - lastFrame).count();
        lastFrame = now;
        m_pathField->Tick(dt);

        if (restyle) {
            Css::StyleApplier::Apply(bridge, sheet, *m_root);
        }
        const AnimationTickResult tick = TickWidgetTreeAnimations(*m_root, dt);
        if (relayout || tick.layoutAffected) {
            m_root->CalculateLayout(kWidth, kHeight);
        }

        renderer->BeginFrame();
        m_root->OnPaint(*renderer);
        renderer->EndFrame();
        renderer->Present();
    }

    renderer->Shutdown();
    return m_exitCode;
}

} // namespace

int RunGui(const Args& args) {
    App app(args);
    return app.Run();
}

} // namespace IzekipSetup
