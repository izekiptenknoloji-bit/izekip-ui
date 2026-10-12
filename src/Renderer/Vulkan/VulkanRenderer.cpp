#include "VulkanRenderer.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdio>

#include "Core/AssetPath.h"
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Renderer/GlyphAtlas.h"
#include "Renderer/RectDrawData.h"

using namespace IzekipUI::Render::detail;

namespace IzekipUI::Render {

namespace {

void VkCheck(VkResult result, const char* what) {
    if (result != VK_SUCCESS) {
        throw std::runtime_error(std::string(what) + " failed (VkResult " + std::to_string(result) + ")");
    }
}

VkDeviceSize AlignUp(VkDeviceSize value, VkDeviceSize alignment) {
    if (alignment == 0) return value;
    return (value + alignment - 1) & ~(alignment - 1);
}

std::vector<char> ReadBinaryFile(const char* path) {
    std::ifstream file(ResolveAssetPath(path), std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::string("Could not open ") + path);
    }
    const std::streamsize size = file.tellg();
    file.seekg(0);
    std::vector<char> buffer(static_cast<size_t>(size));
    file.read(buffer.data(), size);
    return buffer;
}

bool HasValidationLayer() {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& layer : layers) {
        if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            return true;
        }
    }
    return false;
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT,
                                              VkDebugUtilsMessageTypeFlagsEXT,
                                              const VkDebugUtilsMessengerCallbackDataEXT* data, void*) {
    fprintf(stderr, "[IzekipUI][Vulkan] %s\n", data->pMessage);
    return VK_FALSE;
}

} // namespace

bool VulkanRenderer::Initialize(const RendererCreateInfo& info) {
    m_width = info.initialWidth;
    m_height = info.initialHeight;
    m_glfwWindow = static_cast<GLFWwindow*>(info.glfwWindow);
    m_appId = ExecutablePath().stem().string();
    Platform::SafeMode::BeginLaunch(m_appId);
    m_safeMode = Platform::SafeMode::IsActive();
    CreateDeviceResources();
    return true;
}

void VulkanRenderer::CreateDeviceResources() {
    CreateInstance();
#ifdef _DEBUG
    CreateDebugMessenger();
#endif
    CreateSurface(m_glfwWindow);
    PickPhysicalDeviceAndQueueFamily();
    CreateLogicalDeviceAndQueue();
    CreateSwapchain(m_width, m_height);
    CreateSwapchainImageViews();
    CreateRenderPass();
    CreateRenderPassResume();
    CreateFramebuffers();
    CreateDescriptorSetLayoutAndPool();
    CreatePipeline();
    CreateCommandPoolAndBuffer();
    CreateSyncObjects();
    CreateRenderFinishedSemaphores();
    CreateQuadVertexBuffer();
    CreateUniformRingBuffers();
    CreateFontAtlasTexture();
    CreateBackdropScratchImage();
}

void VulkanRenderer::CreateInstance() {
    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "IzekipUI";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "IzekipUI";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_1; // negative-viewport-height needs 1.1+

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    std::vector<const char*> layers;
#ifdef _DEBUG
    const bool useValidation = HasValidationLayer();
    if (useValidation) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
#endif

    VkInstanceCreateInfo createInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    VkCheck(vkCreateInstance(&createInfo, nullptr, &m_instance), "vkCreateInstance");
}

void VulkanRenderer::CreateDebugMessenger() {
    if (!HasValidationLayer()) {
        return; // validation layer wasn't enabled in CreateInstance either
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;

    auto createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (createFn) {
        createFn(m_instance, &createInfo, nullptr, &m_debugMessenger);
    }
}

void VulkanRenderer::CreateSurface(GLFWwindow* window) {
    VkCheck(glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface), "glfwCreateWindowSurface");
}

bool VulkanRenderer::SelectPhysicalDevice(const std::vector<VkPhysicalDevice>& devices, bool softwareOnly, bool anyType) {
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);
        if (!anyType && softwareOnly != (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)) {
            continue;
        }

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i) {
            const bool hasGraphics = (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            VkBool32 hasPresent = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &hasPresent);
            if (hasGraphics && hasPresent) {
                m_physicalDevice = device;
                m_queueFamilyIndex = i;
                m_minUboAlignment = props.limits.minUniformBufferOffsetAlignment;
                return true;
            }
        }
    }
    return false;
}

void VulkanRenderer::PickPhysicalDeviceAndQueueFamily() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan-capable physical devices found");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    // Safe mode prefers a software ICD (CPU device type, e.g. Mesa lavapipe or SwiftShader): it
    // does not depend on the vendor GPU driver that is the usual crash source.
    if (m_safeMode) {
        if (SelectPhysicalDevice(devices, true, false)) {
            return;
        }
        std::fprintf(stderr, "IzekipUI: safe mode requested but no software Vulkan device is installed; "
                             "using the hardware device\n");
    }
    if (SelectPhysicalDevice(devices, false, true)) {
        return;
    }

    throw std::runtime_error(
        "No queue family found with both graphics and present support on any physical device "
        "(IzekipUI's Vulkan backend assumes a single combined queue family -- see plan scope cuts)");
}

void VulkanRenderer::CreateLogicalDeviceAndQueue() {
    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueCreateInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkCheck(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device), "vkCreateDevice");
    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_graphicsQueue);
}

