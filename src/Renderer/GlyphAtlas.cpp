#include "Renderer/GlyphAtlas.h"

#include <algorithm>
#include <cstdio>
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

    // Shared by the main contiguous range below AND the extended sparse codepoints --
    // packs into the same shelf-packer state/cursor (never reset between the two), so both
    // sets land in the same atlas back-to-back.
    auto bakeOneCodepoint = [&](int c, GlyphInfo& glyph) {
        const int glyphIndex = stbtt_FindGlyphIndex(info, c);

        int advanceWidth = 0, leftSideBearing = 0;
        stbtt_GetCodepointHMetrics(info, c, &advanceWidth, &leftSideBearing);
        glyph.advance = advanceWidth * scale;

        if (glyphIndex == 0) {
            return; // no glyph for this codepoint in this font
        }

        int w = 0, h = 0, xoff = 0, yoff = 0;
        unsigned char* sdf = stbtt_GetGlyphSDF(info, scale, glyphIndex, kPadding, kOnEdgeValue,
                                                kPixelDistScale, &w, &h, &xoff, &yoff);
        if (!sdf || w <= 0 || h <= 0) {
            if (sdf) stbtt_FreeSDF(sdf, nullptr);
            return; // e.g. space has no visible outline
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
    };

    for (int c = kFirstChar; c <= kLastChar; ++c) {
        bakeOneCodepoint(c, m_glyphs[c - kFirstChar]);
    }

    for (size_t i = 0; i < kExtendedCodepoints.size(); ++i) {
        const char32_t cp = kExtendedCodepoints[i];
        m_extendedGlyphs[i].first = cp;
        bakeOneCodepoint(static_cast<int>(cp), m_extendedGlyphs[i].second);
    }
}

const GlyphInfo* GlyphAtlas::FindGlyph(char32_t c) const {
    if (c >= static_cast<char32_t>(kFirstChar) && c <= static_cast<char32_t>(kLastChar)) {
        return &m_glyphs[static_cast<size_t>(c) - kFirstChar];
    }
    for (const auto& [codepoint, info] : m_extendedGlyphs) {
        if (codepoint == c) {
            return &info;
        }
    }
    return nullptr;
}

float GlyphAtlas::GetKerning(char32_t a, char32_t b) const {
    if (!m_fontInfo || a > 0xFF || b > 0xFF) {
        // stb_truetype's kerning table lookup only makes sense for codepoints we treat as
        // "narrow" here; the 6 Turkish Latin Extended-A codepoints never kern against
        // anything in this engine (acceptable -- kerning is a minor visual refinement, not
        // a correctness requirement).
        return 0.0f;
    }
    auto* info = static_cast<stbtt_fontinfo*>(m_fontInfo);
    const float scale = stbtt_ScaleForPixelHeight(info, m_bakedPixelHeight);
    return stbtt_GetCodepointKernAdvance(info, static_cast<int>(a), static_cast<int>(b)) * scale;
}

float GlyphAtlas::MeasureTextWidth(const std::string& text, float fontSize) const {
    const float scale = m_bakedPixelHeight > 0.0f ? fontSize / m_bakedPixelHeight : 1.0f;
    float width = 0.0f;
    char prev = 0;
    for (char c : text) {
        const GlyphInfo* glyph = FindGlyph(static_cast<unsigned char>(c));
        if (!glyph) {
            prev = 0;
            continue;
        }
        if (prev) {
            width += GetKerning(static_cast<unsigned char>(prev), static_cast<unsigned char>(c)) * scale;
        }
        width += glyph->advance * scale;
        prev = c;
    }
    return width;
}

float GlyphAtlas::MeasureTextWidth(const std::u32string& text, float fontSize) const {
    const float scale = m_bakedPixelHeight > 0.0f ? fontSize / m_bakedPixelHeight : 1.0f;
    float width = 0.0f;
    char32_t prev = 0;
    for (char32_t c : text) {
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

GlyphAtlas& GlyphAtlas::ForFont(FontId id) {
    switch (id) {
        case FontId::Monospace: return GetMonospaceDefault();
        case FontId::Arial: return GetArialDefault();
        case FontId::Serif: return GetSerifDefault();
        case FontId::Courier: return GetCourierDefault();
        case FontId::Proportional: break;
    }
    return GetDefault();
}

// The classic-web-font stand-ins are optional at runtime: if the file is missing the face falls
// back to Inter, so a damaged install degrades the typography instead of stopping the app.
namespace {
bool TryBuild(GlyphAtlas& atlas, const char* fileName) {
    try {
        atlas.Build(fileName);
        return true;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "IzekipUI: %s (falling back to Inter)\n", error.what());
        return false;
    }
}
} // namespace

GlyphAtlas& GlyphAtlas::GetArialDefault() {
    static GlyphAtlas atlas;
    static bool built = false;
    if (!built) {
        built = true;
        if (!TryBuild(atlas, "Arimo-Regular.ttf")) {
            return GetDefault();
        }
    }
    return atlas;
}

GlyphAtlas& GlyphAtlas::GetSerifDefault() {
    static GlyphAtlas atlas;
    static bool built = false;
    if (!built) {
        built = true;
        if (!TryBuild(atlas, "Tinos-Regular.ttf")) {
            return GetDefault();
        }
    }
    return atlas;
}

GlyphAtlas& GlyphAtlas::GetCourierDefault() {
    static GlyphAtlas atlas;
    static bool built = false;
    if (!built) {
        built = true;
        if (!TryBuild(atlas, "Cousine-Regular.ttf")) {
            return GetDefault();
        }
    }
    return atlas;
}

} // namespace IzekipUI::Render
