#include "InstallEngine.h"

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <system_error>

#include "WinSys.h"

namespace fs = std::filesystem;

namespace IzekipSetup {

namespace {

std::string RelString(const fs::path& rel) {
    const std::u8string s = rel.generic_u8string();
    return std::string(s.begin(), s.end());
}

std::wstring NormalizeForCompare(const fs::path& p) {
    std::wstring s = p.lexically_normal().wstring();
    while (s.size() > 3 && (s.back() == L'\\' || s.back() == L'/')) s.pop_back();
    for (auto& c : s) c = static_cast<wchar_t>(std::towlower(c));
    return s;
}

bool SamePath(const fs::path& a, const fs::path& b) {
    return NormalizeForCompare(a) == NormalizeForCompare(b);
}

std::string ErrorCodeText(const std::error_code& ec) {
    // ec.message() is in the system ANSI code page on MSVC -- not safe to show as UTF-8.
    return "hata kodu " + std::to_string(ec.value());
}

constexpr const char* kEnvName = "IzekipUI_DIR";

} // namespace

fs::path InstallEngine::ManifestPath(const fs::path& prefix) {
    return prefix / "uninstall" / kManifestFileName;
}

const std::vector<std::string>& InstallEngine::SelfAssetNames() {
    static const std::vector<std::string> names = {"IzekipUISetup.exe", "Inter.ttf", "SpaceMono-Regular.ttf",
                                                   "RectShader.hlsl", "setup.css"};
    return names;
}

bool InstallEngine::BeginInstall(const InstallOptions& options, const fs::path& payloadRoot, const fs::path& selfDir,
                                 const std::string& version, bool noSystem, std::string* error) {
    *this = InstallEngine{};
    m_options = options;
    m_selfDir = selfDir;
    m_noSystem = noSystem;

    auto reject = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };

    const fs::path& prefix = options.prefix;
    if (prefix.empty() || !prefix.is_absolute()) {
        return reject("Lütfen tam bir klasör yolu girin (örneğin C:\\Kurulum\\IzekipUI).");
    }
    if (prefix.lexically_normal() == prefix.root_path()) {
        return reject("Bir sürücünün köküne kurulum yapılamaz; bir alt klasör seçin.");
    }

    m_payloadDir = payloadRoot / BackendDirName(options.backend);
    std::error_code ec;
    if (!fs::is_directory(m_payloadDir, ec)) {
        return reject("Kurulum dosyaları bulunamadı (payload\\" + std::string(BackendDirName(options.backend)) +
                      "). Lütfen önce zip arşivini bir klasöre çıkarın ve kurulumu oradan çalıştırın.");
    }

    if (!noSystem) {
        if (auto existing = WinSys::ReadInstalledLocation(options.scope); existing && !SamePath(*existing, prefix)) {
            return reject("Bu kapsam için başka bir konumda zaten bir İzekip UI SDK kurulumu var:\n" +
                          PathToUtf8(*existing) + "\nÖnce onu kaldırın.");
        }
    }

    // Same prefix -> upgrade: keep what the previous install did to the system, so the
    // system steps below can update/undo it, and remember its file list for cleanup.
    if (auto old = LoadManifest(ManifestPath(prefix))) {
        m_manifest = *old;
        m_oldFiles = old->files;
    }
    m_manifest.version = version;
    m_manifest.scope = options.scope;
    m_manifest.prefix = prefix;
    m_manifest.backend = options.backend;
    m_manifest.files.clear();
    m_manifest.bytes = 0;

    const std::string permissionHint =
        options.scope == Scope::CurrentUser
            ? "Bu klasöre yazma izniniz yok. \"Tüm kullanıcılar\" seçeneğini ya da başka bir klasörü deneyin."
            : "Bu klasöre yazılamadı.";
    fs::create_directories(prefix, ec);
    if (ec) {
        return reject(permissionHint);
    }
    const fs::path probe = prefix / ".izekip-write-test";
    {
        std::ofstream file(probe, std::ios::binary);
        if (!file) {
            return reject(permissionHint);
        }
    }
    fs::remove(probe, ec);

