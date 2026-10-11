// SDF (signed-distance-field) shader: draws a possibly rounded, possibly gradient-filled
// rect with an optional drop shadow, anti-aliased, in one draw call -- OR, when
// textParams.x is set, a single glyph quad sampled from a signed-distance-field font
// atlas texture. sdRoundBox is Inigo Quilez's classic rounded-box distance function. The
// shadow uses a smoothstep-of-SDF falloff approximation (not an erf/Gaussian analytic
// integral) -- still a real distance-field computation, just a smaller derivation
// surface. Both the rect path and the text path composite as premultiplied alpha, so the
// blend state must be ONE/INV_SRC_ALPHA, not SRC_ALPHA/INV_SRC_ALPHA.
//
// Shared verbatim between the D3D11 backend (compiled at runtime via D3DCompile) and the
// Vulkan backend (compiled at build time to SPIR-V via dxc.exe -spirv, invoked with
// -D VK_BACKEND=1). The [[vk::binding]] lines only exist for the Vulkan compile --
// D3DCompile never defines VK_BACKEND, so the preprocessor elides them entirely and the
// D3D11 compile sees byte-identical source to before this file was extracted.

#if VK_BACKEND
[[vk::binding(0, 0)]]
#endif
cbuffer VSConstants : register(b0) {
    float4 quadRect;      // x, y, width, height -- the actual draw quad in pixel space
    float2 viewportSize;
    float2 vsPadding;
    float4 atlasUVRect;   // x,y = UV origin (u0,v0), z,w = UV corner (u1,v1); zero for rect draws
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 pixelPos : TEXCOORD0;
    float2 atlasUV : TEXCOORD1;
    float2 screenUV : TEXCOORD2; // pixelPos/viewportSize -- absolute screen UV, used only
                                  // by the backdrop-blur path to sample the full-viewport
                                  // scratch texture (unrelated to atlasUV's per-quad range)
};

VSOutput VSMain(float2 localPos : POSITION) {
    VSOutput output;
    output.pixelPos = quadRect.xy + localPos * quadRect.zw;
    output.atlasUV = atlasUVRect.xy + localPos * (atlasUVRect.zw - atlasUVRect.xy);
    output.screenUV = output.pixelPos / viewportSize;
    float2 ndc;
    ndc.x = (output.pixelPos.x / viewportSize.x) * 2.0 - 1.0;
    ndc.y = 1.0 - (output.pixelPos.y / viewportSize.y) * 2.0;
    output.position = float4(ndc, 0.0, 1.0);
    return output;
}

#if VK_BACKEND
[[vk::binding(1, 0)]]
#endif
cbuffer PSConstants : register(b0) {
    float4 fillColorStart;             // also doubles as the glyph tint color in text mode; gradient stop 0
    float4 fillColorEnd;               // gradient's last stop (stop 1 for 2-stop, stop 2 for 3-stop)
    float4 fillColorMid;               // gradient stop 1, only used when gradientPositions.w > 2.5
    float4 elementRectCenterHalfSize;  // xy = center, zw = half-size, pixel space
    float4 shadowRectCenterHalfSize;   // xy = center, zw = half-size (ignored if no shadow)
    float4 shadowColor;
    float4 params; // x=cornerRadius, y=shadowBlurRadius,
                    // z=gradientMode(0=flat,1=right,2=left,3=top,4=bottom), w=shadowEnabled(0/1)
    float4 textParams; // x=textMode(0/1), y=onEdgeValue/255, z=aaWidth, w=fontId(0=Proportional,1=Monospace)
    float4 gradientPositions; // x,y,z = stop0/1/2 position in [0,1] along the axis, w = stop count (2 or 3)
    float4 backdropParams;      // x=backdropMode(0/1), y=blurRadiusPx, z=viewportW, w=viewportH
    float4 backdropColorAdjust; // x=brightness, y=contrast, z=saturate, w=grayscale
    float4 backdropHueAdjust;   // x=hue rotation in radians
};