void VulkanRenderer::CreateSwapchain(uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

    // Non-sRGB UNORM, deliberately, to match D3D11's non-sRGB R8G8B8A8_UNORM backbuffer --
    // picking an _SRGB surface format here would silently gamma-correct Vulkan's output
    // and break visual parity between the two backends.
    VkSurfaceFormatKHR chosen = formats[0];
    for (const auto& format : formats) {
        if ((format.format == VK_FORMAT_B8G8R8A8_UNORM || format.format == VK_FORMAT_R8G8B8A8_UNORM) &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen = format;
            break;
        }
    }
    m_swapchainFormat = chosen.format;

    if (capabilities.currentExtent.width != UINT32_MAX) {
        m_swapchainExtent = capabilities.currentExtent;
    } else {
        m_swapchainExtent.width = std::clamp(width, capabilities.minImageExtent.width,
                                              capabilities.maxImageExtent.width);
        m_swapchainExtent.height = std::clamp(height, capabilities.minImageExtent.height,
                                               capabilities.maxImageExtent.height);
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = chosen.format;
    createInfo.imageColorSpace = chosen.colorSpace;
    createInfo.imageExtent = m_swapchainExtent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // single combined queue family
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // always available, vsync'd like D3D11's Present(1,0)
    createInfo.clipped = VK_TRUE;

    VkCheck(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain), "vkCreateSwapchainKHR");

    uint32_t actualImageCount = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount, nullptr);
    m_swapchainImages.resize(actualImageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount, m_swapchainImages.data());
}

void VulkanRenderer::CreateSwapchainImageViews() {
    m_swapchainImageViews.resize(m_swapchainImages.size());
    for (size_t i = 0; i < m_swapchainImages.size(); ++i) {
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        VkCheck(vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]),
                "vkCreateImageView");
    }
}

void VulkanRenderer::CreateRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkCheck(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass), "vkCreateRenderPass");
}

void VulkanRenderer::CreateRenderPassResume() {
    // Identical to m_renderPass except loadOp=LOAD (preserve whatever's already in the
    // attachment -- the whole point of resuming) and initialLayout=COLOR_ATTACHMENT_OPTIMAL
    // (DrawBackdropBlurRect's barrier puts the swapchain image back in that layout before
    // calling vkCmdBeginRenderPass with this render pass). Same format/sample-count as
    // m_renderPass, so framebuffers created against m_renderPass remain usable here too --
    // Vulkan render pass *compatibility* (and therefore framebuffer reuse) only depends on
    // attachment format/sample-count/layout-in-reference, never load/store ops.
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    VkCheck(vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPassResume),
            "vkCreateRenderPass(resume)");
}

void VulkanRenderer::CreateFramebuffers() {
    m_swapchainFramebuffers.resize(m_swapchainImageViews.size());
    for (size_t i = 0; i < m_swapchainImageViews.size(); ++i) {
        VkImageView attachments[] = {m_swapchainImageViews[i]};
        VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = m_swapchainExtent.width;
        framebufferInfo.height = m_swapchainExtent.height;
        framebufferInfo.layers = 1;
        VkCheck(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapchainFramebuffers[i]),
                "vkCreateFramebuffer");
    }
}

void VulkanRenderer::CreateDescriptorSetLayoutAndPool() {
    // RectShader.hlsl declares `Texture2DArray fontAtlasArray`/`Texture2D backdropTex` and
    // `SamplerState fontAtlasSampler`/`backdropSampler` as independent
    // HLSL resources (not combined sampler2Ds) -- dxc's default SPIR-V codegen for that
    // pattern is separate descriptors (SAMPLED_IMAGE + SAMPLER) per texture/sampler pair,
    // not COMBINED_IMAGE_SAMPLER. The layout below must match that, binding-for-binding
    // (vk::binding(2,0)/(3,0) -> font array + sampler, (6,0)/(7,0) -> backdrop-blur scratch texture).
    VkDescriptorSetLayoutBinding bindings[6]{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[4].binding = 6;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[4].descriptorCount = 1;
    bindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[5].binding = 7;
    bindings[5].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 6;
    layoutInfo.pBindings = bindings;
    VkCheck(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout),
            "vkCreateDescriptorSetLayout");

    VkDescriptorPoolSize poolSizes[3]{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 2};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 3};
    poolSizes[2] = {VK_DESCRIPTOR_TYPE_SAMPLER, 3};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 3;
    poolInfo.pPoolSizes = poolSizes;
    VkCheck(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool),
            "vkCreateDescriptorPool");

    VkDescriptorSetAllocateInfo allocInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    VkCheck(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet),
            "vkAllocateDescriptorSets");
}

VkShaderModule VulkanRenderer::LoadShaderModule(const char* path) const {
    const std::vector<char> code = ReadBinaryFile(path);
    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    VkCheck(vkCreateShaderModule(m_device, &createInfo, nullptr, &module), "vkCreateShaderModule");
    return module;
}

void VulkanRenderer::CreatePipeline() {
    VkShaderModule vertModule = LoadShaderModule("RectShader.vert.spv");
    VkShaderModule fragModule = LoadShaderModule("RectShader.frag.spv");

    VkPipelineShaderStageCreateInfo vertStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = vertModule;
    vertStage.pName = "VSMain"; // dxc preserves the HLSL entry point name in SPIR-V

    VkPipelineShaderStageCreateInfo fragStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = fragModule;
    fragStage.pName = "PSMain";

    VkPipelineShaderStageCreateInfo stages[] = {vertStage, fragStage};

    VkVertexInputBindingDescription binding{0, sizeof(QuadVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attribute{0, 0, VK_FORMAT_R32G32_SFLOAT, 0};

    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 1;
    vertexInput.pVertexAttributeDescriptions = &attribute;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    // No culling: the negative-viewport-height Y-flip trick (see BeginFrame) inverts
    // effective winding relative to D3D11's default, and there's no correctness reason to
    // cull a flat UI quad -- simpler than re-deriving which winding is "front" post-flip.
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    // Premultiplied-alpha blending: ONE/ONE_MINUS_SRC_ALPHA, the direct Vulkan translation
    // of D3D11Renderer's blend state. The pixel shader composites fill-over-shadow
    // premultiplied internally (see RectShader.hlsl), so the fixed-function blend unit
    // must not multiply the source alpha in again.
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.blendEnable = VK_TRUE;
    blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &blendAttachment;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    VkCheck(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout),
            "vkCreatePipelineLayout");

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;

    VkCheck(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline),
            "vkCreateGraphicsPipelines");

    vkDestroyShaderModule(m_device, vertModule, nullptr);
    vkDestroyShaderModule(m_device, fragModule, nullptr);
}

