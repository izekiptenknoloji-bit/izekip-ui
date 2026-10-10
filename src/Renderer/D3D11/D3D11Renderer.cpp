#include "D3D11Renderer.h"

#include <d3dcompiler.h>

#include <array>
#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

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
    std::ifstream file("RectShader.hlsl");
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
    swapChainDesc.OutputWindow = static_cast<HWND>(info.nativeWindowHandle);
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT deviceFlags = 0;
#ifdef _DEBUG
    deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    ThrowIfFailed(
        D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, &featureLevel, 1,
            D3D11_SDK_VERSION, &swapChainDesc, m_swapChain.GetAddressOf(), m_device.GetAddressOf(),
            nullptr, m_context.GetAddressOf()),
        "D3D11CreateDeviceAndSwapChain failed");

    CreateRenderTarget();
    CreateQuadGeometry();
    CreateShaders();
    CreateBlendState();

    return true;
}

void D3D11Renderer::Shutdown() {
    ReleaseRenderTarget();
    m_quadVertexBuffer.Reset();
    m_vsConstantBuffer.Reset();
    m_psConstantBuffer.Reset();
    m_blendState.Reset();
    m_inputLayout.Reset();
    m_vertexShader.Reset();
    m_pixelShader.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

void D3D11Renderer::CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    ThrowIfFailed(m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                          reinterpret_cast<void**>(backBuffer.GetAddressOf())),
                  "GetBuffer failed");
    ThrowIfFailed(
        m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, m_renderTargetView.GetAddressOf()),
        "CreateRenderTargetView failed");
}

void D3D11Renderer::ReleaseRenderTarget() {
    m_renderTargetView.Reset();
}

void D3D11Renderer::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || (width == m_width && height == m_height)) {
        return;
    }
    m_width = width;
    m_height = height;

    ReleaseRenderTarget();
    ThrowIfFailed(m_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0),
                  "ResizeBuffers failed");
    CreateRenderTarget();
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

void D3D11Renderer::BeginFrame() {
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
}

void D3D11Renderer::EndFrame() {
    // Nothing to flush explicitly -- Present() does it.
}

void D3D11Renderer::Present() {
    m_swapChain->Present(1, 0);
}

void D3D11Renderer::DrawRect(const Rect& rect, const RectPaint& paint) {
    const RectDrawData data =
        ComputeRectDrawData(rect, paint, static_cast<float>(m_width), static_cast<float>(m_height));

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    m_context->Map(m_vsConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    std::memcpy(mapped.pData, &data.vs, sizeof(data.vs));
    m_context->Unmap(m_vsConstantBuffer.Get(), 0);

    m_context->Map(m_psConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    std::memcpy(mapped.pData, &data.ps, sizeof(data.ps));
    m_context->Unmap(m_psConstantBuffer.Get(), 0);

    m_context->Draw(6, 0);
}

} // namespace IzekipUI::Render
