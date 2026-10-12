#include "Platform/Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>

namespace IzekipUI::Platform {

namespace {

// GLFW calls these thunks from C code, where an escaping C++ exception is undefined behaviour.
// A throwing application callback is logged and dropped instead of unwinding into GLFW.
template <typename Callback, typename... Args>
void RunUserCallback(const char* what, Callback& callback, Args&&... args) {
    try {
        callback(std::forward<Args>(args)...);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "IzekipUI: %s callback threw: %s\n", what, error.what());
    } catch (...) {
        std::fprintf(stderr, "IzekipUI: %s callback threw a non-standard exception\n", what);
    }
}
int s_glfwRefCount = 0;

void EnsureGlfwInitialized() {
    if (s_glfwRefCount == 0) {
        if (!glfwInit()) {
            throw std::runtime_error("glfwInit failed");
        }
    }
    ++s_glfwRefCount;
}

void ReleaseGlfw() {
    --s_glfwRefCount;
    if (s_glfwRefCount == 0) {
        glfwTerminate();
    }
}
} // namespace

Window::Window(const WindowDesc& desc) : m_width(desc.width), m_height(desc.height) {
    EnsureGlfwInitialized();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, desc.resizable ? GLFW_TRUE : GLFW_FALSE);

    m_handle = glfwCreateWindow(static_cast<int>(desc.width), static_cast<int>(desc.height),
                                 desc.title.c_str(), nullptr, nullptr);
    if (!m_handle) {
        ReleaseGlfw();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    // The OS/compositor can clamp the actual window size below what was requested (e.g. a
    // requested height taller than the screen) -- glfwCreateWindow silently honors the clamp
    // rather than failing. m_width/m_height must reflect that real size from the start, not
    // just desc.width/height, because FramebufferSizeCallback (registered below) only fires on
    // a LATER resize and would never catch a clamp that already happened during creation.
    int actualWidth = 0;
    int actualHeight = 0;
    glfwGetFramebufferSize(m_handle, &actualWidth, &actualHeight);
    m_width = static_cast<uint32_t>(actualWidth);
    m_height = static_cast<uint32_t>(actualHeight);

    glfwSetWindowUserPointer(m_handle, this);
    glfwSetFramebufferSizeCallback(m_handle, &Window::FramebufferSizeCallback);
    glfwSetCursorPosCallback(m_handle, &Window::CursorPosCallbackThunk);
    glfwSetMouseButtonCallback(m_handle, &Window::MouseButtonCallbackThunk);
    glfwSetKeyCallback(m_handle, &Window::KeyCallbackThunk);
    glfwSetCharCallback(m_handle, &Window::CharCallbackThunk);
}

Window::~Window() {
    for (GLFWcursor* cursor : m_cursors) {
        if (cursor) {
            glfwDestroyCursor(cursor);
        }
    }
    if (m_handle) {
        glfwDestroyWindow(m_handle);
    }
    ReleaseGlfw();
}

void Window::SetCursorShape(CursorShape shape) {
    if (shape == m_currentCursor) {
        return;
    }
    const auto index = static_cast<std::size_t>(shape);
    if (!m_cursors[index]) {
        constexpr int kGlfwShapes[kCursorShapeCount] = {
            GLFW_ARROW_CURSOR, GLFW_POINTING_HAND_CURSOR, GLFW_IBEAM_CURSOR,
            GLFW_CROSSHAIR_CURSOR, GLFW_NOT_ALLOWED_CURSOR};
        m_cursors[index] = glfwCreateStandardCursor(kGlfwShapes[index]);
    }
    glfwSetCursor(m_handle, m_cursors[index]);
    m_currentCursor = shape;
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_handle) != 0;
}

void Window::PollEvents() {
    glfwPollEvents();
}

void* Window::GetNativeHandle() const {
#if defined(_WIN32)
    return glfwGetWin32Window(m_handle);
#else
    return nullptr; // only the Windows D3D11 backend consumes the HWND
#endif
}

