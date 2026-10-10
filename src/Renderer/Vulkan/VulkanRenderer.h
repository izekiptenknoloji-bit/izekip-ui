#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "Renderer/IRenderer.h"

struct GLFWwindow;

namespace IzekipUI::Render {

// Vulkan backend: same SDF rect shader as D3D11 (src/Renderer/Shaders/RectShader.hlsl,
// compiled to SPIR-V at build time via dxc.exe -spirv), same single-draw-call-per-rect
// model. Scope matches D3D11Renderer's feature level deliberately (single frame in
// flight, no staging-buffer uploads, no pipeline cache, no MSAA) -- see the plan doc for
// the full list of named scope cuts.
class VulkanRenderer : public IRenderer {
public:
    bool Initialize(const RendererCreateInfo& info) override;
    void Shutdown() override;

    void Resize(uint32_t width, uint32_t height) override;

    void BeginFrame() override;
    void EndFrame() override;
    void Present() override;

    void DrawRect(const Rect& rect, const RectPaint& paint) override;

    RendererBackend GetBackend() const override { return RendererBackend::Vulkan; }

private:
    void CreateInstance();
    void CreateDebugMessenger();
    void CreateSurface(GLFWwindow* window);
    void PickPhysicalDeviceAndQueueFamily();
    void CreateLogicalDeviceAndQueue();
    void CreateSwapchain(uint32_t width, uint32_t height);
    void CreateSwapchainImageViews();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateDescriptorSetLayoutAndPool();
    void CreatePipeline();
    void CreateCommandPoolAndBuffer();
    void CreateSyncObjects();
    void CreateRenderFinishedSemaphores(); // one per swapchain image -- see Present() for why
    void CreateQuadVertexBuffer();
    void CreateUniformRingBuffers();
    void DestroySwapchainAndDependents();
    void RecreateSwapchain(); // DestroySwapchainAndDependents + Create{Swapchain,ImageViews,Framebuffers}

    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;
    VkShaderModule LoadShaderModule(const char* path) const;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0;
    VkDeviceSize m_minUboAlignment = 0;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent{};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_swapchainFramebuffers;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    // One per swapchain image (not a single shared semaphore): a renderFinished semaphore
    // is signaled by vkQueueSubmit and waited on by the present engine, whose completion
    // our single in-flight fence does NOT track (the fence only tracks command buffer
    // execution). With 2+ swapchain images, reusing one semaphore across different images'
    // presents can try to re-signal it while an earlier present still has it in use.
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    VkFence m_inFlightFence = VK_NULL_HANDLE;

    VkBuffer m_quadVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_quadVertexMemory = VK_NULL_HANDLE;

    static constexpr uint32_t kMaxDrawsPerFrame = 1024;
    VkBuffer m_vsUniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vsUniformMemory = VK_NULL_HANDLE;
    VkBuffer m_psUniformBuffer = VK_NULL_HANDLE;
    VkDeviceMemory m_psUniformMemory = VK_NULL_HANDLE;
    void* m_vsUniformMapped = nullptr;
    void* m_psUniformMapped = nullptr;
    VkDeviceSize m_vsStride = 0;
    VkDeviceSize m_psStride = 0;
    uint32_t m_currentDrawSlot = 0;
    uint32_t m_currentImageIndex = 0;
    bool m_frameSkipped = false; // true when BeginFrame bailed out on VK_ERROR_OUT_OF_DATE_KHR

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace IzekipUI::Render
