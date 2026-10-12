#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <exception>
#include <stdexcept>
#include <string>

#include "Renderer/GlyphAtlas.h"

using namespace IzekipUI::Render;

namespace {

bool Throws(const std::string& path) {
    GlyphAtlas atlas;
    try {
        atlas.Build(path);
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestMissingAndEmptyFontFilesAreRejected() {
    assert(Throws((std::filesystem::temp_directory_path() / "izekip-missing-font.ttf").string()));

    const auto empty = std::filesystem::temp_directory_path() / "izekip-empty-font.ttf";
    { std::ofstream out(empty, std::ios::binary | std::ios::trunc); }
    assert(Throws(empty.string()));
    std::filesystem::remove(empty);
    std::cout << "TestMissingAndEmptyFontFilesAreRejected passed\n";
}

void TestEveryBakedGlyphStaysInsideTheAtlas() {
    // IZEKIP_TEST_FONT_DIR is the repository's third_party/fonts directory (set by CMake).
    GlyphAtlas atlas;
    atlas.Build(std::string(IZEKIP_TEST_FONT_DIR) + "/Inter.ttf");
    const float width = static_cast<float>(atlas.GetAtlasWidth());
    const float height = static_cast<float>(atlas.GetAtlasHeight());

    int baked = 0;
    for (char32_t c = 32; c <= 0xFF; ++c) {
        const GlyphInfo* glyph = atlas.FindGlyph(c);
        if (!glyph || glyph->width <= 0.0f) {
            continue;
        }
        ++baked;
        assert(glyph->u0 >= 0.0f && glyph->v0 >= 0.0f);
        assert(glyph->u1 <= 1.0f && glyph->v1 <= 1.0f);
        assert(glyph->u1 * width <= width + 0.5f && glyph->v1 * height <= height + 0.5f);
        assert(glyph->width <= width && glyph->height <= height);
    }
    assert(baked > 90); // ASCII printable range must have produced real glyphs
    std::cout << "TestEveryBakedGlyphStaysInsideTheAtlas passed (" << baked << " glyphs)\n";
}

} // namespace

int RunGlyphAtlasTests() {
    TestMissingAndEmptyFontFilesAreRejected();
    TestEveryBakedGlyphStaysInsideTheAtlas();
    std::cout << "All GlyphAtlasTests passed.\n";
    return 0;
}

int main() {
    try {
        return RunGlyphAtlasTests();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "GlyphAtlasTests: %s\n", error.what());
        return 1;
    }
}