void VulkanRenderer::CreateCommandPoolAndBuffer() {
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;
    VkCheck(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "vkCreateCommandPool");

    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCheck(vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer),
            "vkAllocateCommandBuffers");
}

void VulkanRenderer::CreateSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkCheck(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore),
            "vkCreateSemaphore(imageAvailable)");

    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // pre-signaled so the first BeginFrame doesn't block
    VkCheck(vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFence), "vkCreateFence");
}

void VulkanRenderer::CreateRenderFinishedSemaphores() {
    m_renderFinishedSemaphores.resize(m_swapchainImages.size());
    VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); ++i) {
        VkCheck(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]),
                "vkCreateSemaphore(renderFinished)");
    }
}

uint32_t VulkanRenderer::FindMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeBits & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("No suitable Vulkan memory type found");
}

void VulkanRenderer::CreateQuadVertexBuffer() {
    // Same unit quad (0,0)-(1,1) as D3D11Renderer -- DrawRect maps it to pixel space via
    // the VS uniform, identically to the D3D11 path.
    const std::array<QuadVertex, 6> vertices = {{
        {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f},
        {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
    }};

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = sizeof(vertices);
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkCheck(vkCreateBuffer(m_device, &bufferInfo, nullptr, &m_quadVertexBuffer), "vkCreateBuffer(quad)");

    VkMemoryRequirements memReq{};
    vkGetBufferMemoryRequirements(m_device, m_quadVertexBuffer, &memReq);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(
        memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkCheck(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_quadVertexMemory),
            "vkAllocateMemory(quad)");
    vkBindBufferMemory(m_device, m_quadVertexBuffer, m_quadVertexMemory, 0);

    void* mapped = nullptr;
    vkMapMemory(m_device, m_quadVertexMemory, 0, sizeof(vertices), 0, &mapped);
    std::memcpy(mapped, vertices.data(), sizeof(vertices));
    vkUnmapMemory(m_device, m_quadVertexMemory);
}

void VulkanRenderer::CreateUniformRingBuffers() {
    m_vsStride = AlignUp(sizeof(VSConstants), m_minUboAlignment);
    m_psStride = AlignUp(sizeof(PSConstants), m_minUboAlignment);

    auto createRingBuffer = [this](VkDeviceSize stride, VkBuffer& buffer, VkDeviceMemory& memory,
                                    void*& mapped) {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = stride * kMaxDrawsPerFrame;
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkCheck(vkCreateBuffer(m_device, &bufferInfo, nullptr, &buffer), "vkCreateBuffer(uniform ring)");

        VkMemoryRequirements memReq{};
        vkGetBufferMemoryRequirements(m_device, buffer, &memReq);

        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex =
            FindMemoryType(memReq.memoryTypeBits,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkCheck(vkAllocateMemory(m_device, &allocInfo, nullptr, &memory),
                "vkAllocateMemory(uniform ring)");
        vkBindBufferMemory(m_device, buffer, memory, 0);

        // Persistently mapped for the renderer's whole lifetime -- HOST_COHERENT means no
        // explicit flush is needed, this is the direct analog of D3D11's cheap per-call
        // Map/Unmap(WRITE_DISCARD).
        vkMapMemory(m_device, memory, 0, bufferInfo.size, 0, &mapped);
    };

    createRingBuffer(m_vsStride, m_vsUniformBuffer, m_vsUniformMemory, m_vsUniformMapped);
    createRingBuffer(m_psStride, m_psUniformBuffer, m_psUniformMemory, m_psUniformMapped);

    VkDescriptorBufferInfo vsBufferInfo{m_vsUniformBuffer, 0, sizeof(VSConstants)};
    VkDescriptorBufferInfo psBufferInfo{m_psUniformBuffer, 0, sizeof(PSConstants)};

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[0].pBufferInfo = &vsBufferInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    writes[1].pBufferInfo = &psBufferInfo;

    vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
}

void VulkanRenderer::CreateFontAtlasTexture() {
    // Every face lives in one 2D array image (one layer per FontId), uploaded through one staging
    // buffer and one synchronous copy. All GlyphAtlases share a size, which is checked here.
    const GlyphAtlas* atlases[kFontCount] = {
        &GlyphAtlas::ForFont(FontId::Proportional), &GlyphAtlas::ForFont(FontId::Monospace),
        &GlyphAtlas::ForFont(FontId::Arial), &GlyphAtlas::ForFont(FontId::Serif),
        &GlyphAtlas::ForFont(FontId::Courier)};
    const uint32_t atlasWidth = atlases[0]->GetAtlasWidth();
    const uint32_t atlasHeight = atlases[0]->GetAtlasHeight();
    for (const GlyphAtlas* atlas : atlases) {
        if (atlas->GetAtlasWidth() != atlasWidth || atlas->GetAtlasHeight() != atlasHeight) {
            throw std::runtime_error("font atlases must share one size for the texture array");
        }
    }
    const VkDeviceSize layerSize = static_cast<VkDeviceSize>(atlasWidth) * atlasHeight;
    const VkDeviceSize totalSize = layerSize * kFontCount;

    // Staging buffer: host-visible, holds every layer's raw bytes back to back.
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    {
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = totalSize;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VkCheck(vkCreateBuffer(m_device, &bufferInfo, nullptr, &stagingBuffer),
                "vkCreateBuffer(fontAtlasStaging)");

        VkMemoryRequirements memReq{};
        vkGetBufferMemoryRequirements(m_device, stagingBuffer, &memReq);
        VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocInfo.allocationSize = memReq.size;
        allocInfo.memoryTypeIndex = FindMemoryType(
            memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkCheck(vkAllocateMemory(m_device, &allocInfo, nullptr, &stagingMemory),
                "vkAllocateMemory(fontAtlasStaging)");
        vkBindBufferMemory(m_device, stagingBuffer, stagingMemory, 0);

        void* mapped = nullptr;
        vkMapMemory(m_device, stagingMemory, 0, totalSize, 0, &mapped);
        for (int layer = 0; layer < kFontCount; ++layer) {
            std::memcpy(static_cast<char*>(mapped) + layer * layerSize, atlases[layer]->GetAtlasPixels().data(),
                        static_cast<size_t>(layerSize));
        }
        vkUnmapMemory(m_device, stagingMemory);
    }

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8_UNORM;
    imageInfo.extent = {atlasWidth, atlasHeight, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = kFontCount;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkCheck(vkCreateImage(m_device, &imageInfo, nullptr, &m_fontAtlasImage), "vkCreateImage(fontAtlasArray)");

    VkMemoryRequirements imageMemReq{};
    vkGetImageMemoryRequirements(m_device, m_fontAtlasImage, &imageMemReq);
    VkMemoryAllocateInfo imageAllocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    imageAllocInfo.allocationSize = imageMemReq.size;
    imageAllocInfo.memoryTypeIndex =
        FindMemoryType(imageMemReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkCheck(vkAllocateMemory(m_device, &imageAllocInfo, nullptr, &m_fontAtlasImageMemory),
            "vkAllocateMemory(fontAtlasArray)");
    vkBindImageMemory(m_device, m_fontAtlasImage, m_fontAtlasImageMemory, 0);

    // One-shot command buffer: upload every layer, synchronous (submit + wait).
    VkCommandBufferAllocateInfo cmdAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAllocInfo.commandPool = m_commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    VkCommandBuffer uploadCmd = VK_NULL_HANDLE;
    VkCheck(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &uploadCmd),
            "vkAllocateCommandBuffers(fontAtlasUpload)");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(uploadCmd, &beginInfo);

    const VkImageSubresourceRange allLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, kFontCount};

    VkImageMemoryBarrier toTransferDst{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransferDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransferDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransferDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransferDst.image = m_fontAtlasImage;
    toTransferDst.subresourceRange = allLayers;
    toTransferDst.srcAccessMask = 0;
    toTransferDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                          nullptr, 0, nullptr, 1, &toTransferDst);

    std::vector<VkBufferImageCopy> copyRegions(kFontCount);
    for (int layer = 0; layer < kFontCount; ++layer) {
        VkBufferImageCopy& region = copyRegions[layer];
        region.bufferOffset = layer * layerSize;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, static_cast<uint32_t>(layer), 1};
        region.imageExtent = {atlasWidth, atlasHeight, 1};
    }
    vkCmdCopyBufferToImage(uploadCmd, stagingBuffer, m_fontAtlasImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            static_cast<uint32_t>(copyRegions.size()), copyRegions.data());

    VkImageMemoryBarrier toShaderRead = toTransferDst;
    toShaderRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toShaderRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(uploadCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                          0, nullptr, 0, nullptr, 1, &toShaderRead);

    vkEndCommandBuffer(uploadCmd);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &uploadCmd;
    VkCheck(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit(fontAtlasUpload)");
    vkQueueWaitIdle(m_graphicsQueue);

    vkFreeCommandBuffers(m_device, m_commandPool, 1, &uploadCmd);
    vkDestroyBuffer(m_device, stagingBuffer, nullptr);
    vkFreeMemory(m_device, stagingMemory, nullptr);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = m_fontAtlasImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = VK_FORMAT_R8_UNORM;
    viewInfo.subresourceRange = allLayers;
    VkCheck(vkCreateImageView(m_device, &viewInfo, nullptr, &m_fontAtlasImageView),
            "vkCreateImageView(fontAtlasArray)");

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0f; // single mip level, no mip range to clamp
    VkCheck(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_fontAtlasSampler),
            "vkCreateSampler(fontAtlasArray)");

    // Static writes for the font array (binding 2, SAMPLED_IMAGE) and its sampler (binding 3).
    // Neither changes again, so they ride along unmodified for the set's lifetime.
    VkDescriptorImageInfo imageDescInfo{};
    imageDescInfo.imageView = m_fontAtlasImageView;
    imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo samplerDescInfo{};
    samplerDescInfo.sampler = m_fontAtlasSampler;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 2;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[0].pImageInfo = &imageDescInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 3;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[1].pImageInfo = &samplerDescInfo;

    vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
}

void VulkanRenderer::CreateBackdropScratchImage() {
    // Viewport-sized, so RecreateSwapchain() calls this again on resize -- tear down the
    // previous version first (no-op on first call, when all three handles are still null).
    if (m_backdropScratchImageView) {
        vkDestroyImageView(m_device, m_backdropScratchImageView, nullptr);
        m_backdropScratchImageView = VK_NULL_HANDLE;
    }
    if (m_backdropScratchImage) {
        vkDestroyImage(m_device, m_backdropScratchImage, nullptr);
        m_backdropScratchImage = VK_NULL_HANDLE;
    }
    if (m_backdropScratchImageMemory) {
        vkFreeMemory(m_device, m_backdropScratchImageMemory, nullptr);
        m_backdropScratchImageMemory = VK_NULL_HANDLE;
    }

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = m_swapchainFormat; // must match the swapchain image vkCmdCopyImage copies from
    // m_swapchainExtent, NOT m_width/m_height: the surface's actual current extent can be
    // clamped by the OS/compositor to something smaller than the requested window size
    // (e.g. a window taller than the screen) -- CreateSwapchain() already accounts for
    // this via capabilities.currentExtent, and vkCmdCopyImage requires its source and
    // destination images to agree on real dimensions, not the originally requested ones.
    imageInfo.extent = {m_swapchainExtent.width, m_swapchainExtent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkCheck(vkCreateImage(m_device, &imageInfo, nullptr, &m_backdropScratchImage),
            "vkCreateImage(backdropScratch)");

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(m_device, m_backdropScratchImage, &memReq);
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkCheck(vkAllocateMemory(m_device, &allocInfo, nullptr, &m_backdropScratchImageMemory),
            "vkAllocateMemory(backdropScratch)");
    vkBindImageMemory(m_device, m_backdropScratchImage, m_backdropScratchImageMemory, 0);

    // One-shot transition UNDEFINED -> GENERAL, kept there permanently: DrawBackdropBlurRect
    // writes to it via vkCmdCopyImage and reads from it in the fragment shader without ever
    // changing its layout, trading a theoretically more optimal TRANSFER_DST/SHADER_READ_ONLY
    // ping-pong for not needing a layout transition around every single draw call.
    VkCommandBufferAllocateInfo cmdAllocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    cmdAllocInfo.commandPool = m_commandPool;
    cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAllocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCheck(vkAllocateCommandBuffers(m_device, &cmdAllocInfo, &cmd),
            "vkAllocateCommandBuffers(backdropScratchInit)");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier toGeneral{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toGeneral.image = m_backdropScratchImage;
    toGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toGeneral.srcAccessMask = 0;
    toGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                          VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                          nullptr, 0, nullptr, 1, &toGeneral);

    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    VkCheck(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE),
            "vkQueueSubmit(backdropScratchInit)");
    vkQueueWaitIdle(m_graphicsQueue);
    vkFreeCommandBuffers(m_device, m_commandPool, 1, &cmd);

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = m_backdropScratchImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_swapchainFormat;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkCheck(vkCreateImageView(m_device, &viewInfo, nullptr, &m_backdropScratchImageView),
            "vkCreateImageView(backdropScratch)");

    if (!m_backdropSampler) {
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 0.0f;
        VkCheck(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_backdropSampler),
                "vkCreateSampler(backdropScratch)");
    }

    // Static write for binding 6 (SAMPLED_IMAGE) / 7 (SAMPLER) -- refreshed every call since,
    // unlike the font atlases, the image view handle genuinely changes after a resize
    // (the old view was just destroyed above and a new one created).
    VkDescriptorImageInfo imageDescInfo{};
    imageDescInfo.imageView = m_backdropScratchImageView;
    imageDescInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkDescriptorImageInfo samplerDescInfo{};
    samplerDescInfo.sampler = m_backdropSampler;

    VkWriteDescriptorSet writes[2]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = m_descriptorSet;
    writes[0].dstBinding = 6;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[0].pImageInfo = &imageDescInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_descriptorSet;
    writes[1].dstBinding = 7;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    writes[1].pImageInfo = &samplerDescInfo;

    vkUpdateDescriptorSets(m_device, 2, writes, 0, nullptr);
}

void VulkanRenderer::DestroySwapchainAndDependents() {
    for (VkSemaphore semaphore : m_renderFinishedSemaphores) {
        vkDestroySemaphore(m_device, semaphore, nullptr);
    }
    m_renderFinishedSemaphores.clear();

    for (VkFramebuffer fb : m_swapchainFramebuffers) {
        vkDestroyFramebuffer(m_device, fb, nullptr);
    }
    m_swapchainFramebuffers.clear();

    for (VkImageView view : m_swapchainImageViews) {
        vkDestroyImageView(m_device, view, nullptr);
    }
    m_swapchainImageViews.clear();
    m_swapchainImages.clear(); // owned by the swapchain, not individually destroyed

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::RecreateSwapchain() {
    vkDeviceWaitIdle(m_device);
    DestroySwapchainAndDependents();
    CreateSwapchain(m_width, m_height);
    CreateSwapchainImageViews();
    CreateFramebuffers();
    CreateRenderFinishedSemaphores(); // image count may have changed
    CreateBackdropScratchImage(); // viewport-sized, must be rebuilt alongside the swapchain
}

void VulkanRenderer::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || (width == m_width && height == m_height)) {
        return;
    }
    m_width = width;
    m_height = height;
    if (m_deviceLost) {
        return; // recovery builds the swapchain at the new size
    }
    try {
        RecreateSwapchain();
    } catch (const std::exception&) {
        MarkDeviceLost();
    }
}

