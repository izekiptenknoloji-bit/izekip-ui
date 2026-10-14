#include "Renderer/ImageDecoder.h"

#include "Core/AssetPath.h"

#include <fstream>

// stb_image is public domain (v2.30). Only this translation unit includes it.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#include "stb_image.h"

namespace IzekipUI::Render {

namespace {

// Reads the whole file, or returns false when it is missing, empty, or over the size limit.
bool ReadFileBounded(const std::string& path, std::vector<std::uint8_t>& bytes) {
    std::ifstream file(ResolveAssetPath(path), std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    const std::streamoff size = file.tellg();
    if (size <= 0 || static_cast<std::size_t>(size) > kMaxImageFileBytes) {
        return false;
    }
    file.seekg(0);
    bytes.resize(static_cast<std::size_t>(size));
    return static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), size));
}

bool SizeAllowed(int width, int height) {
    return width > 0 && height > 0 && width <= kMaxImageSide && height <= kMaxImageSide;
}

} // namespace

bool ReadImageInfo(const std::string& path, ImageInfo& out) {
    std::vector<std::uint8_t> bytes;
    if (!ReadFileBounded(path, bytes)) {
        return false;
    }
    int width = 0, height = 0, comp = 0;
    if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &comp)) {
        return false;
    }
    if (!SizeAllowed(width, height)) {
        return false;
    }
    out.width = width;
    out.height = height;
    return true;
}

bool DecodeImageFile(const std::string& path, DecodedImage& out) {
    out = DecodedImage{};
    std::vector<std::uint8_t> bytes;
    if (!ReadFileBounded(path, bytes)) {
        return false;
    }
    int width = 0, height = 0, comp = 0;
    if (!stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &comp)) {
        return false;
    }
    if (!SizeAllowed(width, height)) {
        return false;
    }
    // req_comp = 4 converts gray, gray+alpha, palette and RGB sources to RGBA8.
    stbi_uc* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                            &width, &height, &comp, 4);
    if (!pixels) {
        return false;
    }
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
    out.width = width;
    out.height = height;
    out.rgba.assign(pixels, pixels + count);
    stbi_image_free(pixels);
    return true;
}

} // namespace IzekipUI::Render
