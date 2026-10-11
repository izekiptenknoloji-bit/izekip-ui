#include "WinSys.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <objbase.h>
#include <objidl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <cwctype>
#include <system_error>

namespace IzekipSetup::WinSys {

namespace {

const wchar_t* kUserEnvKey = L"Environment";
const wchar_t* kMachineEnvKey = L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
const wchar_t* kUninstallKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\IzekipUI";

HKEY RootFor(Scope scope) {
    return scope == Scope::AllUsers ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
}

const wchar_t* EnvKeyFor(Scope scope) {
    return scope == Scope::AllUsers ? kMachineEnvKey : kUserEnvKey;
}

// Raw registry string read (no %VAR% expansion). Returns the value type too.
std::optional<std::wstring> ReadRegString(HKEY root, const wchar_t* subKey, const wchar_t* name, DWORD* typeOut) {
    DWORD type = 0;
    DWORD bytes = 0;
    const DWORD flags = RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ | RRF_NOEXPAND | RRF_SUBKEY_WOW6464KEY;
    if (RegGetValueW(root, subKey, name, flags, &type, nullptr, &bytes) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    std::wstring value(bytes / sizeof(wchar_t), L'\0');
    if (RegGetValueW(root, subKey, name, flags, &type, value.data(), &bytes) != ERROR_SUCCESS) {
        return std::nullopt;
    }
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    if (typeOut) *typeOut = type;
    return value;
}

bool WriteRegString(HKEY root, const wchar_t* subKey, const wchar_t* name, const std::wstring& value, DWORD type) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(root, subKey, 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr, &key, nullptr) !=
        ERROR_SUCCESS) {
        return false;
    }
    const LONG r = RegSetValueExW(key, name, 0, type, reinterpret_cast<const BYTE*>(value.c_str()),
                                  static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return r == ERROR_SUCCESS;
}

bool WriteRegDword(HKEY key, const wchar_t* name, DWORD value) {
    return RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value)) ==
           ERROR_SUCCESS;
}

bool SetSz(HKEY key, const wchar_t* name, const std::wstring& value) {
    return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                          static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

// PATH-entry comparison: trimmed, trailing backslash ignored, case-insensitive.
std::wstring NormalizePathEntry(std::wstring entry) {
    while (!entry.empty() && std::iswspace(entry.front())) entry.erase(entry.begin());
    while (!entry.empty() && std::iswspace(entry.back())) entry.pop_back();
    while (entry.size() > 3 && (entry.back() == L'\\' || entry.back() == L'/')) entry.pop_back();
    for (auto& c : entry) c = static_cast<wchar_t>(std::towlower(c));
    return entry;
}

std::vector<std::wstring> SplitPath(const std::wstring& value) {
    std::vector<std::wstring> parts;
    size_t start = 0;
    while (start <= value.size()) {
        const size_t semi = value.find(L';', start);
        const std::wstring part = value.substr(start, semi == std::wstring::npos ? std::wstring::npos : semi - start);
        if (!NormalizePathEntry(part).empty()) parts.push_back(part);
        if (semi == std::wstring::npos) break;
        start = semi + 1;
    }
    return parts;
}

struct ComScope {
    HRESULT hr;
    ComScope() : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)) {}
    ~ComScope() {
        if (SUCCEEDED(hr)) CoUninitialize();
    }
};

} // namespace

std::wstring Widen(std::string_view utf8) {
    if (utf8.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
    return out;
}

std::string Narrow(std::wstring_view wide) {
    if (wide.empty()) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr,
                                      nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), n, nullptr, nullptr);
    return out;
}

std::vector<std::wstring> CommandLineArgsWithoutExe() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args;
    for (int i = 1; i < argc; ++i) {
        args.emplace_back(argv[i]);
    }
    LocalFree(argv);
    return args;
}

