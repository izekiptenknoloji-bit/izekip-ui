#include "D3D11Renderer.h"

#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "Renderer/GlyphAtlas.h"
#include "Core/AssetPath.h"
#include "Platform/SafeMode.h"
#include <cstdio>
#include "Renderer/RectDrawData.h"

using Microsoft::WRL::ComPtr;
using namespace IzekipUI::Render::detail;

namespace IzekipUI::Render {

namespace {

void ThrowIfFailed(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        throw std::runtime_error(what);
    }
}

// The shared SDF shader (src/Renderer/Shaders/RectShader.hlsl) is copied next to the
// executable at build time (same copy_if_different mechanism as style.css) and compiled
// here at runtime via D3DCompile -- unchanged from before this file was extracted to a
// shared .hlsl, just reading from disk instead of an embedded string literal.
std::string ReadShaderSource() {
    std::ifstream file(ResolveAssetPath("RectShader.hlsl"));
    if (!file) {
        throw std::runtime_error("Could not open RectShader.hlsl next to the executable");
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace

bool D3D11Renderer::Initialize(const RendererCreateInfo& info) {
    m_width = info.initialWidth;
    m_height = info.initialHeight;
    m_hwnd = static_cast<HWND>(info.nativeWindowHandle);
    m_appId = ExecutablePath().stem().string();
    Platform::SafeMode::BeginLaunch(m_appId);
    CreateDeviceResources();
    return true;
}

void D3D11Renderer::CreateDeviceResources() {
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferDesc.Width = m_width;
    swapChainDesc.BufferDesc.Height = m_height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.OutputWindow = m_hwnd;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT deviceFlags = 0;
#ifdef _DEBUG
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    // Safe mode (and any hardware failure) uses WARP, the software rasterizer that ships with
    // Windows, so the application still starts when the GPU driver is broken.
    auto createWith = [&](D3D_DRIVER_TYPE driverType) {
        return D3D11CreateDeviceAndSwapChain(
            nullptr, driverType, nullptr, deviceFlags, &featureLevel, 1, D3D11_SDK_VERSION,
            &swapChainDesc, m_swapChain.GetAddressOf(), m_device.GetAddressOf(), nullptr,
            m_context.GetAddressOf());
    };
    const bool safeMode = Platform::SafeMode::IsActive();
    if (safeMode) {
        std::fprintf(stderr, "IzekipUI: safe mode -- software rendering (WARP) after repeated failed launches\n");
    }
    HRESULT hr = createWith(safeMode ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_HARDWARE);
    if (FAILED(hr) && !safeMode) {
        std::fprintf(stderr, "IzekipUI: hardware D3D11 device unavailable (0x%08lx); using WARP\n",
                     static_cast<unsigned long>(hr));
        hr = createWith(D3D_DRIVER_TYPE_WARP);
    }
    ThrowIfFailed(hr, "D3D11CreateDeviceAndSwapChain failed");

    CreateRenderTarget();
    CreateQuadGeometry();
    CreateShaders();
    CreateBlendState();
    CreateFontAtlasTexture();
    CreateBackdropScratchTexture();
}

void D3D11Renderer::TryRecoverDevice() {
    Shutdown();
    try {
        CreateDeviceResources();
        m_deviceLost = false;
    } catch (const std::exception&) {
        // Driver still resetting or no adapter yet: stay in the lost state, skip drawing,
        // and retry on the next Present(). Throwing here would crash the host application.
        Shutdown();
    }
}

void D3D11Renderer::Shutdown() {
    ReleaseRenderTarget();
    m_quadVertexBuffer.Reset();
    m_vsConstantBuffer.Reset();
    m_psConstantBuffer.Reset();
    m_blendState.Reset();
    m_fontAtlasTexture.Reset();
    m_fontAtlasSRV.Reset();
    m_fontAtlasSampler.Reset();
    m_backdropScratchTexture.Reset();
    m_backdropScratchSRV.Reset();
    m_backdropSampler.Reset();
    m_inputLayout.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

void D3D11Renderer::CreateRenderTarget() {
    ThrowIfFailed(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                          reinterpret_cast<void**>(m_backBufferTexture.GetAddressOf())),
                  "GetBuffer failed");
    ThrowIfFailed(m_device->CreateRenderTargetView(m_backBufferTexture.Get(), nullptr,
                                                     m_renderTargetView.GetAddressOf()),
                  "CreateRenderTargetView failed");
}

void D3D11Renderer::ReleaseRenderTarget() {
    m_renderTargetView.Reset();
    m_backBufferTexture.Reset();
}

void D3D11Renderer::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || (width == m_width && height == m_height)) {
        return;
    }
    m_width = width;
    m_height = height;
    if (m_deviceLost) {
        return; // recovery rebuilds the swapchain at the new size
    }

    ReleaseRenderTarget();
    const HRESULT hr = m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        m_deviceLost = true;
        return;
    }
    try {
        CreateRenderTarget();
        CreateBackdropScratchTexture(); // viewport-sized, must be rebuilt alongside the render target
    } catch (const std::exception&) {
        m_deviceLost = true;
    }
}