void VulkanRenderer::ReportDrawOverflow(const char* what) {
    if (!m_overflowReported) {
        m_overflowReported = true;
        std::fprintf(stderr, "IzekipUI: draw budget of %u %s exceeded in one frame; the rest is skipped\n",
                     kMaxDrawsPerFrame, what);
    }
}

void VulkanRenderer::BeginFrame() {
    m_overflowReported = false;
    if (m_deviceLost) {
        m_frameSkipped = true;
        return;
    }

    const VkResult waitResult = vkWaitForFences(m_device, 1, &m_inFlightFence, VK_TRUE, UINT64_MAX);
    if (waitResult == VK_ERROR_DEVICE_LOST) {
        MarkDeviceLost();
        return;
    }

    const VkResult acquireResult = vkAcquireNextImageKHR(
        m_device, m_swapchain, UINT64_MAX, m_imageAvailableSemaphore, VK_NULL_HANDLE, &m_currentImageIndex);
    if (acquireResult == VK_ERROR_DEVICE_LOST) {
        MarkDeviceLost();
        return;
    }
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR) {
        try {
            RecreateSwapchain();
        } catch (const std::exception&) {
            MarkDeviceLost();
            return;
        }
        m_frameSkipped = true;
        return; // fence was never reset, so it stays signaled for the next BeginFrame's wait
    }
    m_frameSkipped = false;

    vkResetFences(m_device, 1, &m_inFlightFence);
    vkResetCommandBuffer(m_commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

    VkClearValue clearValue{};
    clearValue.color = {{0.1f, 0.1f, 0.1f, 1.0f}};

    VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_swapchainFramebuffers[m_currentImageIndex];
    renderPassInfo.renderArea.extent = m_swapchainExtent;
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;
    vkCmdBeginRenderPass(m_commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    BindFrameDrawState();

    m_currentDrawSlot = 0;
}

void VulkanRenderer::BindFrameDrawState() {
    // Negative-viewport-height trick: Vulkan's NDC Y points down (opposite of D3D11's
    // Y-up), but the shared shader's VSMain was written for D3D11's convention. Flipping
    // the viewport here is a zero-shader-diff fix (core since Vulkan 1.1 / VK_KHR_maintenance1).
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = static_cast<float>(m_height);
    viewport.width = static_cast<float>(m_width);
    viewport.height = -static_cast<float>(m_height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {m_width, m_height}};
    vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkBuffer vertexBuffers[] = {m_quadVertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, vertexBuffers, offsets);
}

void VulkanRenderer::EndFrame() {
    if (m_frameSkipped) {
        return;
    }
    vkCmdEndRenderPass(m_commandBuffer);
    vkEndCommandBuffer(m_commandBuffer);
}

void VulkanRenderer::Present() {
    if (m_deviceLost) {
        TryRecoverDevice();
        return;
    }
    if (m_frameSkipped) {
        return;
    }

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentImageIndex]};

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;
    const VkResult submitResult = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFence);
    if (submitResult == VK_ERROR_DEVICE_LOST) {
        MarkDeviceLost();
        TryRecoverDevice();
        return;
    }
    VkCheck(submitResult, "vkQueueSubmit");

    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &m_currentImageIndex;
    const VkResult presentResult = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);

    if (presentResult == VK_ERROR_DEVICE_LOST) {
        MarkDeviceLost();
        TryRecoverDevice();
        return;
    }
    if (presentResult == VK_SUCCESS) {
        Platform::SafeMode::MarkHealthy(m_appId); // first presented frame ends the crash-loop count
    }
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        try {
            RecreateSwapchain();
        } catch (const std::exception&) {
            MarkDeviceLost();
            TryRecoverDevice();
        }
    }
}

