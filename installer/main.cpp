#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>

#include "CommandLine.h"
#include "Core/Version.h"
#include "InstallEngine.h"
#include "InstallerApp.h"
#include "Manifest.h"
#include "WinSys.h"

namespace fs = std::filesystem;
using namespace IzekipSetup;

namespace {

constexpr const char* kTempDirPrefix = "IzekipUI-uninstall-";

InstallOptions OptionsFromArgs(const Args& args) {
    InstallOptions o;
    o.scope = args.scope.value_or(Scope::CurrentUser);
    o.prefix = args.prefix.value_or(DefaultPrefix(o.scope));
    o.backend = args.backend.value_or(Backend::D3D11);
    o.addToPath = args.addToPath.value_or(true);
    o.setCmakeDirEnv = args.setCmakeDirEnv.value_or(true);
    o.registerUninstall = args.registerUninstall.value_or(true);
    o.startMenuShortcut = args.startMenuShortcut.value_or(true);
    return o;
}

int RunEngineToCompletion(InstallEngine& engine) {
    while (engine.IsRunning()) {
        engine.Step(std::chrono::milliseconds(50));
    }
    const auto& status = engine.GetStatus();
    if (status.failed) {
        std::fprintf(stderr, "IzekipUISetup: %s\n", status.error.c_str());
        return 1;
    }
    return 0;
}

int RunHeadless(const Args& args) {
    InstallEngine engine;
    std::string error;
    if (args.mode == Mode::Uninstall) {
        if (!args.prefix) {
            std::fprintf(stderr, "IzekipUISetup: --prefix is required for --uninstall\n");
            return 1;
        }
        if (!engine.BeginUninstall(*args.prefix, args.noSystem, &error)) {
            std::fprintf(stderr, "IzekipUISetup: %s\n", error.c_str());
            return 1;
        }
    } else {
        const InstallOptions options = OptionsFromArgs(args);
        if (options.scope == Scope::AllUsers && !args.noSystem && !WinSys::IsElevated()) {
            std::fprintf(stderr, "IzekipUISetup: --scope=all needs an elevated (administrator) process\n");
            return 1;
        }
        const fs::path payload = args.payload.value_or(WinSys::ExeDir() / "payload");
        if (!engine.BeginInstall(options, payload, WinSys::ExeDir(), IZEKIP_VERSION_STRING, args.noSystem, &error)) {
            std::fprintf(stderr, "IzekipUISetup: %s\n", error.c_str());
            return 1;
        }
    }
    return RunEngineToCompletion(engine);
}

// Launched from Apps & features (or by hand) as <prefix>\uninstall\IzekipUISetup.exe
// --uninstall. A running exe can't delete itself or its folder, so copy the uninstaller
// to %TEMP% and run THAT copy, which waits for this process to exit first.
int RunUninstallBootstrap(const Args& args) {
    const fs::path selfDir = WinSys::ExeDir();
    fs::path prefix = args.prefix.value_or(fs::path{});
    Scope scope = Scope::CurrentUser;
    if (auto manifest = LoadManifest(selfDir / kManifestFileName)) {
        prefix = manifest->prefix;
        scope = manifest->scope;
    }
    if (prefix.empty()) {
        std::fprintf(stderr, "IzekipUISetup: no install manifest next to the uninstaller and no --prefix\n");
        return 1;
    }

    const unsigned long pid = WinSys::CurrentPid();
    const fs::path tempDir = WinSys::KnownFolder(WinSys::Folder::Temp) / (kTempDirPrefix + std::to_string(pid));
    std::error_code ec;
    fs::create_directories(tempDir, ec);
    for (const auto& name : InstallEngine::SelfAssetNames()) {
        fs::copy_file(selfDir / name, tempDir / name, fs::copy_options::overwrite_existing, ec);
    }

    Args child = args;
    child.fromTemp = true;
    child.prefix = prefix;
    child.waitPid = pid;
    const bool elevate = scope == Scope::AllUsers && !WinSys::IsElevated();
    const auto result = WinSys::Relaunch(tempDir / "IzekipUISetup.exe", SerializeArgs(child), elevate);
    return result == WinSys::LaunchResult::Ok ? 0 : 1;
}

// Only ever removes a directory the bootstrap itself created in %TEMP% -- never the build
// tree or an arbitrary folder the exe happens to be run from.
void CleanupTempCopyIfAny() {
    const fs::path dir = WinSys::ExeDir();
    const std::string name = PathToUtf8(dir.filename());
    if (name.rfind(kTempDirPrefix, 0) == 0) {
        WinSys::SpawnDelayedDirectoryRemoval(dir);
    }
}

} // namespace

int main() {
    WinSys::SetCwdToExeDir();
    const Args args = ParseArgs(WinSys::CommandLineArgsWithoutExe());

    if (args.mode == Mode::Uninstall && !args.fromTemp) {
        return RunUninstallBootstrap(args);
    }
    if (args.fromTemp && args.waitPid) {
        WinSys::WaitForPid(args.waitPid, 15000);
    }

    const int code = args.headless ? RunHeadless(args) : RunGui(args);

    if (args.fromTemp) {
        CleanupTempCopyIfAny();
    }
    return code;
}
