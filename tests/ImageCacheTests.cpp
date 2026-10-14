#undef NDEBUG // tests must check their assertions in every configuration, including Release
#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "Renderer/ImageCache.h"

using namespace IzekipUI::Render;

namespace {

// Stands in for a GPU texture: the cache only moves it around and drops it.
struct FakeTexture {
    int id = 0;
};

constexpr std::size_t kMiB = 1024u * 1024u;

void TestFindMissReturnsNull() {
    ImageCache<FakeTexture> cache;
    assert(cache.Find("a.png") == nullptr);
    assert(cache.Size() == 0 && cache.Bytes() == 0);
    std::cout << "TestFindMissReturnsNull passed\n";
}

void TestInsertThenFindReturnsSameTexture() {
    ImageCache<FakeTexture> cache;
    assert(cache.Insert("a.png", FakeTexture{7}, kMiB));
    FakeTexture* hit = cache.Find("a.png");
    assert(hit != nullptr && hit->id == 7);
    assert(cache.Bytes() == kMiB);
    std::cout << "TestInsertThenFindReturnsSameTexture passed\n";
}

void TestEvictsLeastRecentlyUsedFirst() {
    ImageCache<FakeTexture> cache;
    const std::size_t big = 30 * kMiB;
    assert(cache.Insert("a", FakeTexture{1}, big));
    assert(cache.Insert("b", FakeTexture{2}, big));
    assert(cache.Find("a") != nullptr); // touch a: b is now the least recently used
    assert(cache.Insert("c", FakeTexture{3}, big)); // 90 MiB would exceed 64 MiB: evict b
    assert(cache.Find("b") == nullptr);
    assert(cache.Find("a") != nullptr && cache.Find("c") != nullptr);
    assert(cache.Bytes() == 2 * big);
    std::cout << "TestEvictsLeastRecentlyUsedFirst passed\n";
}

void TestEntryCountLimitEvicts() {
    ImageCache<FakeTexture> cache;
    for (int i = 0; i < 32; ++i) {
        assert(cache.Insert("img" + std::to_string(i), FakeTexture{i}, 1024));
    }
    assert(cache.Size() == 32);
    assert(cache.Insert("img32", FakeTexture{32}, 1024)); // 33rd: the oldest (img0) goes
    assert(cache.Size() == 32);
    assert(cache.Find("img0") == nullptr);
    assert(cache.Find("img32") != nullptr);
    std::cout << "TestEntryCountLimitEvicts passed\n";
}

void TestOversizedSingleImageRejected() {
    ImageCache<FakeTexture> cache;
    assert(!cache.Insert("huge", FakeTexture{1}, ImageCache<FakeTexture>::kMaxBytes + 1));
    assert(cache.Find("huge") == nullptr);
    assert(cache.Size() == 0 && cache.Bytes() == 0);
    std::cout << "TestOversizedSingleImageRejected passed\n";
}

void TestReinsertReplacesWithoutDoubleCounting() {
    ImageCache<FakeTexture> cache;
    assert(cache.Insert("a", FakeTexture{1}, 10 * kMiB));
    assert(cache.Insert("a", FakeTexture{2}, 4 * kMiB));
    assert(cache.Size() == 1);
    assert(cache.Bytes() == 4 * kMiB);
    assert(cache.Find("a")->id == 2);
    std::cout << "TestReinsertReplacesWithoutDoubleCounting passed\n";
}

void TestFailedPathsAreRemembered() {
    ImageCache<FakeTexture> cache;
    assert(!cache.IsFailed("broken.png"));
    cache.MarkFailed("broken.png");
    assert(cache.IsFailed("broken.png"));
    std::cout << "TestFailedPathsAreRemembered passed\n";
}

void TestClearForgetsTexturesAndFailures() {
    ImageCache<FakeTexture> cache;
    assert(cache.Insert("a", FakeTexture{1}, kMiB));
    cache.MarkFailed("b");
    cache.Clear();
    assert(cache.Find("a") == nullptr);
    assert(!cache.IsFailed("b"));
    assert(cache.Size() == 0 && cache.Bytes() == 0);
    std::cout << "TestClearForgetsTexturesAndFailures passed\n";
}

void TestEvictedTexturesGoToReleasedList() {
    // The Vulkan backend must keep evicted images alive until the frame that used them is done,
    // so the cache hands them out instead of dropping them.
    ImageCache<FakeTexture> cache;
    const std::size_t big = 40 * kMiB;
    assert(cache.Insert("a", FakeTexture{1}, big));
    std::vector<FakeTexture> released;
    assert(cache.Insert("b", FakeTexture{2}, big, &released)); // 80 MiB > 64 MiB: evicts a
    assert(released.size() == 1 && released[0].id == 1);
    assert(cache.Find("a") == nullptr && cache.Find("b") != nullptr);

    std::vector<FakeTexture> cleared;
    cache.Clear(&cleared);
    assert(cleared.size() == 1 && cleared[0].id == 2);
    assert(cache.Size() == 0);
    std::cout << "TestEvictedTexturesGoToReleasedList passed\n";
}

} // namespace

int main() {
    TestFindMissReturnsNull();
    TestInsertThenFindReturnsSameTexture();
    TestEvictsLeastRecentlyUsedFirst();
    TestEntryCountLimitEvicts();
    TestOversizedSingleImageRejected();
    TestReinsertReplacesWithoutDoubleCounting();
    TestFailedPathsAreRemembered();
    TestClearForgetsTexturesAndFailures();
    TestEvictedTexturesGoToReleasedList();
    std::cout << "ImageCacheTests: all passed\n";
    return 0;
}