void* Window::GetGLFWHandle() const {
    return m_handle;
}

uint32_t Window::GetWidth() const {
    return m_width;
}

uint32_t Window::GetHeight() const {
    return m_height;
}

void Window::SetResizeCallback(ResizeCallback callback) {
    m_onResize = std::move(callback);
}

void Window::SetCursorPosCallback(CursorPosCallback callback) {
    m_onCursorPos = std::move(callback);
}

void Window::SetMouseButtonCallback(MouseButtonCallback callback) {
    m_onMouseButton = std::move(callback);
}

void Window::SetKeyCallback(KeyCallback callback) {
    m_onKey = std::move(callback);
}

void Window::SetCharCallback(CharCallback callback) {
    m_onChar = std::move(callback);
}

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self) {
        return;
    }
    self->m_width = static_cast<uint32_t>(width);
    self->m_height = static_cast<uint32_t>(height);
    if (self->m_onResize) {
        RunUserCallback("resize", self->m_onResize, self->m_width, self->m_height);
    }
}

void Window::CursorPosCallbackThunk(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_onCursorPos) {
        RunUserCallback("cursor", self->m_onCursorPos, x, y);
    }
}

void Window::MouseButtonCallbackThunk(GLFWwindow* window, int button, int action, int /*mods*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self || !self->m_onMouseButton) {
        return;
    }
    if (action != GLFW_PRESS && action != GLFW_RELEASE) {
        return; // GLFW_REPEAT is not meaningful for mouse buttons
    }

    MouseButton mouseButton = MouseButton::Other;
    if (button == GLFW_MOUSE_BUTTON_LEFT) mouseButton = MouseButton::Left;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT) mouseButton = MouseButton::Right;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE) mouseButton = MouseButton::Middle;

    const MouseAction mouseAction = action == GLFW_PRESS ? MouseAction::Press : MouseAction::Release;
    RunUserCallback("mouse button", self->m_onMouseButton, mouseButton, mouseAction);
}

void Window::KeyCallbackThunk(GLFWwindow* window, int key, int /*scancode*/, int action, int /*mods*/) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self || !self->m_onKey) {
        return;
    }

    Key translatedKey = Key::Unknown;
    switch (key) {
        case GLFW_KEY_BACKSPACE: translatedKey = Key::Backspace; break;
        case GLFW_KEY_DELETE: translatedKey = Key::Delete; break;
        case GLFW_KEY_ENTER: translatedKey = Key::Enter; break;
        case GLFW_KEY_KP_ENTER: translatedKey = Key::Enter; break;
        case GLFW_KEY_TAB: translatedKey = Key::Tab; break;
        case GLFW_KEY_ESCAPE: translatedKey = Key::Escape; break;
        case GLFW_KEY_LEFT: translatedKey = Key::Left; break;
        case GLFW_KEY_RIGHT: translatedKey = Key::Right; break;
        case GLFW_KEY_UP: translatedKey = Key::Up; break;
        case GLFW_KEY_DOWN: translatedKey = Key::Down; break;
        case GLFW_KEY_HOME: translatedKey = Key::Home; break;
        case GLFW_KEY_END: translatedKey = Key::End; break;
        default: return; // not a key this library translates -- drop it
    }

    KeyAction translatedAction;
    if (action == GLFW_PRESS) translatedAction = KeyAction::Press;
    else if (action == GLFW_RELEASE) translatedAction = KeyAction::Release;
    else if (action == GLFW_REPEAT) translatedAction = KeyAction::Repeat;
    else return;

    RunUserCallback("key", self->m_onKey, translatedKey, translatedAction);
}

void Window::CharCallbackThunk(GLFWwindow* window, unsigned int codepoint) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_onChar) {
        RunUserCallback("char", self->m_onChar, static_cast<char32_t>(codepoint));
    }
}

} // namespace IzekipUI::Platform
