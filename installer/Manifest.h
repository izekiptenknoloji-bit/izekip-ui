#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "InstallOptions.h"

namespace IzekipSetup {

// What an installation actually did -- written to <prefix>\uninstall\install.manifest and
// read back by the uninstaller so it reverses exactly that (and nothing else). Plain UTF-8
// key=value lines, one `file=` line per installed file (relative path, forward slashes).
struct InstallManifest {
    std::string version;
    Scope scope = Scope::CurrentUser;
    std::filesystem::path prefix;
    Backend backend = Backend::D3D11;

    bool pathAdded = false;            // we appended <prefix>\bin to PATH ourselves
    bool envSet = false;               // we set IzekipUI_DIR
    std::string envValue;              // the value we set
    bool envPrevPresent = false;       // IzekipUI_DIR existed before we set it
    std::string envPrev;               // ...and its previous value
    bool uninstallRegistered = false;  // Apps & features entry written
    std::filesystem::path shortcut;    // empty = none created

    std::vector<std::string> files;
    std::uint64_t bytes = 0;
};

inline constexpr const char* kManifestFileName = "install.manifest";

std::optional<InstallManifest> LoadManifest(const std::filesystem::path& path);
bool SaveManifest(const std::filesystem::path& path, const InstallManifest& manifest); // atomic

} // namespace IzekipSetup
