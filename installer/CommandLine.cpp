#include "CommandLine.h"

#include <cwchar>

namespace IzekipSetup {

namespace {

std::optional<bool> ParseBool(const std::wstring& value) {
    if (value == L"1" || value == L"true") return true;
    if (value == L"0" || value == L"false") return false;
    return std::nullopt;
}

std::wstring QuoteArg(const std::wstring& arg) {
    if (!arg.empty() && arg.find_first_of(L" \t\"") == std::wstring::npos) {
        return arg;
    }
    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (const wchar_t c : arg) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
        } else {
            out.append(backslashes, L'\\');
        }
        backslashes = 0;
        out.push_back(c);
    }
    out.append(backslashes * 2, L'\\'); // before the closing quote
    out.push_back(L'"');
    return out;
}

} // namespace

Args ParseArgs(const std::vector<std::wstring>& argv) {
    Args args;
    for (const std::wstring& a : argv) {
        const size_t eq = a.find(L'=');
        const std::wstring key = eq == std::wstring::npos ? a : a.substr(0, eq);
        const std::wstring value = eq == std::wstring::npos ? L"" : a.substr(eq + 1);

        if (key == L"--uninstall") args.mode = Mode::Uninstall;
        else if (key == L"--from-temp") args.fromTemp = true;
        else if (key == L"--auto-install") args.autoInstall = true;
        else if (key == L"--headless") args.headless = true;
        else if (key == L"--no-system") args.noSystem = true;
        else if (key == L"--scope") args.scope = (value == L"all") ? Scope::AllUsers : Scope::CurrentUser;
        else if (key == L"--prefix" && !value.empty()) args.prefix = std::filesystem::path(value);
        else if (key == L"--payload" && !value.empty()) args.payload = std::filesystem::path(value);
        else if (key == L"--backend") args.backend = (value == L"vulkan") ? Backend::Vulkan : Backend::D3D11;
        else if (key == L"--path") args.addToPath = ParseBool(value);
        else if (key == L"--env") args.setCmakeDirEnv = ParseBool(value);
        else if (key == L"--uninstall-entry") args.registerUninstall = ParseBool(value);
        else if (key == L"--shortcut") args.startMenuShortcut = ParseBool(value);
        else if (key == L"--wait-pid") args.waitPid = std::wcstoul(value.c_str(), nullptr, 10);
        // Unknown arguments are ignored.
    }
    return args;
}

std::wstring SerializeArgs(const Args& args) {
    std::vector<std::wstring> parts;
    if (args.mode == Mode::Uninstall) parts.push_back(L"--uninstall");
    if (args.fromTemp) parts.push_back(L"--from-temp");
    if (args.autoInstall) parts.push_back(L"--auto-install");
    if (args.headless) parts.push_back(L"--headless");
    if (args.noSystem) parts.push_back(L"--no-system");
    if (args.scope) parts.push_back(*args.scope == Scope::AllUsers ? L"--scope=all" : L"--scope=user");
    if (args.prefix) parts.push_back(L"--prefix=" + args.prefix->wstring());
    if (args.payload) parts.push_back(L"--payload=" + args.payload->wstring());
    if (args.backend) parts.push_back(*args.backend == Backend::Vulkan ? L"--backend=vulkan" : L"--backend=d3d11");
    auto boolArg = [&](const wchar_t* key, const std::optional<bool>& v) {
        if (v) parts.push_back(std::wstring(key) + (*v ? L"=1" : L"=0"));
    };
    boolArg(L"--path", args.addToPath);
    boolArg(L"--env", args.setCmakeDirEnv);
    boolArg(L"--uninstall-entry", args.registerUninstall);
    boolArg(L"--shortcut", args.startMenuShortcut);
    if (args.waitPid) parts.push_back(L"--wait-pid=" + std::to_wstring(args.waitPid));

    std::wstring out;
    for (const auto& p : parts) {
        if (!out.empty()) out.push_back(L' ');
        out += QuoteArg(p);
    }
    return out;
}

} // namespace IzekipSetup
