#undef NDEBUG // tests must check their assertions in every configuration, including Release
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

#include "Platform/SafeMode.h"

using namespace IzekipUI::Platform;

namespace {

std::filesystem::path FreshDirectory(const char* name) {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(dir);
    return dir;
}

void TestThreeFailuresTriggerSafeModeAndHealthyResets() {
    const auto dir = FreshDirectory("izekip-safemode-test-a");
    const std::string app = "demo";
    assert(!SafeMode::BeginLaunchIn(dir, app)); // no failures yet
    assert(!SafeMode::BeginLaunchIn(dir, app)); // one failure recorded
    assert(!SafeMode::BeginLaunchIn(dir, app)); // two failures recorded
    assert(SafeMode::BeginLaunchIn(dir, app));  // three launches without a healthy frame: safe mode

    SafeMode::MarkHealthyIn(dir, app);
    assert(!SafeMode::BeginLaunchIn(dir, app)); // healthy launch cleared the count
    std::cout << "TestThreeFailuresTriggerSafeModeAndHealthyResets passed\n";
}

void TestApplicationIdCannotEscapeStateDirectory() {
    const auto dir = FreshDirectory("izekip-safemode-test-b");
    SafeMode::BeginLaunchIn(dir, "../evil");
    assert(std::filesystem::exists(dir / ".._evil.launches"));
    assert(!std::filesystem::exists(dir.parent_path() / "evil.launches"));
    std::cout << "TestApplicationIdCannotEscapeStateDirectory passed\n";
}

void TestMissingStateDirectoryIsHandledWithoutFiles() {
    assert(!SafeMode::BeginLaunchIn(std::filesystem::path(), "demo"));
    SafeMode::MarkHealthyIn(std::filesystem::path(), "demo");
    std::cout << "TestMissingStateDirectoryIsHandledWithoutFiles passed\n";
}

} // namespace

int main() {
    TestThreeFailuresTriggerSafeModeAndHealthyResets();
    TestApplicationIdCannotEscapeStateDirectory();
    TestMissingStateDirectoryIsHandledWithoutFiles();
    std::cout << "All SafeModeTests passed.\n";
    return 0;
}