#if VK_BACKEND
[[vk::binding(2, 0)]] Texture2D fontAtlasTex : register(t0);
[[vk::binding(3, 0)]] SamplerState fontAtlasSampler : register(s0);
[[vk::binding(4, 0)]] Texture2D fontAtlasTex2 : register(t1);
[[vk::binding(5, 0)]] SamplerState fontAtlasSampler2 : register(s1);
[[vk::binding(6, 0)]] Texture2D backdropTex : register(t2);
[[vk::binding(7, 0)]] SamplerState backdropSampler : register(s2);
#else
Texture2D fontAtlasTex : register(t0);
SamplerState fontAtlasSampler : register(s0);
Texture2D fontAtlasTex2 : register(t1);
SamplerState fontAtlasSampler2 : register(s1);
Texture2D backdropTex : register(t2);
SamplerState backdropSampler : register(s2);
#endif

float sdRoundBox(float2 p, float2 halfSize, float radius) {
    float2 q = abs(p) - halfSize + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

// Real 2D Gaussian convolution, single pass (not the separable 2-pass form -- that needs
// render-to-texture infra this engine doesn't have, see RectDrawData.cpp's named scope
// cut). sigma = radius/3 is the standard "3-sigma covers the kernel" choice; kernelRadius
// taps per axis means (2*kernelRadius+1)^2 total taps, bounded by the caller's
// kMaxBackdropBlurRadiusPx clamp (RectDrawData.cpp) so this never runs unbounded.
float3 SampleBackdropBlur(float2 uv) {
    float blurRadiusPx = backdropParams.y;
    float sigma = max(blurRadiusPx / 2.0, 0.0001);
    int kernelRadius = max(3, (int)ceil(blurRadiusPx * 1.5));
    float2 texel = 1.0 / float2(backdropParams.z, backdropParams.w);

    float3 sum = float3(0.0, 0.0, 0.0);
    float weightSum = 0.0;
    for (int dy = -kernelRadius; dy <= kernelRadius; ++dy) {
        for (int dx = -kernelRadius; dx <= kernelRadius; ++dx) {
            float dist = float(dx * dx + dy * dy);
            float w = exp(-dist / (2.0 * sigma * sigma)) * (1.0 - dist / (kernelRadius * kernelRadius * 2.0));
            sum += backdropTex.Sample(backdropSampler, uv + float2(dx, dy) * texel).rgb * w;
            weightSum += w;
        }
    }
    return sum / max(weightSum, 0.0001);
}

float4 PSMain(VSOutput input) : SV_TARGET {
    if (backdropParams.x > 0.5) {
        // backdrop-filter: blur the content already painted behind this element (sampled
        // from a GPU-to-GPU copy, see IRenderer::DrawBackdropBlurRect), then apply the
        // affine color adjustments in the engine's fixed canonical order (blur always
        // first -- it commutes with these, see StyleProperties.h's BackdropFilter doc
        // comment -- then brightness -> contrast -> saturate -> grayscale -> hue-rotate.
        float3 c = SampleBackdropBlur(input.screenUV);
        c *= backdropColorAdjust.x;                   // brightness: rgb *= amount
        c = (c - 0.5) * backdropColorAdjust.y + 0.5;   // contrast: (rgb-0.5)*amount+0.5
        float luminance = dot(c, float3(0.2126, 0.7152, 0.0722));
        c = lerp(luminance.xxx, c, backdropColorAdjust.z); // saturate: lerp(lum, rgb, amount)
        c = lerp(c, luminance.xxx, backdropColorAdjust.w); // grayscale: lerp(rgb, lum, amount)
        float hueRadians = backdropHueAdjust.x;
        if (abs(hueRadians) > 0.00001) {
            float cosine = cos(hueRadians);
            float sine = sin(hueRadians);
            c = float3(
                dot(c, float3(0.213 + 0.787 * cosine - 0.213 * sine,
                              0.715 - 0.715 * cosine - 0.715 * sine,
                              0.072 - 0.072 * cosine + 0.928 * sine)),
                dot(c, float3(0.213 - 0.213 * cosine + 0.143 * sine,
                              0.715 + 0.285 * cosine + 0.140 * sine,
                              0.072 - 0.072 * cosine - 0.283 * sine)),
                dot(c, float3(0.213 - 0.213 * cosine - 0.787 * sine,
                              0.715 - 0.715 * cosine + 0.715 * sine,
                              0.072 + 0.928 * cosine + 0.072 * sine)));
        }

        float cornerRadius = params.x;
        float2 elemCenter = elementRectCenterHalfSize.xy;
        float2 elemHalfSize = elementRectCenterHalfSize.zw;
        float elemDist = sdRoundBox(input.pixelPos - elemCenter, elemHalfSize, cornerRadius);
        float alpha = saturate(1.0 - smoothstep(0.0, 1.0, elemDist + 0.5)); // ~1px AA ramp
        return float4(c * alpha, alpha); // premultiplied
    }

    if (textParams.x > 0.5) {
        // Glyph quad: the atlas texel is itself an SDF (distance to the glyph outline,
        // encoded 0..1), so this is the same smoothstep-around-the-boundary technique
        // as the rect path's AA ramp, just sourced from a texture instead of sdRoundBox.
        float distance = (textParams.w > 0.5)
            ? fontAtlasTex2.Sample(fontAtlasSampler2, input.atlasUV).r
            : fontAtlasTex.Sample(fontAtlasSampler, input.atlasUV).r;
        float onEdge = textParams.y;
        float aa = max(textParams.z, 0.001);
        float alpha = smoothstep(onEdge - aa, onEdge + aa, distance);
        float4 tint = fillColorStart;
        float a = tint.a * alpha;
        return float4(tint.rgb * a, a); // premultiplied
    }

    float cornerRadius = params.x;
    float shadowBlur = params.y;
    float gradientMode = params.z;
    bool shadowOn = params.w > 0.5;

    float2 elemCenter = elementRectCenterHalfSize.xy;
    float2 elemHalfSize = elementRectCenterHalfSize.zw;
    float elemDist = sdRoundBox(input.pixelPos - elemCenter, elemHalfSize, cornerRadius);
    float fillMask = 1.0 - smoothstep(0.0, 1.0, elemDist + 0.5); // ~1px AA ramp

    float4 fillColor = fillColorStart;
    if (gradientMode > 0.5) {
        float2 unit = (input.pixelPos - (elemCenter - elemHalfSize)) / (elemHalfSize * 2.0);
        float t;
        if (gradientMode < 1.5) t = unit.x;             // to right
        else if (gradientMode < 2.5) t = 1.0 - unit.x;  // to left
        else if (gradientMode < 3.5) t = 1.0 - unit.y;  // to top
        else t = unit.y;                                 // to bottom

        // Piecewise-linear interpolation across 2 or 3 stops at their own (possibly
        // non-even, CSS-percent-driven) positions -- not just a single lerp across [0,1].
        float p0 = gradientPositions.x;
        float p1 = gradientPositions.y;
        if (gradientPositions.w > 2.5) {
            float p2 = gradientPositions.z;
            if (t <= p1) {
                float localT = (p1 > p0) ? saturate((t - p0) / (p1 - p0)) : 0.0;
                fillColor = lerp(fillColorStart, fillColorMid, localT);
            } else {
                float localT = (p2 > p1) ? saturate((t - p1) / (p2 - p1)) : 1.0;
                fillColor = lerp(fillColorMid, fillColorEnd, localT);
            }
        } else {
            float localT = (p1 > p0) ? saturate((t - p0) / (p1 - p0)) : saturate(t);
            fillColor = lerp(fillColorStart, fillColorEnd, localT);
        }
    }

    float4 shadow = float4(0, 0, 0, 0);
    if (shadowOn) {
        float2 shadowCenter = shadowRectCenterHalfSize.xy;
        float2 shadowHalfSize = shadowRectCenterHalfSize.zw;
        float shadowDist = sdRoundBox(input.pixelPos - shadowCenter, shadowHalfSize, cornerRadius);
        float shadowAlpha = 1.0 - smoothstep(0.0, shadowBlur, shadowDist);
        shadow = float4(shadowColor.rgb, shadowColor.a * saturate(shadowAlpha));
    }

    // Shader-internal Porter-Duff "fill OVER shadow", premultiplied.
    float fillAlpha = fillColor.a * saturate(fillMask);
    float3 outRgb = fillColor.rgb * fillAlpha + shadow.rgb * shadow.a * (1.0 - fillAlpha);
    float outA = fillAlpha + shadow.a * (1.0 - fillAlpha);
    return float4(outRgb, outA);
}
