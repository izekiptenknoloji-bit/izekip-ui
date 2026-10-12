#include "Platform/SafeMode.h"

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <system_error>

namespace IzekipUI::Platform::SafeMode {

namespace {

bool g_active = false;
bool g_healthReported = false;

// Empty string when the variable is unset. MSVC's getenv is deprecated, so use _dupenv_s there.
std::string EnvironmentValue(const char* name) {
#if defined(_WIN32)
    char* value = nullptr;
    size_t length = 0;
    if (_dupenv_s(&value, &length, name) != 0 || value == nullptr) {
        return {};
    }
    std::string result(value);
    std::free(value);
    return result;
#else
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
#endif
}

std::filesystem::path DefaultStateDirectory() {
#if defined(_WIN32)
    const std::string base = EnvironmentValue("LOCALAPPDATA");
    if (base.empty()) {
        return {};
    }
    return std::filesystem::path(base) / "IzekipUI" / "state";
#else
    if (const std::string xdg = EnvironmentValue("XDG_STATE_HOME"); !xdg.empty()) {
        return std::filesystem::path(xdg) / "IzekipUI";
    }
    const std::string home = EnvironmentValue("HOME");
    if (home.empty()) {
        return {};
    }
    return std::filesystem::path(home) / ".local" / "state" / "IzekipUI";
#endif
}

// Application ids become file names: keep letters, digits, '-', '_' and '.' only.
std::string SanitizeAppId(const std::string& appId) {
    std::string clean;
    for (char c : appId) {
        const bool safe = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                          c == '-' || c == '_' || c == '.';
        clean.push_back(safe ? c : '_');
    }
    return clean.empty() ? std::string("app") : clean;
}

std::filesystem::path CounterFile(const std::filesystem::path& stateDirectory, const std::string& appId) {
    return stateDirectory / (SanitizeAppId(appId) + ".launches");
}

int ReadCount(const std::filesystem::path& file) {
    std::ifstream in(file);
    int count = 0;
    if (!(in >> count) || count < 0) {
        return 0; // missing or damaged file counts as no recorded failures
    }
    return count;
}

void WriteCount(const std::filesystem::path& file, int count) {
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    // Write a sibling temporary file, then rename over the old one, so a power cut leaves either
    // the old count or the new one.
    const std::filesystem::path temporary = file.string() + ".tmp";
    {
        std::ofstream out(temporary, std::ios::trunc);
        out << count << '\n';
    }
    std::filesystem::rename(temporary, file, ec);
}

} // namespace

bool BeginLaunchIn(const std::filesystem::path& stateDirectory, const std::string& appId) {
    if (stateDirectory.empty()) {
        return g_active;
    }
    const std::filesystem::path file = CounterFile(stateDirectory, appId);
    const int previousFailures = ReadCount(file);
    g_active = previousFailures >= kLaunchFailureThreshold;
    WriteCount(file, previousFailures + 1);
    return g_active;
}

void MarkHealthyIn(const std::filesystem::path& stateDirectory, const std::string& appId) {
    if (stateDirectory.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::remove(CounterFile(stateDirectory, appId), ec);
}

bool BeginLaunch(const std::string& appId) {
    if (EnvironmentValue("IZEKIP_SAFE_MODE") == "1") {
        g_active = true;
        return true;
    }
    try {
        return BeginLaunchIn(DefaultStateDirectory(), appId);
    } catch (const std::exception& error) {
        // Protection is best effort: a state directory problem must never stop the application.
        std::fprintf(stderr, "IzekipUI: crash-loop state unavailable (%s)\n", error.what());
        return g_active;
    }
}

void MarkHealthy(const std::string& appId) {
    if (g_healthReported) {
        return;
    }
    g_healthReported = true;
    try {
        MarkHealthyIn(DefaultStateDirectory(), appId);
    } catch (const std::exception&) {
        // Losing this write only means one extra launch is counted later.
    }
}

bool IsActive() {
    return g_active;
}

} // namespace IzekipUI::Platform::SafeMode
