// SDF (signed-distance-field) shader: draws a possibly rounded, possibly gradient-filled
// rect with an optional drop shadow, anti-aliased, in one draw call. sdRoundBox is Inigo
// Quilez's classic rounded-box distance function. The shadow uses a smoothstep-of-SDF
// falloff approximation (not an erf/Gaussian analytic integral) -- still a real
// distance-field computation, just a smaller derivation surface. Fill-over-shadow is
// composited inside the shader (premultiplied alpha), so the blend state must be
// ONE/INV_SRC_ALPHA, not SRC_ALPHA/INV_SRC_ALPHA.
//
// Shared verbatim between the D3D11 backend (compiled at runtime via D3DCompile) and the
// Vulkan backend (compiled at build time to SPIR-V via dxc.exe -spirv, invoked with
// -D VK_BACKEND=1). The two [[vk::binding]] lines only exist for the Vulkan compile --
// D3DCompile never defines VK_BACKEND, so the preprocessor elides them entirely and the
// D3D11 compile sees byte-identical source to before this file was extracted.

#if VK_BACKEND
[[vk::binding(0, 0)]]
#endif
cbuffer VSConstants : register(b0) {
    float4 quadRect;      // x, y, width, height -- the actual draw quad in pixel space
    float2 viewportSize;
    float2 vsPadding;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float2 pixelPos : TEXCOORD0;
};

VSOutput VSMain(float2 localPos : POSITION) {
    VSOutput output;
    output.pixelPos = quadRect.xy + localPos * quadRect.zw;
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
    float4 fillColorStart;
    float4 fillColorEnd;
    float4 elementRectCenterHalfSize;  // xy = center, zw = half-size, pixel space
    float4 shadowRectCenterHalfSize;   // xy = center, zw = half-size (ignored if no shadow)
    float4 shadowColor;
    float4 params; // x=cornerRadius, y=shadowBlurRadius,
                    // z=gradientMode(0=flat,1=right,2=left,3=top,4=bottom), w=shadowEnabled(0/1)
};

float sdRoundBox(float2 p, float2 halfSize, float radius) {
    float2 q = abs(p) - halfSize + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float4 PSMain(VSOutput input) : SV_TARGET {
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
        fillColor = lerp(fillColorStart, fillColorEnd, saturate(t));
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
