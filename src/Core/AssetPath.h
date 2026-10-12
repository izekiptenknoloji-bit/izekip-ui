#pragma once

#include <filesystem>
#include <string>

namespace IzekipUI {

// Full path of the running executable, or an empty path if the OS does not report it.
std::filesystem::path ExecutablePath();

// Locates a runtime asset (font, shader): next to the executable first, then the working
// directory. Relative names only; absolute paths are returned unchanged.
std::filesystem::path ResolveAssetPath(const std::string& name);

} // namespace IzekipUI
