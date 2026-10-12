#include "Platform/Diagnostics.h"

#include <array>
#include <cstddef>
#include <chrono>
#include <exception>
#include <cstdio>
#include <fstream>
#include <string>

#include "Core/Environment.h"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace IzekipUI::Platform::Diagnostics {

namespace {

constexpr std::size_t kCounterCount = static_cast<std::size_t>(Counter::Count);
std::array<std::uint64_t, kCounterCount> g_counters{};

constexpr auto kLogInterval = std::chrono::minutes(1);

}  // namespace

void Increment(Counter counter) {
    ++g_counters[static_cast<std::size_t>(counter)];
}

void Decrement(Counter counter) {
    auto& value = g_counters[static_cast<std::size_t>(counter)];
    if (value > 0) {
        --value;
    }
}

std::uint64_t Get(Counter counter) {
    return g_counters[static_cast<std::size_t>(counter)];
}

ProcessMemory ReadProcessMemory() {
    ProcessMemory memory;
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS_EX counters{};
    counters.cb = sizeof(counters);
    if (K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PPROCESS_MEMORY_COUNTERS>(&counters),
                                sizeof(counters))) {
        memory.residentBytes = static_cast<std::uint64_t>(counters.WorkingSetSize);
        memory.privateBytes = static_cast<std::uint64_t>(counters.PrivateUsage);
    }
#else
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        // Lines look like "VmRSS:    12345 kB"; the value is in kibibytes.
        const auto parseKilobytes = [&line]() -> std::uint64_t {
            try {
                return std::stoull(line.substr(line.find(':') + 1)) * 1024ULL;
            } catch (const std::exception&) {
                return 0ULL; // unexpected format: report nothing rather than fail
            }
        };
        if (line.rfind("VmRSS:", 0) == 0) {
            memory.residentBytes = parseKilobytes();
        } else if (line.rfind("RssAnon:", 0) == 0) {
            memory.privateBytes = parseKilobytes();
        }
    }
#endif
    return memory;
}

void MaybeLogPeriodically() {
    static const bool enabled = EnvironmentValue("IZEKIP_DIAG") == "1";
    if (!enabled) {
        return;
    }
    static auto lastLog = std::chrono::steady_clock::now() - kLogInterval;
    const auto now = std::chrono::steady_clock::now();
    if (now - lastLog < kLogInterval) {
        return;
    }
    lastLog = now;

    const ProcessMemory memory = ReadProcessMemory();
    std::fprintf(stderr,
                 "IzekipUI diag: rss=%llu MB private=%llu MB widgets=%llu restyles=%llu "
                 "draws_dropped=%llu recoveries=%llu\n",
                 static_cast<unsigned long long>(memory.residentBytes / (1024 * 1024)),
                 static_cast<unsigned long long>(memory.privateBytes / (1024 * 1024)),
                 static_cast<unsigned long long>(Get(Counter::LiveWidgets)),
                 static_cast<unsigned long long>(Get(Counter::StyleApplies)),
                 static_cast<unsigned long long>(Get(Counter::DrawBudgetExceeded)),
                 static_cast<unsigned long long>(Get(Counter::DeviceRecoveries)));
}

}  // namespace IzekipUI::Platform::Diagnostics
