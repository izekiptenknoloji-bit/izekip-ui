/**
 * @file
 * @internal
 * Tier-2 (see COMPATIBILITY.md) -- shared D3D11/Vulkan backend plumbing, used only by
 * src/Renderer/D3D11/D3D11Renderer.cpp and src/Renderer/Vulkan/VulkanRenderer.cpp. Not
 * consumer-facing, not covered by the API compatibility policy.
 */
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
    float atlasU0, atlasV0, atlasU1, atlasV1; // zero for non-text draws
    float xformA, xformS, xformTx, xformTy;   // Transform2D: layout -> screen (identity = 1,0,0,0)
};

struct Float4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;
};

struct PSConstants {
    Float4 fillColorStart; // also doubles as the glyph tint color in text mode; gradient stop 0
    Float4 fillColorEnd;   // gradient's last stop (stop 1 for a 2-stop gradient, stop 2 for 3-stop)
    Float4 fillColorMid;   // gradient stop 1, only meaningful when gradientPositions.w > 2.5 (3-stop)
    Float4 elementRectCenterHalfSize;
    Float4 shadowRectCenterHalfSize;
    Float4 shadowColor;
    Float4 params;
    Float4 textParams;       // x=textMode(0/1), y=onEdgeValue/255, z=aaWidth, w=fontId(0=Proportional,1=Monospace)
    Float4 gradientPositions; // x,y,z = stop0/1/2 position in [0,1] along the axis, w = stop count (2 or 3)
    Float4 backdropParams;      // x=backdropMode(0/1), y=blurRadiusPx, z=viewportW, w=viewportH
    Float4 backdropColorAdjust; // x=brightness, y=contrast, z=saturate, w=grayscale
    Float4 backdropHueAdjust;   // x=hue rotation in radians
    Float4 worldXform;          // same Transform2D as VSConstants, for mapping screen -> layout in the PS
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
                                  float viewportHeight, const Transform2D& world);

// Structural analog of ComputeRectDrawData for a single glyph quad. No AA padding is
// added to the quad bounds -- stbtt_GetGlyphSDF's own `padding` argument already bakes
// that margin into each glyph's atlas cell. glyphScale (requestedFontSize /
// GlyphAtlas::GetBakedPixelHeight()) widens/narrows the SDF smoothstep ramp so the
// on-screen transition stays ~1 screen pixel at any display size.
RectDrawData ComputeTextQuadDrawData(const Rect& rect, const UVRect& atlasUV, const Color& color,
                                      float glyphScale, FontId fontId, float viewportWidth,
                                      float viewportHeight, const Transform2D& world);

// Structural analog of ComputeRectDrawData for a backdrop-blur quad. The quad bounds are
// padded by the blur kernel radius (same "compact support" reasoning as the shadow
// padding in ComputeRectDrawData) so the convolution has real source pixels to sample at
// the element's edges, not the AA ramp's clamp-to-edge fallback.
RectDrawData ComputeBackdropBlurDrawData(const Rect& rect, const BackdropBlurPaint& paint,
                                          float viewportWidth, float viewportHeight,
                                          const Transform2D& world);

} // namespace IzekipUI::Render::detail