void VulkanRenderer::MarkDeviceLost() {
    m_deviceLost = true;
    m_frameSkipped = true;
}

void VulkanRenderer::TryRecoverDevice() {
    Shutdown();
    try {
        CreateDeviceResources();
        m_deviceLost = false;
        m_frameSkipped = false;
    } catch (const std::exception&) {
        // Driver still resetting or device not yet available: stay lost, skip drawing, and
        // retry on the next Present(). Throwing here would crash the host application.
        Shutdown();
    }
}

void VulkanRenderer::DrawRect(const Rect& rect, const RectPaint& paint) {
    if (m_frameSkipped) {
        return;
    }
    if (m_currentDrawSlot >= kMaxDrawsPerFrame) {
        ReportDrawOverflow("rects");
        return;
    }

    const RectDrawData data =
        ComputeRectDrawData(rect, paint, static_cast<float>(m_width), static_cast<float>(m_height),
                            m_worldTransform);

    const VkDeviceSize vsOffset = m_currentDrawSlot * m_vsStride;
    const VkDeviceSize psOffset = m_currentDrawSlot * m_psStride;
    std::memcpy(static_cast<char*>(m_vsUniformMapped) + vsOffset, &data.vs, sizeof(data.vs));
    std::memcpy(static_cast<char*>(m_psUniformMapped) + psOffset, &data.ps, sizeof(data.ps));

    const uint32_t dynamicOffsets[] = {static_cast<uint32_t>(vsOffset), static_cast<uint32_t>(psOffset)};
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                             &m_descriptorSet, 2, dynamicOffsets);

    vkCmdDraw(m_commandBuffer, 6, 1, 0, 0);

    ++m_currentDrawSlot;
}

