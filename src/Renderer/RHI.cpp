#include "Renderer/IRenderer.h"
#include "Renderer/RenderBackendConfig.h"

#if IZEKIP_RENDER_BACKEND_D3D11
#include "D3D11/D3D11Renderer.h"
#elif IZEKIP_RENDER_BACKEND_VULKAN
#include "Vulkan/VulkanRenderer.h"
#endif

#include <stdexcept>

namespace IzekipUI::Render {

RendererBackend GetCompiledInBackend() {
#if IZEKIP_RENDER_BACKEND_D3D11
    return RendererBackend::D3D11;
#elif IZEKIP_RENDER_BACKEND_VULKAN
    return RendererBackend::Vulkan;
#else
#error "No render backend selected"
#endif
}

std::unique_ptr<IRenderer> CreateRenderer(RendererBackend backend) {
#if IZEKIP_RENDER_BACKEND_D3D11
    if (backend != RendererBackend::D3D11) {
        throw std::runtime_error("IzekipUI was built with the D3D11 backend only");
    }
    return std::make_unique<D3D11Renderer>();
#elif IZEKIP_RENDER_BACKEND_VULKAN
    if (backend != RendererBackend::Vulkan) {
        throw std::runtime_error("IzekipUI was built with the Vulkan backend only");
    }
    return std::make_unique<VulkanRenderer>();
#else
#error "No render backend selected"
#endif
}

} // namespace IzekipUI::Render
