#pragma once

// Builds small BMP files in memory so image tests need no binary assets. BMP is the simplest
// format stb_image reads, and it exercises the same decode path the library uses for PNG.

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace IzekipUI::TestFixtures {

inline void PutLE16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}

inline void PutLE32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    PutLE16(b, static_cast<std::uint16_t>(v & 0xFFFF));
    PutLE16(b, static_cast<std::uint16_t>(v >> 16));
}

// BMP with the given dimensions and pixel payload. Bottom-up rows, 24 bpp, rows padded to 4 bytes
// (the caller supplies the padded payload).
inline std::vector<std::uint8_t> BuildBmp(std::int32_t width, std::int32_t height,
                                          const std::vector<std::uint8_t>& pixels) {
    std::vector<std::uint8_t> b;
    b.push_back('B');
    b.push_back('M');
    PutLE32(b, static_cast<std::uint32_t>(54 + pixels.size())); // file size
    PutLE32(b, 0);                                              // reserved
    PutLE32(b, 54);                                             // pixel data offset
    PutLE32(b, 40);                                             // info header size
    PutLE32(b, static_cast<std::uint32_t>(width));
    PutLE32(b, static_cast<std::uint32_t>(height));
    PutLE16(b, 1);                                              // planes
    PutLE16(b, 24);                                             // bits per pixel
    PutLE32(b, 0);                                              // compression: none
    PutLE32(b, static_cast<std::uint32_t>(pixels.size()));      // image size
    PutLE32(b, 0);                                              // x pixels per meter
    PutLE32(b, 0);                                              // y pixels per meter
    PutLE32(b, 0);                                              // colors used
    PutLE32(b, 0);                                              // important colors
    b.insert(b.end(), pixels.begin(), pixels.end());
    return b;
}

// 2x2 image: top-left red, top-right green, bottom-left blue, bottom-right white.
// BMP stores the bottom row first and pixels as B,G,R; each row is padded to 4 bytes.
inline std::vector<std::uint8_t> TwoByTwoBmp() {
    const std::vector<std::uint8_t> px = {
        255, 0, 0,   255, 255, 255, 0, 0, // bottom row: blue, white, 2 pad bytes
        0, 0, 255,   0, 255, 0,     0, 0, // top row:    red,  green, 2 pad bytes
    };
    return BuildBmp(2, 2, px);
}

inline void WriteFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
}

} // namespace IzekipUI::TestFixtures
