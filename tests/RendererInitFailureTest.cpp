// GPU-dependent, Vulkan only: points the Vulkan loader at an ICD that does not exist, so no device
// can be found. Initialize must report that as a failure (false) instead of throwing out of main.

#undef NDEBUG // tests must check their assertions in every configuration, including Release
#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "Platform/Window.h"
#include "Renderer/IRenderer.h"

using namespace IzekipUI;

int main() {
#if defined(_WIN32)
    _putenv_s("VK_ICD_FILENAMES", "C:\\izekip-no-such-icd.json");
#else
    setenv("VK_ICD_FILENAMES", "/nonexistent/izekip-no-such-icd.json", 1);
#endif

    Platform::Window window({.title = "IzekipUI init failure test", .width = 320, .height = 200});

    auto renderer = Render::CreateRenderer(Render::GetCompiledInBackend());
    Render::RendererCreateInfo info{
        .nativeWindowHandle = window.GetNativeHandle(),
        .glfwWindow = window.GetGLFWHandle(),
        .initialWidth = 320,
        .initialHeight = 200,
    };
    const bool started = renderer->Initialize(info);
    assert(!started);
    renderer->Shutdown();

    std::printf("RendererInitFailureTest passed\n");
    return 0;
}
