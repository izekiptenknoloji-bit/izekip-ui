#pragma once

#include "Renderer/IRenderer.h"

namespace IzekipUI::Render::detail {

// CPU-side constant-buffer/UBO layouts shared byte-for-byte between the D3D11 and Vulkan
// backends -- every field is a float4 (or a pair of float2s filling one), so HLSL cbuffer
// packing and SPIR-V/UBO packing agree member-for-member with no repacking needed.

struct QuadVertex {
    float x, y;
};

struct VSConstants {
    float quadX, quadY, quadW, quadH;
    float viewportW, viewportH;
    float pad0, pad1;
};

struct Float4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

struct PSConstants {
    Float4 fillColorStart;
    Float4 fillColorEnd;
    Float4 elementRectCenterHalfSize;
    Float4 shadowRectCenterHalfSize;
    Float4 shadowColor;
    Float4 params;
};

struct RectDrawData {
    VSConstants vs;
    PSConstants ps;
};

// Quad-bounds/shadow-union/gradient-mode math shared by both backends -- see
// D3D11Renderer::DrawRect's history for why this isn't duplicated per-backend: it's the
// same "don't re-derive carefully-derived math twice" principle applied to the CPU side
// that already governs why the shader itself is a single shared .hlsl file.
RectDrawData ComputeRectDrawData(const Rect& rect, const RectPaint& paint, float viewportWidth,
                                  float viewportHeight);

} // namespace IzekipUI::Render::detail
