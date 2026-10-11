#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "InstallOptions.h"

// The ONLY place in the installer that touches Win32 (WinSys.cpp includes <windows.h>);
// this header stays Windows-header-free so the rest of the installer doesn't drag in
// windows.h macros. Everything takes/returns UTF-8 std::string or std::filesystem::path;
// UTF-16 conversion happens only inside WinSys.cpp.
namespace IzekipSetup::WinSys {

std::wstring Widen(std::string_view utf8);
std::string Narrow(std::wstring_view wide);

std::vector<std::wstring> CommandLineArgsWithoutExe();
std::filesystem::path ExePath();
std::filesystem::path ExeDir();
// The library loads fonts/shaders/style.css relative to the CURRENT DIRECTORY. An
// uninstaller started from Windows "Apps & features" runs with CWD = System32, so the
// installer pins the CWD to its own folder at startup.
void SetCwdToExeDir();
bool IsElevated();
unsigned long CurrentPid();

enum class Folder { LocalAppData, ProgramFiles, Programs, CommonPrograms, Temp };
std::filesystem::path KnownFolder(Folder folder);

enum class LaunchResult { Ok, Cancelled, Failed };
LaunchResult Relaunch(const std::filesystem::path& exe, const std::wstring& args, bool elevate);
bool WaitForPid(unsigned long pid, unsigned long timeoutMs);

// Environment variables (persistent, registry-backed; scope picks HKCU vs HKLM).
std::optional<std::string> ReadEnv(Scope scope, const std::string& name);
bool WriteEnv(Scope scope, const std::string& name, const std::string& value);
bool DeleteEnv(Scope scope, const std::string& name);
bool PathContains(Scope scope, const std::filesystem::path& dir);
bool PathAppend(Scope scope, const std::filesystem::path& dir);
bool PathRemove(Scope scope, const std::filesystem::path& dir);
void BroadcastEnvChange();

bool CreateShortcut(const std::filesystem::path& lnk, const std::filesystem::path& target,
                    const std::string& description);
bool DeleteShortcut(const std::filesystem::path& lnk); // also removes its folder if empty

struct UninstallEntry {
    std::string displayName;
    std::string displayVersion;
    std::string publisher;
    std::filesystem::path installLocation;
    std::filesystem::path uninstallerExe; // UninstallString = "<exe>" --uninstall
    std::uint32_t estimatedSizeKB = 0;
};
bool WriteUninstallEntry(Scope scope, const UninstallEntry& entry);
bool DeleteUninstallEntry(Scope scope);
std::optional<std::filesystem::path> ReadInstalledLocation(Scope scope);

std::optional<std::filesystem::path> PickFolder(void* ownerHwnd, const std::filesystem::path& initial);
void OpenInExplorer(const std::filesystem::path& path);

// Deletes `dir` a few seconds after this process exits (a running exe can't delete itself).
void SpawnDelayedDirectoryRemoval(const std::filesystem::path& dir);

} // namespace IzekipSetup::WinSys