    for (auto it = fs::recursive_directory_iterator(m_payloadDir, ec); !ec && it != fs::recursive_directory_iterator();
         it.increment(ec)) {
        if (it->is_regular_file(ec)) {
            m_work.push_back(it->path().lexically_relative(m_payloadDir));
            m_totalBytes += it->file_size(ec);
        }
    }
    if (m_work.empty()) {
        return reject("Kurulum dosyaları boş (payload\\" + std::string(BackendDirName(options.backend)) + ").");
    }

    m_phase = Phase::Copy;
    m_status.message = "Hazırlanıyor...";
    return true;
}

bool InstallEngine::BeginUninstall(const fs::path& prefix, bool noSystem, std::string* error) {
    *this = InstallEngine{};
    m_noSystem = noSystem;

    auto manifest = LoadManifest(ManifestPath(prefix));
    if (!manifest) {
        if (error) *error = "Kurulum bilgisi bulunamadı: " + PathToUtf8(ManifestPath(prefix));
        return false;
    }
    m_manifest = *manifest;
    m_options.prefix = prefix;
    m_options.scope = manifest->scope;
    for (const auto& f : manifest->files) {
        m_work.push_back(PathFromUtf8(f));
    }
    m_phase = Phase::UninstallFiles;
    m_status.message = "Kaldırılıyor...";
    return true;
}

const InstallEngine::Status& InstallEngine::Step(std::chrono::milliseconds budget) {
    const auto start = std::chrono::steady_clock::now();
    while (IsRunning()) {
        if (!StepOnce()) break;
        if (std::chrono::steady_clock::now() - start >= budget) break;
    }
    return m_status;
}

void InstallEngine::Fail(const std::string& error) {
    m_status.failed = true;
    m_status.done = true;
    m_status.error = error;
    m_phase = Phase::Finished;
}

bool InstallEngine::SaveCurrentManifest() {
    if (!SaveManifest(ManifestPath(m_options.prefix), m_manifest)) {
        Fail("Kurulum bilgisi (manifest) yazılamadı.");
        return false;
    }
    return true;
}

