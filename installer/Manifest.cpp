#include "Manifest.h"

#include <fstream>
#include <sstream>
#include <system_error>

namespace IzekipSetup {

std::optional<InstallManifest> LoadManifest(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return std::nullopt;
    }
    InstallManifest m;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);

        if (key == "version") m.version = value;
        else if (key == "scope") m.scope = value == "all" ? Scope::AllUsers : Scope::CurrentUser;
        else if (key == "prefix") m.prefix = PathFromUtf8(value);
        else if (key == "backend") m.backend = value == "vulkan" ? Backend::Vulkan : Backend::D3D11;
        else if (key == "path_added") m.pathAdded = value == "1";
        else if (key == "env_set") m.envSet = value == "1";
        else if (key == "env_value") m.envValue = value;
        else if (key == "env_prev_present") m.envPrevPresent = value == "1";
        else if (key == "env_prev") m.envPrev = value;
        else if (key == "uninstall_registered") m.uninstallRegistered = value == "1";
        else if (key == "shortcut") m.shortcut = PathFromUtf8(value);
        else if (key == "file") m.files.push_back(value);
        else if (key == "bytes") m.bytes = std::stoull(value.empty() ? "0" : value);
    }
    return m;
}

bool SaveManifest(const std::filesystem::path& path, const InstallManifest& m) {
    std::ostringstream out;
    out << "version=" << m.version << '\n';
    out << "scope=" << (m.scope == Scope::AllUsers ? "all" : "user") << '\n';
    out << "prefix=" << PathToUtf8(m.prefix) << '\n';
    out << "backend=" << BackendDirName(m.backend) << '\n';
    out << "path_added=" << (m.pathAdded ? 1 : 0) << '\n';
    out << "env_set=" << (m.envSet ? 1 : 0) << '\n';
    out << "env_value=" << m.envValue << '\n';
    out << "env_prev_present=" << (m.envPrevPresent ? 1 : 0) << '\n';
    out << "env_prev=" << m.envPrev << '\n';
    out << "uninstall_registered=" << (m.uninstallRegistered ? 1 : 0) << '\n';
    out << "shortcut=" << PathToUtf8(m.shortcut) << '\n';
    out << "bytes=" << m.bytes << '\n';
    for (const auto& f : m.files) {
        out << "file=" << f << '\n';
    }

    std::filesystem::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file << out.str();
        if (!file) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec); // replaces an existing manifest
    return !ec;
}

} // namespace IzekipSetup
