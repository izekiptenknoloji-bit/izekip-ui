/**
 * @file
 * @internal
 * Tier-2 (see COMPATIBILITY.md) -- internal to Label.cpp/TextArea.cpp. Not consumer-facing,
 * not covered by the API compatibility policy.
 */
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "Renderer/IRenderer.h"

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

// Backend-agnostic: bakes a signed-distance-field glyph atlas from a TTF file for ASCII +
// Latin-1 Supplement (32-0xFF, contiguous) plus an explicit small sparse set of Latin
// Extended-A codepoints needed for Turkish (ş Ş ğ Ğ ı İ, which sit far past 0xFF with a
// large gap of irrelevant codepoints in between -- not worth bridging contiguously, see
// Build()'s second bake pass) at a fixed reference pixel height. Produces CPU-side pixels
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

    // Takes a decoded Unicode codepoint, not a raw byte -- Label and TextArea both decode
    // UTF-8 to codepoints before calling this.
    const GlyphInfo* FindGlyph(char32_t c) const;
    float GetBakedPixelHeight() const { return m_bakedPixelHeight; }
    float GetAscent() const { return m_ascent; }
    float GetLineHeight() const { return m_lineHeight; }
    float GetKerning(char32_t a, char32_t b) const;
    float MeasureTextWidth(const std::string& text, float fontSize) const;
    float MeasureTextWidth(const std::u32string& text, float fontSize) const;

    const std::vector<uint8_t>& GetAtlasPixels() const { return m_atlasPixels; }
    uint32_t GetAtlasWidth() const { return m_atlasWidth; }
    uint32_t GetAtlasHeight() const { return m_atlasHeight; }

    // Lazily builds (once) from "Inter.ttf" next to the executable, same relative-path
    // convention RectShader.hlsl already uses for "RectShader.hlsl"/style.css.
    static GlyphAtlas& GetDefault();
    static GlyphAtlas& GetMonospaceDefault();
    // Open-licensed stand-ins for the classic web fonts (SIL OFL, shipped in share/IzekipUI/fonts):
    // Arimo ~ Arial, Tinos ~ Times New Roman, Cousine ~ Courier New.
    static GlyphAtlas& GetArialDefault();
    static GlyphAtlas& GetSerifDefault();
    static GlyphAtlas& GetCourierDefault();
    static GlyphAtlas& ForFont(FontId id);

private:
    // 1024 wide (was 512): widening kLastChar to 0xFF bakes ~90 more real Latin-1
    // Supplement glyphs on top of the original 95 ASCII ones -- doesn't fit 512x512
    // anymore. Height stays 512; if Build() still throws its overflow error for either
    // font, bump this further (see the doc comment on kLastChar).
    static constexpr uint32_t kAtlasWidth = 1024;
    static constexpr uint32_t kAtlasHeight = 512;
    static constexpr int kFirstChar = 32;
    // 0xFF (was 126): ASCII + Latin-1 Supplement, contiguous. Deliberately NOT widened
    // further to bridge the 6 Turkish Latin Extended-A codepoints (0x11E-0x15F) contiguously
    // -- that range is mostly OTHER European languages' letters that a comprehensive font
    // like Inter.ttf likely has real outlines for, risking a much larger atlas than this
    // feature needs. Those 6 codepoints are baked separately below instead.
    static constexpr int kLastChar = 0xFF;

    // ş Ş ğ Ğ ı İ -- baked in a second pass in Build(), looked up via a short linear scan
    // in FindGlyph() when a codepoint falls outside [kFirstChar, kLastChar]. Six entries is
    // cheap enough that this doesn't need a hash map.
    static constexpr std::array<char32_t, 6> kExtendedCodepoints = {0x15F, 0x15E, 0x11F, 0x11E, 0x131, 0x130};

    std::vector<uint8_t> m_fontFileData; // kept alive: stbtt_fontinfo points into it
    std::vector<uint8_t> m_atlasPixels;
    uint32_t m_atlasWidth = 0;
    uint32_t m_atlasHeight = 0;
    float m_bakedPixelHeight = 0.0f;
    float m_ascent = 0.0f;
    float m_lineHeight = 0.0f;
    std::vector<GlyphInfo> m_glyphs; // indexed by (c - kFirstChar), c in [kFirstChar, kLastChar]
    std::array<std::pair<char32_t, GlyphInfo>, 6> m_extendedGlyphs{}; // parallel to kExtendedCodepoints

    // Opaque (pimpl-style) stbtt_fontinfo* -- kept alive for the atlas's lifetime so
    // GetKerning() can query it after Build() returns, without leaking the stb_truetype
    // type into this public header.
    void* m_fontInfo = nullptr;
};

} // namespace IzekipUI::Render