bool InstallEngine::StepOnce() {
    const fs::path& prefix = m_options.prefix;
    std::error_code ec;

    switch (m_phase) {
        case Phase::Copy: {
            if (m_index >= m_work.size()) {
                m_phase = Phase::UpgradeCleanup;
                return true;
            }
            const fs::path& rel = m_work[m_index];
            const fs::path src = m_payloadDir / rel;
            const fs::path dst = prefix / rel;
            fs::create_directories(dst.parent_path(), ec);
            fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            const std::string relStr = RelString(rel);
            if (ec) {
                Fail("Dosya kopyalanamadı: " + relStr + " (" + ErrorCodeText(ec) + ")");
                return false;
            }
            const std::uint64_t size = fs::file_size(src, ec);
            m_copied.push_back(relStr);
            m_manifest.files.push_back(relStr);
            m_manifest.bytes += size;
            m_doneBytes += size;
            ++m_index;
            m_status.progress = m_totalBytes ? 0.9f * static_cast<float>(m_doneBytes) / static_cast<float>(m_totalBytes)
                                             : 0.9f;
            m_status.message = "Kopyalanıyor: " + relStr;
            return true;
        }

        case Phase::UpgradeCleanup: {
            const std::unordered_set<std::string> fresh(m_manifest.files.begin(), m_manifest.files.end());
            for (const auto& old : m_oldFiles) {
                if (!fresh.count(old)) {
                    fs::remove(prefix / PathFromUtf8(old), ec);
                }
            }
            m_phase = Phase::SelfCopy;
            return true;
        }

        case Phase::SelfCopy: {
            const fs::path uninstallDir = prefix / "uninstall";
            fs::create_directories(uninstallDir, ec);
            for (const auto& name : SelfAssetNames()) {
                const fs::path src = m_selfDir / name;
                const fs::path dst = uninstallDir / name;
                if (fs::exists(dst, ec) && fs::equivalent(src, dst, ec)) {
                    continue; // running from the installed copy itself
                }
                fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                if (ec) {
                    Fail("Kaldırıcı dosyası kopyalanamadı: " + name + " (" + ErrorCodeText(ec) + ")");
                    return false;
                }
            }
            if (!SaveCurrentManifest()) return false;
            m_status.progress = 0.92f;
            m_status.message = "Sistem ayarları yapılıyor...";
            if (m_noSystem) {
                m_phase = Phase::Finished;
                m_status.done = true;
                m_status.progress = 1.0f;
                m_status.message = "Kurulum tamamlandı.";
                return false;
            }
            m_phase = Phase::SystemPath;
            return true;
        }

        case Phase::SystemPath: {
            const fs::path bin = (prefix / "bin").make_preferred();
            if (m_options.addToPath) {
                if (!WinSys::PathContains(m_options.scope, bin)) {
                    if (!WinSys::PathAppend(m_options.scope, bin)) {
                        Fail("PATH ortam değişkeni güncellenemedi.");
                        return false;
                    }
                    m_manifest.pathAdded = true;
                    m_envChanged = true;
                }
            } else if (m_manifest.pathAdded) {
                WinSys::PathRemove(m_options.scope, bin);
                m_manifest.pathAdded = false;
                m_envChanged = true;
            }
            if (!SaveCurrentManifest()) return false;
            m_status.progress = 0.94f;
            m_phase = Phase::SystemEnv;
            return true;
        }

        case Phase::SystemEnv: {
            const std::string value = PathToUtf8((prefix / "lib" / "cmake" / "IzekipUI").make_preferred());
            if (m_options.setCmakeDirEnv) {
                if (!m_manifest.envSet) {
                    const auto previous = WinSys::ReadEnv(m_options.scope, kEnvName);
                    m_manifest.envPrevPresent = previous.has_value();
                    m_manifest.envPrev = previous.value_or("");
                }
                if (!WinSys::WriteEnv(m_options.scope, kEnvName, value)) {
                    Fail("IzekipUI_DIR ortam değişkeni ayarlanamadı.");
                    return false;
                }
                m_manifest.envSet = true;
                m_manifest.envValue = value;
                m_envChanged = true;
            } else if (m_manifest.envSet) {
                const auto current = WinSys::ReadEnv(m_options.scope, kEnvName);
                if (current && *current == m_manifest.envValue) {
                    if (m_manifest.envPrevPresent) {
                        WinSys::WriteEnv(m_options.scope, kEnvName, m_manifest.envPrev);
                    } else {
                        WinSys::DeleteEnv(m_options.scope, kEnvName);
                    }
                }
                m_manifest.envSet = false;
                m_envChanged = true;
            }
            if (!SaveCurrentManifest()) return false;
            m_status.progress = 0.96f;
            m_phase = Phase::SystemShortcut;
            return true;
        }

        case Phase::SystemShortcut: {
            const WinSys::Folder folder =
                m_options.scope == Scope::AllUsers ? WinSys::Folder::CommonPrograms : WinSys::Folder::Programs;
            const fs::path lnk = WinSys::KnownFolder(folder) / PathFromUtf8("İzekip UI") / PathFromUtf8("İzekip UI SDK.lnk");
            if (m_options.startMenuShortcut) {
                if (!WinSys::CreateShortcut(lnk, prefix, "İzekip UI SDK klasörü")) {
                    Fail("Başlat menüsü kısayolu oluşturulamadı.");
                    return false;
                }
                m_manifest.shortcut = lnk;
            } else if (!m_manifest.shortcut.empty()) {
                WinSys::DeleteShortcut(m_manifest.shortcut);
                m_manifest.shortcut.clear();
            }
            if (!SaveCurrentManifest()) return false;
            m_status.progress = 0.98f;
            m_phase = Phase::SystemUninstallEntry;
            return true;
        }

        case Phase::SystemUninstallEntry: {
            if (m_options.registerUninstall) {
                WinSys::UninstallEntry entry;
                entry.displayName = "İzekip UI SDK " + m_manifest.version;
                entry.displayVersion = m_manifest.version;
                entry.publisher = "İzekip Teknoloji";
                entry.installLocation = prefix;
                entry.uninstallerExe = (prefix / "uninstall" / "IzekipUISetup.exe").make_preferred();
                entry.estimatedSizeKB = static_cast<std::uint32_t>(m_manifest.bytes / 1024);
                if (!WinSys::WriteUninstallEntry(m_options.scope, entry)) {
                    Fail("Programlar ve Özellikler kaydı oluşturulamadı.");
                    return false;
                }
                m_manifest.uninstallRegistered = true;
            } else if (m_manifest.uninstallRegistered) {
                WinSys::DeleteUninstallEntry(m_options.scope);
                m_manifest.uninstallRegistered = false;
            }
            if (!SaveCurrentManifest()) return false;
            if (m_envChanged) {
                WinSys::BroadcastEnvChange();
            }
            m_phase = Phase::Finished;
            m_status.done = true;
            m_status.progress = 1.0f;
            m_status.message = "Kurulum tamamlandı.";
            return false;
        }

        case Phase::UninstallFiles: {
            if (m_index >= m_work.size()) {
                m_phase = m_noSystem ? Phase::UninstallFinalize : Phase::UninstallSystem;
                return true;
            }
            const fs::path& rel = m_work[m_index];
            fs::remove(prefix / rel, ec); // already-missing files are fine
            ++m_index;
            m_status.progress = 0.85f * static_cast<float>(m_index) / static_cast<float>(m_work.size());
            m_status.message = "Siliniyor: " + RelString(rel);
            return true;
        }

        case Phase::UninstallSystem: {
            bool envChanged = false;
            if (!m_manifest.shortcut.empty()) {
                WinSys::DeleteShortcut(m_manifest.shortcut);
            }
            if (m_manifest.pathAdded) {
                WinSys::PathRemove(m_options.scope, (prefix / "bin").make_preferred());
                envChanged = true;
            }
            if (m_manifest.envSet) {
                const auto current = WinSys::ReadEnv(m_options.scope, kEnvName);
                if (current && *current == m_manifest.envValue) { // untouched since we set it
                    if (m_manifest.envPrevPresent) {
                        WinSys::WriteEnv(m_options.scope, kEnvName, m_manifest.envPrev);
                    } else {
                        WinSys::DeleteEnv(m_options.scope, kEnvName);
                    }
                    envChanged = true;
                }
            }
            if (m_manifest.uninstallRegistered) {
                WinSys::DeleteUninstallEntry(m_options.scope);
            }
            if (envChanged) {
                WinSys::BroadcastEnvChange();
            }
            m_status.progress = 0.92f;
            m_phase = Phase::UninstallFinalize;
            return true;
        }

        case Phase::UninstallFinalize: {
            const fs::path uninstallDir = prefix / "uninstall";
            for (const auto& name : SelfAssetNames()) {
                fs::remove(uninstallDir / name, ec);
            }
            fs::remove(uninstallDir / kManifestFileName, ec);
            fs::remove(uninstallDir / (std::string(kManifestFileName) + ".tmp"), ec);
            RemoveEmptyDirectories(prefix);
            m_phase = Phase::Finished;
            m_status.done = true;
            m_status.progress = 1.0f;
            m_status.message = "Kaldırma tamamlandı.";
            return false;
        }

        case Phase::Idle:
        case Phase::Finished:
            return false;
    }
    return false;
}

void InstallEngine::RemoveEmptyDirectories(const fs::path& root) {
    std::error_code ec;
    std::vector<fs::path> dirs;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (it->is_directory(ec)) {
            dirs.push_back(it->path());
        }
    }
    // Deepest first. fs::remove only deletes a directory if it is EMPTY, so anything the
    // user put there themselves survives -- this is never a remove_all.
    std::sort(dirs.begin(), dirs.end(), [](const fs::path& a, const fs::path& b) {
        return a.native().size() > b.native().size();
    });
    for (const auto& dir : dirs) {
        fs::remove(dir, ec);
    }
    fs::remove(root, ec);
}

void InstallEngine::RollbackCopiedFiles() {
    std::error_code ec;
    for (const auto& rel : m_copied) {
        fs::remove(m_options.prefix / PathFromUtf8(rel), ec);
    }
    m_copied.clear();
    RemoveEmptyDirectories(m_options.prefix);
}

} // namespace IzekipSetup
