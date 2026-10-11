#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct GLFWwindow;

namespace IzekipUI::Platform {

struct WindowDesc {
    std::string title = "IzekipUI";
    uint32_t width = 1280;
    uint32_t height = 720;
    bool resizable = true;
};

enum class MouseButton { Left, Right, Middle, Other };
enum class MouseAction { Press, Release };

// Only the keys a text-editing widget needs -- anything else is dropped in the thunk
// (mirrors how MouseButtonCallbackThunk drops GLFW_REPEAT since it's irrelevant there).
enum class Key { Unknown, Backspace, Delete, Enter, Tab, Escape, Left, Right, Up, Down, Home, End };
// Unlike MouseAction, Repeat is kept -- a held-down Backspace/arrow key should repeat.
enum class KeyAction { Press, Release, Repeat };

// Thin GLFW wrapper. Created with GLFW_CLIENT_API = GLFW_NO_API since the renderer
// (D3D11 today, Vulkan later) owns its own swapchain/surface instead of an OpenGL context.
class Window {
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool ShouldClose() const;
    void PollEvents();

    // HWND on Windows (via GLFW_EXPOSE_NATIVE_WIN32), used to create the D3D11 swapchain.
    void* GetNativeHandle() const;

    // Raw GLFWwindow*, used only by the Vulkan backend's glfwCreateWindowSurface -- D3D11
    // keeps using GetNativeHandle() (the HWND) exclusively.
    void* GetGLFWHandle() const;

    uint32_t GetWidth() const;
    uint32_t GetHeight() const;

    using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
    void SetResizeCallback(ResizeCallback callback);

    // Cursor position in the same pixel space as GetWidth()/GetHeight() and the widget
    // tree's absolute Rects, so hit-testing can use it directly.
    using CursorPosCallback = std::function<void(double x, double y)>;
    void SetCursorPosCallback(CursorPosCallback callback);

    using MouseButtonCallback = std::function<void(MouseButton button, MouseAction action)>;
    void SetMouseButtonCallback(MouseButtonCallback callback);

    // Control keys (Backspace/Delete/Enter/arrows/Home/End/...) -- see enum class Key.
    using KeyCallback = std::function<void(Key key, KeyAction action)>;
    void SetKeyCallback(KeyCallback callback);

    // Actual typed text, one already-decoded Unicode codepoint at a time. This is GLFW's
    // layout-aware path (distinct from the raw-scancode key callback above) -- it is what
    // delivers Turkish characters (ş ğ ı ö ü ç ...) correctly from a Turkish keyboard
    // layout without this library doing any of its own key-to-character translation.
    using CharCallback = std::function<void(char32_t codepoint)>;
    void SetCharCallback(CharCallback callback);

private:
    static void FramebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void CursorPosCallbackThunk(GLFWwindow* window, double x, double y);
    static void MouseButtonCallbackThunk(GLFWwindow* window, int button, int action, int mods);
    static void KeyCallbackThunk(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void CharCallbackThunk(GLFWwindow* window, unsigned int codepoint);

    GLFWwindow* m_handle = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    ResizeCallback m_onResize;
    CursorPosCallback m_onCursorPos;
    MouseButtonCallback m_onMouseButton;
    KeyCallback m_onKey;
    CharCallback m_onChar;
};

} // namespace IzekipUI::Platform
