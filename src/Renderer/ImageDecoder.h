#pragma once

// Internal to the library: the decoder is not part of the public API (no header under include/).
// Both renderers and the Image widget use it; applications only see DrawImage and Image.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace IzekipUI::Render {

// Limits protect against hostile or accidental huge files. A 4096x4096 RGBA image is 64 MiB
// once decoded, which is the most a single image may cost.
constexpr std::size_t kMaxImageFileBytes = 32 * 1024 * 1024;
constexpr int kMaxImageSide = 4096;

struct ImageInfo {
    int width = 0;
    int height = 0;
};

struct DecodedImage {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> rgba; // width * height * 4, top row first
};

// Reads only the header. Returns false when the file is missing, larger than the limit, not a
// recognized image format, or has a side longer than kMaxImageSide.
bool ReadImageInfo(const std::string& path, ImageInfo& out);

// Decodes the whole image to RGBA8. Returns false (and leaves out empty) on any failure.
bool DecodeImageFile(const std::string& path, DecodedImage& out);

} // namespace IzekipUI::Render
