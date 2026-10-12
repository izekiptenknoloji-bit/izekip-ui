#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

#include "Renderer/IRenderer.h"

struct GLFWwindow;

namespace IzekipUI::Render {

class GlyphAtlas;

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
    void DrawTextQuad(const Rect& rect, const UVRect& atlasUV, const Color& color,
                      float glyphScale, FontId fontId) override;
    void DrawBackdropBlurRect(const Rect& rect, const BackdropBlurPaint& paint) override;
    void DrawImage(const std::string& imagePath, const Rect& rect, float opacity) override;
    void SetWorldTransform(const Transform2D& transform) override { m_worldTransform = transform; }

    RendererBackend GetBackend() const override { return RendererBackend::Vulkan; }

private:
    // Creates the instance, surface, device, pipelines and every GPU resource for the current
    // size. Used by Initialize and by device-loss recovery.
    void CreateDeviceResources();
    // Full teardown and rebuild after VK_ERROR_DEVICE_LOST. Failure keeps m_deviceLost set so
    // the next Present() retries instead of throwing into the host application.
    void TryRecoverDevice();
    void MarkDeviceLost();
    void DestroyInstanceAndSurface();

    void CreateInstance();
    void CreateDebugMessenger();
    void CreateSurface(GLFWwindow* window);
    void PickPhysicalDeviceAndQueueFamily();
    void CreateLogicalDeviceAndQueue();
    void CreateSwapchain(uint32_t width, uint32_t height);
    void CreateSwapchainImageViews();
    void CreateRenderPass();
    // Second render pass, same attachment format/sample-count as m_renderPass (so the
    // same VkPipeline is compatible with both -- render pass compatibility only looks at
    // format/sample-count, not load/store ops), but loadOp=LOAD instead of CLEAR. Used to
    // resume rendering after DrawBackdropBlurRect's end-render-pass + vkCmdCopyImage,
    // since vkCmdCopyImage cannot be called inside an active render pass instance (core
    // Vulkan spec constraint) -- see DrawBackdropBlurRect.
    void CreateRenderPassResume();
    void CreateFramebuffers();
    void CreateDescriptorSetLayoutAndPool();
    void CreatePipeline();
    void CreateCommandPoolAndBuffer();
    void CreateSyncObjects();
    void CreateRenderFinishedSemaphores(); // one per swapchain image -- see Present() for why
    void CreateQuadVertexBuffer();
    void CreateUniformRingBuffers();
    void CreateFontAtlasTexture();
    void UploadFontAtlasTexture(const GlyphAtlas& atlas, VkImage& outImage, VkDeviceMemory& outMemory,
                                 VkImageView& outView, VkSampler& outSampler, uint32_t imageBinding,
                                 uint32_t samplerBinding);
    void CreateBackdropScratchImage(); // viewport-sized, GENERAL layout (see .cpp for why)
    void DestroySwapchainAndDependents();
    void RecreateSwapchain(); // DestroySwapchainAndDependents + Create{Swapchain,ImageViews,Framebuffers}

    uint32_t FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const;
    VkShaderModule LoadShaderModule(const char* path) const;

    // The pipeline/vertex-buffer/viewport/scissor bind sequence shared between BeginFrame
    // and the render-pass-resume point in DrawBackdropBlurRect (command-buffer-level
    // bindings aren't guaranteed to survive an end+begin-render-pass pair, so this is
    // re-run after every resume rather than relied on to persist).
    void BindFrameDrawState();

    Transform2D m_worldTransform;
    GLFWwindow* m_glfwWindow = nullptr;
    bool m_deviceLost = false;
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
    VkRenderPass m_renderPassResume = VK_NULL_HANDLE; // loadOp=LOAD variant, see CreateRenderPassResume
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

    VkImage m_fontAtlasImage = VK_NULL_HANDLE;
    VkDeviceMemory m_fontAtlasImageMemory = VK_NULL_HANDLE;
    VkImageView m_fontAtlasImageView = VK_NULL_HANDLE;
    VkSampler m_fontAtlasSampler = VK_NULL_HANDLE;

    VkImage m_fontAtlasImage2 = VK_NULL_HANDLE;
    VkDeviceMemory m_fontAtlasImageMemory2 = VK_NULL_HANDLE;
    VkImageView m_fontAtlasImageView2 = VK_NULL_HANDLE;
    VkSampler m_fontAtlasSampler2 = VK_NULL_HANDLE;

    // backdrop-filter scratch image: a GPU-to-GPU copy of whatever's been painted to the
    // swapchain image so far, sampled by DrawBackdropBlurRect's blur shader. Viewport-sized,
    // recreated alongside the swapchain in RecreateSwapchain().
    VkImage m_backdropScratchImage = VK_NULL_HANDLE;
    VkDeviceMemory m_backdropScratchImageMemory = VK_NULL_HANDLE;
    VkImageView m_backdropScratchImageView = VK_NULL_HANDLE;
    VkSampler m_backdropSampler = VK_NULL_HANDLE;

    // 8 showcase demo rows x multi-line monospace code text can easily exceed 1000
    // glyph-quad draws in a single frame; D3D11 has no equivalent cap (each draw does its
    // own Map/Unmap) but this ring buffer is sized for a fixed draw count up front.
    static constexpr uint32_t kMaxDrawsPerFrame = 4096;
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
