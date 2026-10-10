#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace IzekipUI::Render {

// One glyph's metrics and its location in the shared SDF atlas texture. All distances
// (advance/bearing/width/height) are in pixel space at GetBakedPixelHeight() scale --
// callers rescale by (requestedFontSize / GetBakedPixelHeight()) at draw time.
struct GlyphInfo {
    float advance = 0.0f;
    float bearingX = 0.0f;
    float bearingY = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f; // normalized UV rect into the atlas
};

// Backend-agnostic: bakes a signed-distance-field glyph atlas from a TTF file for a fixed
// ASCII 32-126 character set at a fixed reference pixel height. Produces CPU-side pixels
// + UV metadata only -- no GPU API knowledge, no stb_truetype type in this header (the
// implementation is the only translation unit that includes stb_truetype.h). Each render
// backend uploads GetAtlasPixels() through its own texture-creation path.
class GlyphAtlas {
public:
    GlyphAtlas();
    ~GlyphAtlas();
    GlyphAtlas(const GlyphAtlas&) = delete;
    GlyphAtlas& operator=(const GlyphAtlas&) = delete;

    void Build(const std::string& ttfPath, float bakedPixelHeight = 48.0f);

    const GlyphInfo* FindGlyph(char c) const;
    float GetBakedPixelHeight() const { return m_bakedPixelHeight; }
    float GetAscent() const { return m_ascent; }
    float GetLineHeight() const { return m_lineHeight; }
    float GetKerning(char a, char b) const;
    float MeasureTextWidth(const std::string& text, float fontSize) const;

    const std::vector<uint8_t>& GetAtlasPixels() const { return m_atlasPixels; }
    uint32_t GetAtlasWidth() const { return m_atlasWidth; }
    uint32_t GetAtlasHeight() const { return m_atlasHeight; }

    // Lazily builds (once) from "Inter.ttf" next to the executable, same relative-path
    // convention RectShader.hlsl already uses for "RectShader.hlsl"/style.css.
    static GlyphAtlas& GetDefault();
    static GlyphAtlas& GetMonospaceDefault();

private:
    static constexpr uint32_t kAtlasWidth = 512;
    static constexpr uint32_t kAtlasHeight = 512;
    static constexpr int kFirstChar = 32;
    static constexpr int kLastChar = 126;

    std::vector<uint8_t> m_fontFileData; // kept alive: stbtt_fontinfo points into it
    std::vector<uint8_t> m_atlasPixels;
    uint32_t m_atlasWidth = 0;
    uint32_t m_atlasHeight = 0;
    float m_bakedPixelHeight = 0.0f;
    float m_ascent = 0.0f;
    float m_lineHeight = 0.0f;
    std::vector<GlyphInfo> m_glyphs; // indexed by (char - kFirstChar)

    // Opaque (pimpl-style) stbtt_fontinfo* -- kept alive for the atlas's lifetime so
    // GetKerning() can query it after Build() returns, without leaking the stb_truetype
    // type into this public header.
    void* m_fontInfo = nullptr;
};

} // namespace IzekipUI::Render