void VulkanRenderer::DrawTextQuad(const Rect& rect, const UVRect& atlasUV, const Color& color,
                                  float glyphScale, FontId fontId) {
    if (m_frameSkipped) {
        return;
    }
    if (m_currentDrawSlot >= kMaxDrawsPerFrame) {
        ReportDrawOverflow("glyphs");
        return;
    }

    const RectDrawData data = ComputeTextQuadDrawData(rect, atlasUV, color, glyphScale, fontId,
                                                       static_cast<float>(m_width),
                                                       static_cast<float>(m_height), m_worldTransform);

    const VkDeviceSize vsOffset = m_currentDrawSlot * m_vsStride;
    const VkDeviceSize psOffset = m_currentDrawSlot * m_psStride;
    std::memcpy(static_cast<char*>(m_vsUniformMapped) + vsOffset, &data.vs, sizeof(data.vs));
    std::memcpy(static_cast<char*>(m_psUniformMapped) + psOffset, &data.ps, sizeof(data.ps));

    const uint32_t dynamicOffsets[] = {static_cast<uint32_t>(vsOffset), static_cast<uint32_t>(psOffset)};
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                             &m_descriptorSet, 2, dynamicOffsets);

    vkCmdDraw(m_commandBuffer, 6, 1, 0, 0);

    ++m_currentDrawSlot;
}

