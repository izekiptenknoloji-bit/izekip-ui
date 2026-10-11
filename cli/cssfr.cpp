// cssfr -- IzekipUI project scaffolding CLI.
//
// Generates a new, ready-to-build consumer project: a CMakeLists.txt wired for
// find_package(IzekipUI), a minimal main.cpp, and a starter style.css. The goal is that a
// new user only has to write code -- not re-derive the asset-copy add_custom_command
// blocks every example in the IzekipUI repo itself currently hand-rolls.
//
// A native C++ tool (not a Python script) so it compiles to a real, signable .exe as part
// of the same build/install as the rest of the SDK, with zero runtime dependency on
// anything but the C++ standard library -- the four template files are embedded as string
// literals below rather than read from disk, so this binary is fully self-contained.
//
// Usage:
//   cssfr create <name> [--izekip-path <prefix>] [--output-dir <dir>]

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const char* kCMakeListsTemplate = R"TPL(cmake_minimum_required(VERSION 3.21)
project(@@PROJECT_NAME@@ CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

@@PREFIX_PATH_LINE@@

find_package(IzekipUI REQUIRED)

add_executable(@@PROJECT_NAME@@ main.cpp)
target_link_libraries(@@PROJECT_NAME@@ PRIVATE IzekipUI::IzekipUI)

# Copies the fonts + shader (source or precompiled, matching whichever backend your
# IzekipUI install was built with) next to this executable -- see
# cmake/IzekipUIAssets.cmake in the IzekipUI repo if you want to see what this does.
izekip_ui_copy_runtime_assets(TARGET @@PROJECT_NAME@@)

# style.css is this project's own, not IzekipUI's -- copy it yourself.
add_custom_command(TARGET @@PROJECT_NAME@@ POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/style.css"
        "$<TARGET_FILE_DIR:@@PROJECT_NAME@@>/style.css"
)
)TPL";

const char* kMainCppTemplate = R"TPL(#include <chrono>
#include <memory>

#include "CSS/LexborBridge.h"
#include "CSS/StyleApplier.h"
#include "CSS/StyleSheet.h"
#include "Platform/Window.h"
#include "Renderer/IRenderer.h"
#include "Widgets/Label.h"
#include "Widgets/Panel.h"

using namespace IzekipUI;

namespace {

void MarkHoveredChain(Widget* widget, bool hovered) {
    for (Widget* w = widget; w != nullptr; w = w->GetParent()) {
        w->SetHovered(hovered);
    }
}

} // namespace

int main() {
    Platform::WindowDesc windowDesc;
    windowDesc.title = "@@PROJECT_NAME@@";
    windowDesc.width = 800;
    windowDesc.height = 600;
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

    auto root = std::make_shared<Widgets::Panel>();
    root->SetId("root");

    auto label = std::make_shared<Widgets::Label>();
    label->SetText("Hello, @@PROJECT_NAME@@!");
    root->AddChild(label);

    Css::StyleSheet sheet = Css::StyleSheet::LoadFromFile("style.css");
    Css::LexborBridge bridge;
    bridge.Build(*root);
    Css::StyleApplier::Apply(bridge, sheet, *root);
    root->CalculateLayout(static_cast<float>(window.GetWidth()), static_cast<float>(window.GetHeight()));

    Widget* hoveredWidget = nullptr;
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
)TPL";

const char* kStyleCssTemplate = R"TPL(#root {
    background-color: #1E1E1E;
    width: 800px;
    height: 600px;
    flex-direction: column;
    justify-content: center;
    align-items: center;
}

Label {
    color: #FFFFFF;
    font-size: 24px;
}
)TPL";

const char* kGitignoreTemplate = R"TPL(build/
build-*/
.vs/
.vscode/
)TPL";

bool IsValidProjectName(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    if (!(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_')) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    });
}

std::string Replace(std::string text, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = text.find(from, pos)) != std::string::npos) {
        text.replace(pos, from.size(), to);
        pos += to.size();
    }
    return text;
}

void WriteFile(const fs::path& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

int PrintUsage() {
    std::cout << "Usage: cssfr create <name> [--izekip-path <prefix>] [--output-dir <dir>]\n";
    return 2;
}

int CmdCreate(const std::vector<std::string>& args) {
    std::string name;
    std::string izekipPath;
    std::string outputDir;

    for (size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        if (a == "--izekip-path" && i + 1 < args.size()) {
            izekipPath = args[++i];
        } else if (a == "--output-dir" && i + 1 < args.size()) {
            outputDir = args[++i];
        } else if (name.empty() && a.rfind("--", 0) != 0) {
            name = a;
        }
    }

    if (!IsValidProjectName(name)) {
        std::cerr << "error: '" << name << "' is not a valid project name "
                  << "(must start with a letter/underscore, then letters/digits/underscores "
                  << "only -- it becomes both the directory name and the CMake "
                  << "project()/target name)\n";
        return 2;
    }

    const fs::path outputRoot = outputDir.empty() ? fs::current_path() : fs::absolute(outputDir);
    const fs::path projectDir = outputRoot / name;

    if (fs::exists(projectDir) && !fs::is_empty(projectDir)) {
        std::cerr << "error: " << projectDir.string() << " already exists and is not empty\n";
        return 2;
    }

    std::string prefixPathLine;
    if (!izekipPath.empty()) {
        const std::string posixPath = fs::absolute(izekipPath).generic_string();
        prefixPathLine =
            "# Baked in from `cssfr create --izekip-path " + izekipPath + "` -- NOT portable if\n"
            "# this project is copied to another machine; edit the path below or delete this\n"
            "# line and pass -DCMAKE_PREFIX_PATH yourself if you move this project.\n"
            "list(APPEND CMAKE_PREFIX_PATH \"" + posixPath + "\")";
    } else {
        prefixPathLine =
            "# No --izekip-path was given when this project was generated. Point CMake at your\n"
            "# IzekipUI install prefix one of these ways:\n"
            "#   cmake -B build -DCMAKE_PREFIX_PATH=/path/to/izekip-install\n"
            "# or set the IzekipUI_DIR environment variable / CMake cache variable to\n"
            "# <prefix>/lib/cmake/IzekipUI, or uncomment and edit the line below.\n"
            "# list(APPEND CMAKE_PREFIX_PATH \"/path/to/izekip-install\")";
    }

    fs::create_directories(projectDir);

    auto render = [&](const char* tpl) {
        std::string text = Replace(tpl, "@@PROJECT_NAME@@", name);
        text = Replace(text, "@@PREFIX_PATH_LINE@@", prefixPathLine);
        return text;
    };

    WriteFile(projectDir / "CMakeLists.txt", render(kCMakeListsTemplate));
    WriteFile(projectDir / "main.cpp", render(kMainCppTemplate));
    WriteFile(projectDir / "style.css", render(kStyleCssTemplate));
    WriteFile(projectDir / ".gitignore", render(kGitignoreTemplate));

    std::cout << "Created " << projectDir.string() << "\n\n";
    std::cout << "Next steps:\n";
    std::cout << "  cd " << projectDir.string() << "\n";
    std::cout << "  cmake -B build";
    if (izekipPath.empty()) {
        std::cout << " -DCMAKE_PREFIX_PATH=/path/to/izekip-install";
    }
    std::cout << "\n  cmake --build build\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return PrintUsage();
    }
    const std::string command = argv[1];
    if (command != "create") {
        return PrintUsage();
    }

    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    return CmdCreate(args);
}
