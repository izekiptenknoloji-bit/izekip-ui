#pragma once

#include <cstdint>

namespace IzekipUI::Platform::Diagnostics {

// Counters the library keeps for memory and overload diagnosis. Cheap enough to be always on;
// the periodic log line is opt-in.
enum class Counter {
    LiveWidgets,        // widgets currently alive (created minus destroyed)
    StyleApplies,       // StyleApplier::Apply calls
    DrawBudgetExceeded, // draws dropped because a frame exceeded the per-frame budget
    DeviceRecoveries,   // successful device rebuilds after a loss or a failed submit
    TextRejected,       // text edits or input refused because a text limit was reached
    OutOfMemory,        // frames that ended with std::bad_alloc or a GPU out-of-memory result
    WidgetRejected,     // AddChild refused: invalid child, cycle, or live-widget limit
    Count
};

void Increment(Counter counter);
void Decrement(Counter counter);
std::uint64_t Get(Counter counter);

struct ProcessMemory {
    std::uint64_t residentBytes = 0; // working set (Windows) or VmRSS (Linux)
    std::uint64_t privateBytes = 0;  // committed private bytes (Windows) or RssAnon (Linux)
};

// Degraded mode: entered after an out-of-memory frame or result, and left after a run of healthy
// frames. While degraded, optional memory-heavy effects (backdrop blur) are skipped.
void EnterDegradedMode();
bool IsDegraded();
void NoteHealthyFrame();

// Zeros when the platform cannot report it.
ProcessMemory ReadProcessMemory();

// Writes one line to stderr at most once a minute, and only when IZEKIP_DIAG=1. Call once per
// presented frame; the cost between log lines is a clock read.
void MaybeLogPeriodically();

} // namespace IzekipUI::Platform::Diagnostics