std::filesystem::path ExePath() {
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (n < buffer.size()) {
            buffer.resize(n);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
}

std::filesystem::path ExeDir() {
    return ExePath().parent_path();
}

void SetCwdToExeDir() {
    SetCurrentDirectoryW(ExeDir().c_str());
}

bool IsElevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

unsigned long CurrentPid() {
    return GetCurrentProcessId();
}

std::filesystem::path KnownFolder(Folder folder) {
    if (folder == Folder::Temp) {
        wchar_t buffer[MAX_PATH + 1];
        const DWORD n = GetTempPathW(MAX_PATH + 1, buffer);
        return std::filesystem::path(std::wstring(buffer, n));
    }
    const KNOWNFOLDERID* id = &FOLDERID_LocalAppData;
    switch (folder) {
        case Folder::LocalAppData: id = &FOLDERID_LocalAppData; break;
        case Folder::ProgramFiles: id = &FOLDERID_ProgramFiles; break;
        case Folder::Programs: id = &FOLDERID_Programs; break;
        case Folder::CommonPrograms: id = &FOLDERID_CommonPrograms; break;
        case Folder::Temp: break;
    }
    PWSTR raw = nullptr;
    std::filesystem::path result;
    if (SUCCEEDED(SHGetKnownFolderPath(*id, KF_FLAG_DEFAULT, nullptr, &raw))) {
        result = std::filesystem::path(raw);
    }
    CoTaskMemFree(raw);
    return result;
}

LaunchResult Relaunch(const std::filesystem::path& exe, const std::wstring& args, bool elevate) {
    const std::wstring file = exe.wstring();
    const std::wstring dir = exe.parent_path().wstring();
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.lpVerb = elevate ? L"runas" : L"open";
    info.lpFile = file.c_str();
    info.lpParameters = args.c_str();
    info.lpDirectory = dir.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (ShellExecuteExW(&info)) {
        return LaunchResult::Ok;
    }
    return GetLastError() == ERROR_CANCELLED ? LaunchResult::Cancelled : LaunchResult::Failed;
}

bool WaitForPid(unsigned long pid, unsigned long timeoutMs) {
    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!process) return true; // already gone
    const DWORD r = WaitForSingleObject(process, timeoutMs);
    CloseHandle(process);
    return r == WAIT_OBJECT_0;
}

std::optional<std::string> ReadEnv(Scope scope, const std::string& name) {
    const auto value = ReadRegString(RootFor(scope), EnvKeyFor(scope), Widen(name).c_str(), nullptr);
    if (!value) return std::nullopt;
    return Narrow(*value);
}

bool WriteEnv(Scope scope, const std::string& name, const std::string& value) {
    return WriteRegString(RootFor(scope), EnvKeyFor(scope), Widen(name).c_str(), Widen(value), REG_SZ);
}

bool DeleteEnv(Scope scope, const std::string& name) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(RootFor(scope), EnvKeyFor(scope), 0, KEY_SET_VALUE | KEY_WOW64_64KEY, &key) != ERROR_SUCCESS) {
        return false;
    }
    const LONG r = RegDeleteValueW(key, Widen(name).c_str());
    RegCloseKey(key);
    return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
}

bool PathContains(Scope scope, const std::filesystem::path& dir) {
    const auto value = ReadRegString(RootFor(scope), EnvKeyFor(scope), L"Path", nullptr);
    if (!value) return false;
    const std::wstring wanted = NormalizePathEntry(dir.wstring());
    for (const auto& part : SplitPath(*value)) {
        if (NormalizePathEntry(part) == wanted) return true;
    }
    return false;
}

bool PathAppend(Scope scope, const std::filesystem::path& dir) {
    DWORD type = REG_EXPAND_SZ;
    const auto value = ReadRegString(RootFor(scope), EnvKeyFor(scope), L"Path", &type);
    std::wstring current = value.value_or(L"");
    const std::wstring wanted = NormalizePathEntry(dir.wstring());
    for (const auto& part : SplitPath(current)) {
        if (NormalizePathEntry(part) == wanted) return true; // already there
    }
    while (!current.empty() && current.back() == L';') current.pop_back();
    if (!current.empty()) current.push_back(L';');
    current += dir.wstring();
    return WriteRegString(RootFor(scope), EnvKeyFor(scope), L"Path", current, value ? type : REG_EXPAND_SZ);
}

bool PathRemove(Scope scope, const std::filesystem::path& dir) {
    DWORD type = REG_EXPAND_SZ;
    const auto value = ReadRegString(RootFor(scope), EnvKeyFor(scope), L"Path", &type);
    if (!value) return true;
    const std::wstring wanted = NormalizePathEntry(dir.wstring());
    std::wstring rebuilt;
    for (const auto& part : SplitPath(*value)) {
        if (NormalizePathEntry(part) == wanted) continue;
        if (!rebuilt.empty()) rebuilt.push_back(L';');
        rebuilt += part;
    }
    return WriteRegString(RootFor(scope), EnvKeyFor(scope), L"Path", rebuilt, type);
}