void VulkanRenderer::DrawBackdropBlurRect(const Rect& rect, const BackdropBlurPaint& paint) {
    if (m_frameSkipped) {
        return;
    }
    if (m_currentDrawSlot >= kMaxDrawsPerFrame) {
        ReportDrawOverflow("backdrop-blur rects");
        return;
    }

    // Copy region: the element's own bounds padded by the (unclamped) blur radius, so the
    // shader's convolution (clamped to kMaxBackdropBlurRadiusPx in ComputeBackdropBlurDrawData)
    // always finds real previously-drawn pixels within the copied region instead of
    // whatever garbage sits outside it. Clamped to the viewport's own bounds since
    // vkCmdCopyImage's region must stay inside both images.
    const float pad = paint.filter.blurRadiusPx;
    const int32_t left = static_cast<int32_t>(std::max(0.0f, rect.x - pad));
    const int32_t top = static_cast<int32_t>(std::max(0.0f, rect.y - pad));
    // Clamped against the swapchain's real extent (see CreateBackdropScratchImage's doc
    // comment) -- m_width/m_height can be larger than what the surface actually allocated.
    const int32_t right = static_cast<int32_t>(
        std::min(static_cast<float>(m_swapchainExtent.width), rect.x + rect.width + pad));
    const int32_t bottom = static_cast<int32_t>(
        std::min(static_cast<float>(m_swapchainExtent.height), rect.y + rect.height + pad));

    if (right > left && bottom > top) {
        // vkCmdCopyImage cannot run inside an active render pass instance (core Vulkan
        // spec constraint) -- end it, do the copy, then resume with m_renderPassResume
        // (loadOp=LOAD, so the content drawn so far is preserved, not cleared).
        vkCmdEndRenderPass(m_commandBuffer);

        VkImageMemoryBarrier swapchainToTransferSrc{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        swapchainToTransferSrc.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // m_renderPass's finalLayout
        swapchainToTransferSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        swapchainToTransferSrc.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchainToTransferSrc.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        swapchainToTransferSrc.image = m_swapchainImages[m_currentImageIndex];
        swapchainToTransferSrc.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        swapchainToTransferSrc.srcAccessMask = 0;
        swapchainToTransferSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                              &swapchainToTransferSrc);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.srcOffset = {left, top, 0};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstOffset = {left, top, 0}; // same x,y as source -- keeps screenUV mapping simple
        copyRegion.extent = {static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top), 1};
        vkCmdCopyImage(m_commandBuffer, m_swapchainImages[m_currentImageIndex],
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_backdropScratchImage,
                        VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);

        // Two barriers: swapchain image back to COLOR_ATTACHMENT_OPTIMAL (m_renderPassResume's
        // initialLayout expects this), and the scratch image's write synchronized before the
        // fragment shader reads it (layout stays GENERAL throughout -- purely a memory barrier).
        VkImageMemoryBarrier swapchainToColorAttachment = swapchainToTransferSrc;
        swapchainToColorAttachment.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        swapchainToColorAttachment.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        swapchainToColorAttachment.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        swapchainToColorAttachment.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkImageMemoryBarrier scratchWriteToRead{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        scratchWriteToRead.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        scratchWriteToRead.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        scratchWriteToRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        scratchWriteToRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        scratchWriteToRead.image = m_backdropScratchImage;
        scratchWriteToRead.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        scratchWriteToRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        scratchWriteToRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkImageMemoryBarrier barriers[] = {swapchainToColorAttachment, scratchWriteToRead};
        vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                  VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                              0, 0, nullptr, 0, nullptr, 2, barriers);

        VkRenderPassBeginInfo resumeInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        resumeInfo.renderPass = m_renderPassResume;
        resumeInfo.framebuffer = m_swapchainFramebuffers[m_currentImageIndex];
        resumeInfo.renderArea.extent = m_swapchainExtent;
        resumeInfo.clearValueCount = 0; // loadOp=LOAD, nothing to clear
        vkCmdBeginRenderPass(m_commandBuffer, &resumeInfo, VK_SUBPASS_CONTENTS_INLINE);

        BindFrameDrawState(); // pipeline/vertex-buffer/viewport/scissor don't survive the split
    }

    const RectDrawData data = ComputeBackdropBlurDrawData(rect, paint, static_cast<float>(m_width),
                                                            static_cast<float>(m_height),
                                                            m_worldTransform);

    const VkDeviceSize vsOffset = m_currentDrawSlot * m_vsStride;
    const VkDeviceSize psOffset = m_currentDrawSlot * m_psStride;
    std::memcpy(static_cast<char*>(m_vsUniformMapped) + vsOffset, &data.vs, sizeof(data.vs));
    std::memcpy(static_cast<char*>(m_psUniformMapped) + psOffset, &data.ps, sizeof(data.ps));

    const uint32_t dynamicOffsets[] = {static_cast<uint32_t>(vsOffset), static_cast<uint32_t>(psOffset)};
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                             &m_descriptorSet, 2, dynamicOffsets);

    vkCmdDraw(m_commandBuffer, 6, 1, 0, 0);

    ++m_currentDrawSlot;
}

