#pragma once

#include <filesystem>
#include <string>

namespace IzekipUI::Platform::SafeMode {

// Crash-loop protection, per application. Every launch that starts the renderer is counted in a
// small per-user state file; MarkHealthy() clears the count after the first presented frame. When
// the last kLaunchFailureThreshold launches never reached a presented frame, the next launch runs
// in safe mode: the D3D11 backend uses the WARP software rasterizer, so a broken GPU driver can no
// longer take the application down. The environment variable IZEKIP_SAFE_MODE=1 forces safe mode.
inline constexpr int kLaunchFailureThreshold = 3;

// Records one launch of `appId` and returns true when this launch must run in safe mode.
bool BeginLaunch(const std::string& appId);

// Clears the failure count for `appId`. Safe to call on every frame; only the first call writes.
void MarkHealthy(const std::string& appId);

// True when the current process was started in safe mode.
bool IsActive();

// Same as above but with an explicit state directory (used by tests and tooling).
bool BeginLaunchIn(const std::filesystem::path& stateDirectory, const std::string& appId);
void MarkHealthyIn(const std::filesystem::path& stateDirectory, const std::string& appId);

} // namespace IzekipUI::Platform::SafeMode
