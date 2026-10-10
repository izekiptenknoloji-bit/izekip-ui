#include "Renderer/GlyphAtlas.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <stdexcept>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace IzekipUI::Render {

GlyphAtlas::GlyphAtlas() = default;

GlyphAtlas::~GlyphAtlas() {
    delete static_cast<stbtt_fontinfo*>(m_fontInfo);
}

void GlyphAtlas::Build(const std::string& ttfPath, float bakedPixelHeight) {
    std::ifstream file(ttfPath, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error("GlyphAtlas: could not open font file: " + ttfPath);
    }
    const std::streamsize size = file.tellg();
    file.seekg(0);
    m_fontFileData.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(m_fontFileData.data()), size);

    auto* info = new stbtt_fontinfo();
    if (!stbtt_InitFont(info, m_fontFileData.data(),
                         stbtt_GetFontOffsetForIndex(m_fontFileData.data(), 0))) {
        delete info;
        throw std::runtime_error("GlyphAtlas: stbtt_InitFont failed for " + ttfPath);
    }
    m_fontInfo = info;

    m_bakedPixelHeight = bakedPixelHeight;
    const float scale = stbtt_ScaleForPixelHeight(info, bakedPixelHeight);

    int ascent = 0, descent = 0, lineGap = 0;
    stbtt_GetFontVMetrics(info, &ascent, &descent, &lineGap);
    m_ascent = ascent * scale;
    m_lineHeight = (ascent - descent + lineGap) * scale;

    m_atlasWidth = kAtlasWidth;
    m_atlasHeight = kAtlasHeight;
    m_atlasPixels.assign(static_cast<size_t>(kAtlasWidth) * kAtlasHeight, 0);
    m_glyphs.assign(kLastChar - kFirstChar + 1, GlyphInfo{});

    // padding=4, onedge_value=180: the SDF encodes distance as
    // onedge_value + pixel_dist_scale * signedDistance, clamped to [0,255]. Using
    // onedge_value/padding as pixel_dist_scale means the SDF saturates at exactly the
    // padding distance -- matches the padding we reserve around each glyph cell.
    constexpr int kPadding = 4;
    constexpr unsigned char kOnEdgeValue = 180;
    constexpr float kPixelDistScale = static_cast<float>(kOnEdgeValue) / kPadding;

    uint32_t cursorX = 0, shelfY = 0, shelfHeight = 0;

    for (int c = kFirstChar; c <= kLastChar; ++c) {
        const int glyphIndex = stbtt_FindGlyphIndex(info, c);
        GlyphInfo& glyph = m_glyphs[c - kFirstChar];

        int advanceWidth = 0, leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(info, c, &advanceWidth, &leftSideBearing);
        glyph.advance = advanceWidth * scale;

        if (glyphIndex == 0) {
            continue; // no glyph for this codepoint in this font
        }

        int w = 0, h = 0, xoff = 0, yoff = 0;
        unsigned char* sdf = stbtt_GetGlyphSDF(info, scale, glyphIndex, kPadding, kOnEdgeValue,
                                                kPixelDistScale, &w, &h, &xoff, &yoff);
        if (!sdf || w <= 0 || h <= 0) {
            if (sdf) stbtt_FreeSDF(sdf, nullptr);
            continue; // e.g. space has no visible outline
        }

        if (cursorX + static_cast<uint32_t>(w) > kAtlasWidth) {
            shelfY += shelfHeight + 1;
            shelfHeight = 0;
            cursorX = 0;
        }
        if (shelfY + static_cast<uint32_t>(h) > kAtlasHeight) {
            stbtt_FreeSDF(sdf, nullptr);
            throw std::runtime_error("GlyphAtlas: atlas overflow -- increase kAtlasWidth/kAtlasHeight");
        }

        for (int row = 0; row < h; ++row) {
            std::memcpy(&m_atlasPixels[static_cast<size_t>(shelfY + row) * kAtlasWidth + cursorX],
                        &sdf[row * w], static_cast<size_t>(w));
        }

        glyph.width = static_cast<float>(w);
        glyph.height = static_cast<float>(h);
        glyph.bearingX = static_cast<float>(xoff);
        glyph.bearingY = static_cast<float>(yoff);
        glyph.u0 = static_cast<float>(cursorX) / static_cast<float>(kAtlasWidth);
        glyph.v0 = static_cast<float>(shelfY) / static_cast<float>(kAtlasHeight);
        glyph.u1 = static_cast<float>(cursorX + w) / static_cast<float>(kAtlasWidth);
        glyph.v1 = static_cast<float>(shelfY + h) / static_cast<float>(kAtlasHeight);

        cursorX += static_cast<uint32_t>(w) + 1;
        shelfHeight = std::max(shelfHeight, static_cast<uint32_t>(h));

        stbtt_FreeSDF(sdf, nullptr);
    }
}

const GlyphInfo* GlyphAtlas::FindGlyph(char c) const {
    const int ic = static_cast<unsigned char>(c);
    if (ic < kFirstChar || ic > kLastChar) {
        return nullptr;
    }
    return &m_glyphs[static_cast<size_t>(ic - kFirstChar)];
}

float GlyphAtlas::GetKerning(char a, char b) const {
    if (!m_fontInfo) {
        return 0.0f;
    }
    auto* info = static_cast<stbtt_fontinfo*>(m_fontInfo);
    const float scale = stbtt_ScaleForPixelHeight(info, m_bakedPixelHeight);
    return stbtt_GetCodepointKernAdvance(info, static_cast<unsigned char>(a),
                                          static_cast<unsigned char>(b)) *
           scale;
}

float GlyphAtlas::MeasureTextWidth(const std::string& text, float fontSize) const {
    const float scale = m_bakedPixelHeight > 0.0f ? fontSize / m_bakedPixelHeight : 1.0f;
    float width = 0.0f;
    char prev = 0;
    for (char c : text) {
        const GlyphInfo* glyph = FindGlyph(c);
        if (!glyph) {
            prev = 0;
            continue;
        }
        if (prev) {
            width += GetKerning(prev, c) * scale;
        }
        width += glyph->advance * scale;
        prev = c;
    }
    return width;
}

GlyphAtlas& GlyphAtlas::GetDefault() {
    static GlyphAtlas atlas;
    static bool built = false;
    if (!built) {
        atlas.Build("Inter.ttf");
        built = true;
    }
    return atlas;
}

GlyphAtlas& GlyphAtlas::GetMonospaceDefault() {
    static GlyphAtlas atlas;
    static bool built = false;
    if (!built) {
        atlas.Build("SpaceMono-Regular.ttf");
        built = true;
    }
    return atlas;
}

} // namespace IzekipUI::Render
