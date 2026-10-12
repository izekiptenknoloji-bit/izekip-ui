#pragma once

#include <cstdlib>
#include <string>

namespace IzekipUI {

// Empty string when the variable is unset. MSVC's getenv is deprecated, so use _dupenv_s there.
inline std::string EnvironmentValue(const char* name) {
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

} // namespace IzekipUI
