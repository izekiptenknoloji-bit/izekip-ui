#pragma once

#include <filesystem>
#include <string>

namespace IzekipUI {

// Locates a runtime asset (font, shader): next to the executable first, then the working
// directory. Relative names only; absolute paths are returned unchanged.
std::filesystem::path ResolveAssetPath(const std::string& name);

} // namespace IzekipUI
