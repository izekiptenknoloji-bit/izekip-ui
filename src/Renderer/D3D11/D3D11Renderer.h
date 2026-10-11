#pragma once

#ifndef NOMINMAX
#define NOMINMAX // avoid windows.h's min/max macros clobbering std::min/std::max
#endif

#include <wrl/client.h>

#include <d3d11.h>

#include "Renderer/IRenderer.h"

namespace IzekipUI::Render {

// D3D11 backend. Draws flat/gradient-filled, optionally rounded and drop-shadowed
// rectangles via a signed-distance-field pixel shader -- a single reusable unit-quad
// vertex buffer (expanded per-draw to fit any shadow halo) and one embedded shader pair.
class D3D11Renderer : public IRenderer {
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

    RendererBackend GetBackend() const override { return RendererBackend::D3D11; }

private:
    void CreateRenderTarget();
    void ReleaseRenderTarget();
    void CreateQuadGeometry();
    void CreateShaders();
    void CreateBlendState();
    void CreateFontAtlasTexture();
    void CreateBackdropScratchTexture();

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_backBufferTexture; // kept for CopySubresourceRegion's source
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendState;

    // Backdrop-filter scratch texture: a GPU-to-GPU copy of whatever's been painted to
    // the backbuffer so far, sampled by DrawBackdropBlurRect's blur shader. Viewport-sized,
    // recreated alongside the render target in Resize().
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_backdropScratchTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_backdropScratchSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_backdropSampler;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_fontAtlasTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_fontAtlasSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_fontAtlasSampler;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> m_fontAtlasTexture2;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_fontAtlasSRV2;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_fontAtlasSampler2;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_quadVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vsConstantBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_psConstantBuffer;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
};

} // namespace IzekipUI::Render
