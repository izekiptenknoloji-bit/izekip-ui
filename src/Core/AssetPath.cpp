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

namespace {

std::filesystem::path ExecutableDirectory() {
    std::error_code ec;
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH * 4];
    const DWORD length = GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0 || length >= std::size(buffer)) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
#else
    const auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (ec) {
        return {};
    }
    return exe.parent_path();
#endif
}

} // namespace

std::filesystem::path ResolveAssetPath(const std::string& name) {
    const std::filesystem::path relative(name);
    if (relative.is_absolute()) {
        return relative;
    }

    const std::filesystem::path beside = ExecutableDirectory() / relative;
    std::error_code ec;
    if (!beside.empty() && std::filesystem::exists(beside, ec)) {
        return beside;
    }
    // Fall back to the working directory so developer builds that run from the build tree keep working.
    return relative;
}

} // namespace IzekipUI
