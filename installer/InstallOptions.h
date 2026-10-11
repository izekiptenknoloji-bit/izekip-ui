#pragma once

#include <filesystem>
#include <string>

namespace IzekipSetup {

enum class Scope { CurrentUser, AllUsers };
enum class Backend { D3D11, Vulkan };

struct InstallOptions {
    Scope scope = Scope::CurrentUser;
    std::filesystem::path prefix;
    Backend backend = Backend::D3D11;
    bool addToPath = true;
    bool setCmakeDirEnv = true;
    bool registerUninstall = true;
    bool startMenuShortcut = true;
    bool aiGuide = true; // copy share/doc/IzekipUI/AI_GUIDE.md (teaching text for AI coding assistants)
};

inline const char* BackendDirName(Backend backend) {
    return backend == Backend::Vulkan ? "vulkan" : "d3d11";
}

// UTF-8 <-> std::filesystem::path without <windows.h> (C++20 u8string round-trip).
inline std::string PathToUtf8(const std::filesystem::path& path) {
    const std::u8string s = path.u8string();
    return std::string(s.begin(), s.end());
}

inline std::filesystem::path PathFromUtf8(const std::string& utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

} // namespace IzekipSetup
