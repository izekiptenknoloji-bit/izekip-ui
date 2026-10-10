#include "Platform/Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <stdexcept>

namespace IzekipUI::Platform {

namespace {
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

    glfwSetWindowUserPointer(m_handle, this);
    glfwSetFramebufferSizeCallback(m_handle, &Window::FramebufferSizeCallback);
    glfwSetCursorPosCallback(m_handle, &Window::CursorPosCallbackThunk);
    glfwSetMouseButtonCallback(m_handle, &Window::MouseButtonCallbackThunk);
}

Window::~Window() {
    if (m_handle) {
        glfwDestroyWindow(m_handle);
    }
    ReleaseGlfw();
}

bool Window::ShouldClose() const {
    return glfwWindowShouldClose(m_handle) != 0;
}

void Window::PollEvents() {
    glfwPollEvents();
}

void* Window::GetNativeHandle() const {
    return glfwGetWin32Window(m_handle);
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

void Window::FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self) {
        return;
    }
    self->m_width = static_cast<uint32_t>(width);
    self->m_height = static_cast<uint32_t>(height);
    if (self->m_onResize) {
        self->m_onResize(self->m_width, self->m_height);
    }
}

void Window::CursorPosCallbackThunk(GLFWwindow* window, double x, double y) {
    auto* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (self && self->m_onCursorPos) {
        self->m_onCursorPos(x, y);
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
    self->m_onMouseButton(mouseButton, mouseAction);
}

} // namespace IzekipUI::Platform
