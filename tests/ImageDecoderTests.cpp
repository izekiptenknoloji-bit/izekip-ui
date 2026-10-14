#undef NDEBUG // tests must check their assertions in every configuration, including Release
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "BmpFixtures.h"
#include "Renderer/ImageDecoder.h"

using namespace IzekipUI::Render;
using namespace IzekipUI::TestFixtures;

namespace {

// A file name with Turkish letters: the decoder must resolve UTF-8 names on every platform.
const std::string kTurkishName = "G\xC3\xB6rsel_\xC5\x9F.bmp"; // Görsel_ş.bmp

void TestReadInfoReportsDimensions() {
    WriteFile("decoder_2x2.bmp", TwoByTwoBmp());
    ImageInfo info;
    assert(ReadImageInfo("decoder_2x2.bmp", info));
    assert(info.width == 2 && info.height == 2);
    std::cout << "TestReadInfoReportsDimensions passed\n";
}

void TestDecodeProducesRgbaTopRowFirst() {
    WriteFile("decoder_pixels.bmp", TwoByTwoBmp());
    DecodedImage img;
    assert(DecodeImageFile("decoder_pixels.bmp", img));
    assert(img.width == 2 && img.height == 2);
    assert(img.rgba.size() == 2 * 2 * 4);
    const std::uint8_t expected[] = {
        255, 0, 0, 255,       0, 255, 0, 255,       // top row: red, green
        0, 0, 255, 255,       255, 255, 255, 255,   // bottom row: blue, white
    };
    for (std::size_t i = 0; i < sizeof(expected); ++i) {
        assert(img.rgba[i] == expected[i]);
    }
    std::cout << "TestDecodeProducesRgbaTopRowFirst passed\n";
}

void TestUtf8TurkishFileName() {
    WriteFile(std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(kTurkishName.data()),
                                                  kTurkishName.size())),
              TwoByTwoBmp());
    DecodedImage img;
    assert(DecodeImageFile(kTurkishName, img));
    assert(img.width == 2);
    std::cout << "TestUtf8TurkishFileName passed\n";
}

void TestMissingFileFails() {
    DecodedImage img;
    ImageInfo info;
    assert(!DecodeImageFile("does_not_exist.png", img));
    assert(img.rgba.empty());
    assert(!ReadImageInfo("does_not_exist.png", info));
    std::cout << "TestMissingFileFails passed\n";
}

void TestEmptyAndGarbageFilesFail() {
    WriteFile("decoder_empty.bin", {});
    WriteFile("decoder_garbage.bin", {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0});
    DecodedImage img;
    ImageInfo info;
    assert(!DecodeImageFile("decoder_empty.bin", img));
    assert(!DecodeImageFile("decoder_garbage.bin", img));
    assert(!ReadImageInfo("decoder_garbage.bin", info));
    std::cout << "TestEmptyAndGarbageFilesFail passed\n";
}

void TestTruncatedPixelDataDoesNotOverrun() {
    // stb_image does not report a short BMP payload: it zero-fills the missing bytes. The
    // guarantee tested here is memory safety: the output is still the full RGBA buffer size.
    // PNG truncation is reported as a failure by stb_image and needs no special handling.
    auto bytes = TwoByTwoBmp();
    bytes.resize(54 + 4);
    WriteFile("decoder_truncated.bmp", bytes);
    DecodedImage img;
    if (DecodeImageFile("decoder_truncated.bmp", img)) {
        assert(img.rgba.size() == static_cast<std::size_t>(img.width) * img.height * 4);
    } else {
        assert(img.rgba.empty());
    }
    std::cout << "TestTruncatedPixelDataDoesNotOverrun passed\n";
}

void TestOversizedDimensionsRejected() {
    // Header claims 5000 x 1; the side limit must reject it before any pixel allocation.
    WriteFile("decoder_wide.bmp", BuildBmp(5000, 1, std::vector<std::uint8_t>(20000, 0)));
    ImageInfo info;
    DecodedImage img;
    assert(!ReadImageInfo("decoder_wide.bmp", info));
    assert(!DecodeImageFile("decoder_wide.bmp", img));
    std::cout << "TestOversizedDimensionsRejected passed\n";
}

} // namespace

int main() {
    TestReadInfoReportsDimensions();
    TestDecodeProducesRgbaTopRowFirst();
    TestUtf8TurkishFileName();
    TestMissingFileFails();
    TestEmptyAndGarbageFilesFail();
    TestTruncatedPixelDataDoesNotOverrun();
    TestOversizedDimensionsRejected();
    std::cout << "ImageDecoderTests: all passed\n";
    return 0;
}
