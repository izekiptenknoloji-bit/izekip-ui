#pragma once

#include <filesystem>

#include "CommandLine.h"

namespace IzekipSetup {

// The wizard / uninstall GUI, built entirely with İzekip UI (dogfooding). Returns the
// process exit code (0 = the install/uninstall that was started succeeded).
int RunGui(const Args& args);

std::filesystem::path DefaultPrefix(Scope scope);

} // namespace IzekipSetup