void BroadcastEnvChange() {
    DWORD_PTR result = 0;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Environment"),
                        SMTO_ABORTIFHUNG, 5000, &result);
}

bool CreateShortcut(const std::filesystem::path& lnk, const std::filesystem::path& target,
                    const std::string& description) {
    ComScope com;
    std::error_code ec;
    std::filesystem::create_directories(lnk.parent_path(), ec);

    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&link)))) {
        return false;
    }
    link->SetPath(target.c_str());
    link->SetDescription(Widen(description).c_str());
    link->SetWorkingDirectory(target.c_str());

    IPersistFile* file = nullptr;
    bool ok = false;
    if (SUCCEEDED(link->QueryInterface(IID_PPV_ARGS(&file)))) {
        ok = SUCCEEDED(file->Save(lnk.c_str(), TRUE));
        file->Release();
    }
    link->Release();
    return ok;
}

bool DeleteShortcut(const std::filesystem::path& lnk) {
    std::error_code ec;
    std::filesystem::remove(lnk, ec);
    std::filesystem::remove(lnk.parent_path(), ec); // only succeeds if now empty
    return true;
}

bool WriteUninstallEntry(Scope scope, const UninstallEntry& entry) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(RootFor(scope), kUninstallKey, 0, nullptr, 0, KEY_SET_VALUE | KEY_WOW64_64KEY, nullptr,
                        &key, nullptr) != ERROR_SUCCESS) {
        return false;
    }
    const std::wstring exe = entry.uninstallerExe.wstring();
    bool ok = SetSz(key, L"DisplayName", Widen(entry.displayName));
    ok = SetSz(key, L"DisplayVersion", Widen(entry.displayVersion)) && ok;
    ok = SetSz(key, L"Publisher", Widen(entry.publisher)) && ok;
    ok = SetSz(key, L"InstallLocation", entry.installLocation.wstring()) && ok;
    ok = SetSz(key, L"UninstallString", L"\"" + exe + L"\" --uninstall") && ok;
    ok = SetSz(key, L"DisplayIcon", exe) && ok;
    ok = WriteRegDword(key, L"EstimatedSize", entry.estimatedSizeKB) && ok;
    ok = WriteRegDword(key, L"NoModify", 1) && ok;
    ok = WriteRegDword(key, L"NoRepair", 1) && ok;
    RegCloseKey(key);
    return ok;
}

bool DeleteUninstallEntry(Scope scope) {
    const LONG r = RegDeleteKeyExW(RootFor(scope), kUninstallKey, KEY_WOW64_64KEY, 0);
    return r == ERROR_SUCCESS || r == ERROR_FILE_NOT_FOUND;
}

std::optional<std::filesystem::path> ReadInstalledLocation(Scope scope) {
    const auto value = ReadRegString(RootFor(scope), kUninstallKey, L"InstallLocation", nullptr);
    if (!value || value->empty()) return std::nullopt;
    return std::filesystem::path(*value);
}

std::optional<std::filesystem::path> PickFolder(void* ownerHwnd, const std::filesystem::path& initial) {
    ComScope com;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return std::nullopt;
    }
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"Kurulum klasörünü seçin");

    std::error_code ec;
    std::filesystem::path start = initial;
    while (!start.empty() && !std::filesystem::exists(start, ec)) {
        const auto parent = start.parent_path();
        if (parent == start) break;
        start = parent;
    }
    IShellItem* startItem = nullptr;
    if (!start.empty() && SUCCEEDED(SHCreateItemFromParsingName(start.c_str(), nullptr, IID_PPV_ARGS(&startItem)))) {
        dialog->SetFolder(startItem);
        startItem->Release();
    }

    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(dialog->Show(static_cast<HWND>(ownerHwnd)))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = std::filesystem::path(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

void OpenInExplorer(const std::filesystem::path& path) {
    ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void SpawnDelayedDirectoryRemoval(const std::filesystem::path& dir) {
    wchar_t system[MAX_PATH];
    GetSystemDirectoryW(system, MAX_PATH);
    const std::wstring cmd = std::wstring(system) + L"\\cmd.exe";
    std::wstring line = L"\"" + cmd + L"\" /c ping -n 4 127.0.0.1 >nul & rmdir /s /q \"" + dir.wstring() + L"\"";

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(cmd.c_str(), line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si,
                       &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

} // namespace IzekipSetup::WinSys