void D3D11Renderer::CreateQuadGeometry() {
    // Unit quad in local 0..1 space; DrawRect maps it to pixel space via the VS constant buffer.
    const std::array<QuadVertex, 6> vertices = {{
        {0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 1.0f},
        {0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 1.0f},
    }};

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.ByteWidth = sizeof(vertices);
    bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices.data();

    ThrowIfFailed(m_device->CreateBuffer(&bufferDesc, &initData, m_quadVertexBuffer.GetAddressOf()),
                  "CreateBuffer(quad) failed");

    D3D11_BUFFER_DESC cbDesc = {};
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    cbDesc.ByteWidth = sizeof(VSConstants);
    ThrowIfFailed(m_device->CreateBuffer(&cbDesc, nullptr, m_vsConstantBuffer.GetAddressOf()),
                  "CreateBuffer(vsConstants) failed");

    cbDesc.ByteWidth = sizeof(PSConstants);
    ThrowIfFailed(m_device->CreateBuffer(&cbDesc, nullptr, m_psConstantBuffer.GetAddressOf()),
                  "CreateBuffer(psConstants) failed");
}

void D3D11Renderer::CreateShaders() {
    const std::string shaderSource = ReadShaderSource();

    UINT compileFlags = 0;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG;
#endif

    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

    HRESULT hr = D3DCompile(shaderSource.data(), shaderSource.size(), "IzekipUIQuadShader", nullptr,
                             nullptr, "VSMain", "vs_4_0", compileFlags, 0, vsBlob.GetAddressOf(),
                             errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        std::string message = "VSMain compile failed";
        if (errorBlob) {
            message += ": ";
            message += static_cast<const char*>(errorBlob->GetBufferPointer());
        }
        throw std::runtime_error(message);
    }

    hr = D3DCompile(shaderSource.data(), shaderSource.size(), "IzekipUIQuadShader", nullptr, nullptr,
                     "PSMain", "ps_4_0", compileFlags, 0, psBlob.GetAddressOf(),
                     errorBlob.GetAddressOf());
    if (FAILED(hr)) {
        std::string message = "PSMain compile failed";
        if (errorBlob) {
            message += ": ";
            message += static_cast<const char*>(errorBlob->GetBufferPointer());
        }
        throw std::runtime_error(message);
    }

    ThrowIfFailed(m_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
                                                nullptr, m_vertexShader.GetAddressOf()),
                  "CreateVertexShader failed");
    ThrowIfFailed(m_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
                                               nullptr, m_pixelShader.GetAddressOf()),
                  "CreatePixelShader failed");

    const D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    ThrowIfFailed(m_device->CreateInputLayout(layoutDesc, 1, vsBlob->GetBufferPointer(),
                                               vsBlob->GetBufferSize(), m_inputLayout.GetAddressOf()),
                  "CreateInputLayout failed");
}

void D3D11Renderer::CreateBlendState() {
    // Premultiplied-alpha blending: ONE/INV_SRC_ALPHA, not SRC_ALPHA/INV_SRC_ALPHA. The
    // pixel shader already composites fill-over-shadow as premultiplied internally, so
    // the hardware blend unit must not multiply the source alpha in again.
    D3D11_BLEND_DESC blendDesc = {};
    D3D11_RENDER_TARGET_BLEND_DESC& rt = blendDesc.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_ONE;
    rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;
    rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    ThrowIfFailed(m_device->CreateBlendState(&blendDesc, m_blendState.GetAddressOf()),
                  "CreateBlendState failed");
}

