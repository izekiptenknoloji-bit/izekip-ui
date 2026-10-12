#include "Core/AssetPath.h"

#include <filesystem>
#include <iterator>
#include <system_error>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace IzekipUI {

std::filesystem::path ExecutablePath() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH * 4];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return {};
    }
    return std::filesystem::path(buffer);
#else
    std::error_code ec;
    return std::filesystem::read_symlink("/proc/self/exe", ec);
#endif
}

std::filesystem::path ResolveAssetPath(const std::string& name) {
    // Names are UTF-8. A narrow std::string path would be read in the ANSI code page on Windows,
    // which breaks non-ASCII directory names such as a Turkish user profile.
    const std::filesystem::path relative(std::u8string(reinterpret_cast<const char8_t*>(name.data()), name.size()));
    if (relative.is_absolute()) {
        return relative;
    }

    const std::filesystem::path exe = ExecutablePath();
    if (!exe.empty()) {
        const std::filesystem::path beside = exe.parent_path() / relative;
        std::error_code ec;
        if (std::filesystem::exists(beside, ec)) {
            return beside;
        }
    }
    // Fall back to the working directory so developer builds that run from the build tree keep working.
    return relative;
}

} // namespace IzekipUI
