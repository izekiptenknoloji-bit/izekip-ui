#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "InstallOptions.h"

namespace IzekipSetup {

enum class Mode { Install, Uninstall };

// Command-line contract, used both by humans/scripts and by the installer relaunching
// itself (elevation, temp-copied uninstaller):
//   --uninstall          uninstall mode (default: install)
//   --from-temp          uninstaller already running from its %TEMP% copy
//   --auto-install       skip the wizard, start installing immediately (elevated relaunch)
//   --headless           no window; exit code 0 = success, 1 = failure
//   --no-system          files + manifest only: no PATH/env/shortcut/registry changes
//   --scope=user|all  --prefix=<dir>  --backend=d3d11|vulkan  --payload=<dir>
//   --path=0|1  --env=0|1  --uninstall-entry=0|1  --shortcut=0|1  --wait-pid=<pid>
struct Args {
    Mode mode = Mode::Install;
    bool fromTemp = false;
    bool autoInstall = false;
    bool headless = false;
    bool noSystem = false;
    std::optional<Scope> scope;
    std::optional<std::filesystem::path> prefix;
    std::optional<Backend> backend;
    std::optional<std::filesystem::path> payload;
    std::optional<bool> addToPath;
    std::optional<bool> setCmakeDirEnv;
    std::optional<bool> registerUninstall;
    std::optional<bool> startMenuShortcut;
    std::optional<bool> aiGuide;
    unsigned long waitPid = 0;
};

Args ParseArgs(const std::vector<std::wstring>& argsWithoutExe);

// Quotes per CommandLineToArgvW rules (backslashes before a quote are doubled).
std::wstring SerializeArgs(const Args& args);

} // namespace IzekipSetup
