#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "InstallOptions.h"
#include "Manifest.h"

namespace IzekipSetup {

// UI-free, thread-free install/uninstall state machine. The GUI calls Step() once per frame
// with a small time budget (so the window keeps repainting and the progress bar moves); the
// headless mode just calls it in a loop. Uninstall is driven entirely by the manifest the
// install wrote -- it never deletes anything it didn't record (no remove_all(prefix)).
class InstallEngine {
public:
    struct Status {
        float progress = 0.0f;
        std::string message;
        bool done = false;
        bool failed = false;
        std::string error;
    };

    // selfDir = folder holding the installer exe + its runtime assets (copied into
    // <prefix>\uninstall so the uninstaller can run later). noSystem = files + manifest only.
    bool BeginInstall(const InstallOptions& options, const std::filesystem::path& payloadRoot,
                      const std::filesystem::path& selfDir, const std::string& version, bool noSystem,
                      std::string* error);
    bool BeginUninstall(const std::filesystem::path& prefix, bool noSystem, std::string* error);

    const Status& Step(std::chrono::milliseconds budget);
    const Status& GetStatus() const { return m_status; }
    bool IsRunning() const { return m_phase != Phase::Idle && m_phase != Phase::Finished; }

    // Best-effort cleanup after a failed install: deletes only the files this run copied.
    void RollbackCopiedFiles();

    static std::filesystem::path ManifestPath(const std::filesystem::path& prefix);
    static const std::vector<std::string>& SelfAssetNames();

private:
    enum class Phase {
        Idle,
        Copy,
        UpgradeCleanup,
        SelfCopy,
        SystemPath,
        SystemEnv,
        SystemShortcut,
        SystemUninstallEntry,
        UninstallFiles,
        UninstallSystem,
        UninstallFinalize,
        Finished
    };

    bool StepOnce();
    void Fail(const std::string& error);
    bool SaveCurrentManifest();
    void RemoveEmptyDirectories(const std::filesystem::path& root);

    Phase m_phase = Phase::Idle;
    Status m_status;
    InstallOptions m_options;
    std::filesystem::path m_payloadDir;
    std::filesystem::path m_selfDir;
    bool m_noSystem = false;
    bool m_envChanged = false;

    std::vector<std::filesystem::path> m_work; // relative paths to copy / delete
    size_t m_index = 0;
    std::uint64_t m_totalBytes = 0;
    std::uint64_t m_doneBytes = 0;
    std::vector<std::string> m_copied;
    std::vector<std::string> m_oldFiles; // from a previous install at the same prefix

    InstallManifest m_manifest;
};

} // namespace IzekipSetup