namespace {
// All font faces live in one Texture2DArray, one slice per FontId. Every GlyphAtlas is built at
// the same size (kAtlasWidth x kAtlasHeight), so a single texture description covers them all.
void CreateFontAtlasArray(ID3D11Device* device, Microsoft::WRL::ComPtr<ID3D11Texture2D>& outTexture,
                           Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& outSRV,
                           Microsoft::WRL::ComPtr<ID3D11SamplerState>& outSampler) {
    const GlyphAtlas* atlases[kFontCount] = {
        &GlyphAtlas::ForFont(FontId::Proportional), &GlyphAtlas::ForFont(FontId::Monospace),
        &GlyphAtlas::ForFont(FontId::Arial), &GlyphAtlas::ForFont(FontId::Serif),
        &GlyphAtlas::ForFont(FontId::Courier)};
    const uint32_t width = atlases[0]->GetAtlasWidth();
    const uint32_t height = atlases[0]->GetAtlasHeight();
    for (const GlyphAtlas* atlas : atlases) {
        if (atlas->GetAtlasWidth() != width || atlas->GetAtlasHeight() != height) {
            throw std::runtime_error("font atlases must share one size for the texture array");
        }
    }

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = kFontCount;
    texDesc.Format = DXGI_FORMAT_R8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData[kFontCount] = {};
    for (int slice = 0; slice < kFontCount; ++slice) {
        initData[slice].pSysMem = atlases[slice]->GetAtlasPixels().data();
        initData[slice].SysMemPitch = width;
    }

    ThrowIfFailed(device->CreateTexture2D(&texDesc, initData, outTexture.GetAddressOf()),
                  "CreateTexture2D(fontAtlasArray) failed");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = kFontCount;
    ThrowIfFailed(device->CreateShaderResourceView(outTexture.Get(), &srvDesc, outSRV.GetAddressOf()),
                  "CreateShaderResourceView(fontAtlasArray) failed");

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    ThrowIfFailed(device->CreateSamplerState(&samplerDesc, outSampler.GetAddressOf()),
                  "CreateSamplerState(fontAtlasArray) failed");
}
} // namespace

void D3D11Renderer::CreateFontAtlasTexture() {
    CreateFontAtlasArray(m_device.Get(), m_fontAtlasTexture, m_fontAtlasSRV, m_fontAtlasSampler);
}

void D3D11Renderer::CreateBackdropScratchTexture() {
    m_backdropScratchSRV.Reset();
    m_backdropScratchTexture.Reset();

    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = m_width;
    texDesc.Height = m_height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // must match the backbuffer: CopySubresourceRegion requires identical formats
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT; // written via CopySubresourceRegion, not CPU-mapped
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ThrowIfFailed(m_device->CreateTexture2D(&texDesc, nullptr, m_backdropScratchTexture.GetAddressOf()),
                  "CreateTexture2D(backdropScratch) failed");
    ThrowIfFailed(m_device->CreateShaderResourceView(m_backdropScratchTexture.Get(), nullptr,
                                                       m_backdropScratchSRV.GetAddressOf()),
                  "CreateShaderResourceView(backdropScratch) failed");

    if (!m_backdropSampler) {
        D3D11_SAMPLER_DESC samplerDesc = {};
        samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
        ThrowIfFailed(m_device->CreateSamplerState(&samplerDesc, m_backdropSampler.GetAddressOf()),
                      "CreateSamplerState(backdropScratch) failed");
    }
}

void D3D11Renderer::BeginFrame() {
    if (m_deviceLost) {
        return;
    }
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MaxDepth = 1.0f;
    m_context->RSSetViewports(1, &viewport);

    ID3D11RenderTargetView* rtv = m_renderTargetView.Get();
    m_context->OMSetRenderTargets(1, &rtv, nullptr);

    const float clearColor[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    m_context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    m_context->OMSetBlendState(m_blendState.Get(), nullptr, 0xFFFFFFFF);

    m_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_context->IASetInputLayout(m_inputLayout.Get());

    const UINT stride = sizeof(QuadVertex);
    const UINT offset = 0;
    ID3D11Buffer* vertexBuffer = m_quadVertexBuffer.Get();
    m_context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

    m_context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    m_context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    ID3D11Buffer* vsCb = m_vsConstantBuffer.Get();
    m_context->VSSetConstantBuffers(0, 1, &vsCb);
    ID3D11Buffer* psCb = m_psConstantBuffer.Get();
    m_context->PSSetConstantBuffers(0, 1, &psCb);

    // Register slots match RectShader.hlsl: font array t0/s0, backdrop t2/s2.
    ID3D11ShaderResourceView* fontSrv = m_fontAtlasSRV.Get();
    m_context->PSSetShaderResources(0, 1, &fontSrv);
    ID3D11ShaderResourceView* backdropSrv = m_backdropScratchSRV.Get();
    m_context->PSSetShaderResources(2, 1, &backdropSrv);
    ID3D11SamplerState* fontSampler = m_fontAtlasSampler.Get();
    m_context->PSSetSamplers(0, 1, &fontSampler);
    ID3D11SamplerState* backdropSamplerState = m_backdropSampler.Get();
    m_context->PSSetSamplers(2, 1, &backdropSamplerState);
}

void D3D11Renderer::EndFrame() {
    // Nothing to flush explicitly -- Present() does it.
}

void D3D11Renderer::Present() {
    if (m_deviceLost) {
        TryRecoverDevice();
        return;
    }
    const HRESULT hr = m_swapChain->Present(1, 0);
    if (SUCCEEDED(hr)) {
        Platform::SafeMode::MarkHealthy(m_appId); // first presented frame ends the crash-loop count
    } else if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
        m_deviceLost = true;
        TryRecoverDevice();
    }
}

