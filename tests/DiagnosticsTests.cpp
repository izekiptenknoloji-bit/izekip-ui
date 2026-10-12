#undef NDEBUG // tests must check their assertions in every configuration, including Release
#include <cassert>
#include <iostream>
#include <memory>

#include "Core/Widget.h"
#include "Platform/Diagnostics.h"

using namespace IzekipUI;
using namespace IzekipUI::Platform::Diagnostics;

namespace {

void TestLiveWidgetCounterFollowsLifetime() {
    const std::uint64_t before = Get(Counter::LiveWidgets);
    {
        auto a = std::make_shared<Widget>("Panel");
        auto b = std::make_shared<Widget>("Panel");
        assert(Get(Counter::LiveWidgets) == before + 2);
    }
    assert(Get(Counter::LiveWidgets) == before);
    std::cout << "TestLiveWidgetCounterFollowsLifetime passed\n";
}

void TestProcessMemoryIsReported() {
    const ProcessMemory memory = ReadProcessMemory();
    // A running test process always has resident pages; zero means the platform read failed.
    assert(memory.residentBytes > 0);
    std::cout << "TestProcessMemoryIsReported passed (rss " << memory.residentBytes / (1024 * 1024)
              << " MB)\n";
}

} // namespace

int main() {
    TestLiveWidgetCounterFollowsLifetime();
    TestProcessMemoryIsReported();
    std::cout << "All DiagnosticsTests passed.\n";
    return 0;
}