void VulkanRenderer::Shutdown() {
    if (m_device == VK_NULL_HANDLE) {
        DestroyInstanceAndSurface(); // a recovery attempt may have failed before the device existed
        return;
    }
    vkDeviceWaitIdle(m_device);

    // Every handle is reset to VK_NULL_HANDLE after destruction: recovery rebuilds the device and
    // several Create* paths test "already created?" by null-checking these members.
    if (m_fontAtlasImageView) vkDestroyImageView(m_device, m_fontAtlasImageView, nullptr);
    if (m_fontAtlasImage) vkDestroyImage(m_device, m_fontAtlasImage, nullptr);
    if (m_fontAtlasImageMemory) vkFreeMemory(m_device, m_fontAtlasImageMemory, nullptr);
    if (m_fontAtlasSampler) vkDestroySampler(m_device, m_fontAtlasSampler, nullptr);
    m_fontAtlasImageView = VK_NULL_HANDLE;
    m_fontAtlasImage = VK_NULL_HANDLE;
    m_fontAtlasImageMemory = VK_NULL_HANDLE;
    m_fontAtlasSampler = VK_NULL_HANDLE;

    if (m_backdropScratchImageView) vkDestroyImageView(m_device, m_backdropScratchImageView, nullptr);
    if (m_backdropScratchImage) vkDestroyImage(m_device, m_backdropScratchImage, nullptr);
    if (m_backdropScratchImageMemory) vkFreeMemory(m_device, m_backdropScratchImageMemory, nullptr);
    if (m_backdropSampler) vkDestroySampler(m_device, m_backdropSampler, nullptr);
    m_backdropScratchImageView = VK_NULL_HANDLE;
    m_backdropScratchImage = VK_NULL_HANDLE;
    m_backdropScratchImageMemory = VK_NULL_HANDLE;
    m_backdropSampler = VK_NULL_HANDLE;

    if (m_quadVertexBuffer) vkDestroyBuffer(m_device, m_quadVertexBuffer, nullptr);
    if (m_quadVertexMemory) vkFreeMemory(m_device, m_quadVertexMemory, nullptr);
    if (m_vsUniformBuffer) vkDestroyBuffer(m_device, m_vsUniformBuffer, nullptr);
    if (m_vsUniformMemory) vkFreeMemory(m_device, m_vsUniformMemory, nullptr);
    if (m_psUniformBuffer) vkDestroyBuffer(m_device, m_psUniformBuffer, nullptr);
    if (m_psUniformMemory) vkFreeMemory(m_device, m_psUniformMemory, nullptr);
    m_quadVertexBuffer = VK_NULL_HANDLE;
    m_quadVertexMemory = VK_NULL_HANDLE;
    m_vsUniformBuffer = VK_NULL_HANDLE;
    m_vsUniformMemory = VK_NULL_HANDLE;
    m_psUniformBuffer = VK_NULL_HANDLE;
    m_psUniformMemory = VK_NULL_HANDLE;

    if (m_descriptorPool) vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    if (m_pipeline) vkDestroyPipeline(m_device, m_pipeline, nullptr);
    if (m_pipelineLayout) vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    if (m_descriptorSetLayout) vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
    if (m_renderPass) vkDestroyRenderPass(m_device, m_renderPass, nullptr);
    if (m_renderPassResume) vkDestroyRenderPass(m_device, m_renderPassResume, nullptr);
    m_descriptorPool = VK_NULL_HANDLE;
    m_pipeline = VK_NULL_HANDLE;
    m_pipelineLayout = VK_NULL_HANDLE;
    m_descriptorSetLayout = VK_NULL_HANDLE;
    m_renderPass = VK_NULL_HANDLE;
    m_renderPassResume = VK_NULL_HANDLE;

    DestroySwapchainAndDependents(); // also destroys m_renderFinishedSemaphores

    if (m_imageAvailableSemaphore) vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
    if (m_inFlightFence) vkDestroyFence(m_device, m_inFlightFence, nullptr);
    if (m_commandPool) vkDestroyCommandPool(m_device, m_commandPool, nullptr);
    m_imageAvailableSemaphore = VK_NULL_HANDLE;
    m_inFlightFence = VK_NULL_HANDLE;
    m_commandPool = VK_NULL_HANDLE;
    m_commandBuffer = VK_NULL_HANDLE;
    m_vsUniformMapped = nullptr;
    m_psUniformMapped = nullptr;

    vkDestroyDevice(m_device, nullptr);
    m_device = VK_NULL_HANDLE;

    DestroyInstanceAndSurface();
}

void VulkanRenderer::DestroyInstanceAndSurface() {
    if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    m_surface = VK_NULL_HANDLE;

    if (m_debugMessenger && m_instance) {
        auto destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyFn) {
            destroyFn(m_instance, m_debugMessenger, nullptr);
        }
    }
    m_debugMessenger = VK_NULL_HANDLE;

    if (m_instance) vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
}

void VulkanRenderer::DrawImage(const std::string& imagePath, const Rect& rect, float opacity) {
    // TODO: Load PNG from imagePath, create Vulkan image + texture, draw as quad.
    // For now, silent no-op to unblock Image widget integration.
    (void)imagePath;
    (void)rect;
    (void)opacity;
}

} // namespace IzekipUI::Render