bool D3D11Renderer::UploadConstants(const detail::RectDrawData& data) {
    // A failed Map leaves pData null; writing through it would crash. The draw is skipped, and a
    // removed device is recorded so Present() rebuilds it.
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(m_context->Map(m_vsConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        NoteConstantUploadFailure();
        return false;
    }
    std::memcpy(mapped.pData, &data.vs, sizeof(data.vs));
    m_context->Unmap(m_vsConstantBuffer.Get(), 0);

    if (FAILED(m_context->Map(m_psConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        NoteConstantUploadFailure();
        return false;
    }
    std::memcpy(mapped.pData, &data.ps, sizeof(data.ps));
    m_context->Unmap(m_psConstantBuffer.Get(), 0);
    return true;
}

void D3D11Renderer::NoteConstantUploadFailure() {
    if (m_device && m_device->GetDeviceRemovedReason() != S_OK) {
        m_deviceLost = true;
    }
}

void D3D11Renderer::DrawRect(const Rect& rect, const RectPaint& paint) {
    if (m_deviceLost) {
        return;
    }
    const RectDrawData data =
        ComputeRectDrawData(rect, paint, static_cast<float>(m_width), static_cast<float>(m_height),
                            m_worldTransform);

    if (!UploadConstants(data)) {
        return;
    }

    m_context->Draw(6, 0);
}

void D3D11Renderer::DrawTextQuad(const Rect& rect, const UVRect& atlasUV, const Color& color,
                                 float glyphScale, FontId fontId) {
    if (m_deviceLost) {
        return;
    }
    const RectDrawData data = ComputeTextQuadDrawData(rect, atlasUV, color, glyphScale, fontId,
                                                       static_cast<float>(m_width),
                                                       static_cast<float>(m_height), m_worldTransform);

    if (!UploadConstants(data)) {
        return;
    }

    m_context->Draw(6, 0);
}

void D3D11Renderer::DrawBackdropBlurRect(const Rect& rect, const BackdropBlurPaint& paint) {
    if (m_deviceLost) {
        return;
    }
    // Copy region: the element's own bounds padded by the (unclamped) blur radius, so the
    // shader's convolution -- clamped to kMaxBackdropBlurRadiusPx in ComputeBackdropBlurDrawData --
    // always finds real previously-drawn pixels within the copied region, never garbage
    // from outside it. Clamped to the viewport's own bounds since CopySubresourceRegion's
    // source box must stay inside the source texture.
    const float pad = paint.filter.blurRadiusPx;
    const LONG left = static_cast<LONG>(std::max(0.0f, rect.x - pad));
    const LONG top = static_cast<LONG>(std::max(0.0f, rect.y - pad));
    const LONG right = static_cast<LONG>(std::min(static_cast<float>(m_width), rect.x + rect.width + pad));
    const LONG bottom =
        static_cast<LONG>(std::min(static_cast<float>(m_height), rect.y + rect.height + pad));

    if (right > left && bottom > top) {
        D3D11_BOX srcBox{static_cast<UINT>(left), static_cast<UINT>(top), 0,
                          static_cast<UINT>(right), static_cast<UINT>(bottom), 1};
        m_context->CopySubresourceRegion(m_backdropScratchTexture.Get(), 0, static_cast<UINT>(left),
                                          static_cast<UINT>(top), 0, m_backBufferTexture.Get(), 0,
                                          &srcBox);
    }

    const RectDrawData data = ComputeBackdropBlurDrawData(rect, paint, static_cast<float>(m_width),
                                                            static_cast<float>(m_height),
                                                            m_worldTransform);

    if (!UploadConstants(data)) {
        return;
    }

    m_context->Draw(6, 0);
}

void D3D11Renderer::DrawImage(const std::string& imagePath, const Rect& rect, float opacity) {
    // TODO: Load PNG from imagePath, create D3D11 texture, draw as quad.
    // For now, silent no-op to unblock Image widget integration.
    (void)imagePath;
    (void)rect;
    (void)opacity;
}

} // namespace IzekipUI::Render
