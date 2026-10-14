#pragma once

// Internal to the library. The LRU policy is kept separate from the GPU code so both backends
// share it and it can be tested without a GPU.

#include <cstddef>
#include <list>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace IzekipUI::Render {

// Least-recently-used cache of GPU image textures keyed by file path. `Texture` is the
// backend's own handle type (it owns its GPU objects, so evicting an entry releases them).
template <typename Texture>
class ImageCache {
public:
    // At most 32 images and 64 MiB of decoded RGBA. A single 4096x4096 image is exactly 64 MiB.
    static constexpr std::size_t kMaxEntries = 32;
    static constexpr std::size_t kMaxBytes = 64u * 1024u * 1024u;
    // Paths that failed to decode are remembered so a broken file is not re-read every frame.
    // The set is bounded; when it fills, it is emptied and every failed path is tried again.
    static constexpr std::size_t kMaxFailedPaths = 256;

    // Returns the texture for `path` and marks it most recently used, or nullptr.
    Texture* Find(const std::string& path) {
        const auto it = m_entries.find(path);
        if (it == m_entries.end()) {
            return nullptr;
        }
        m_order.splice(m_order.begin(), m_order, it->second.position);
        return &it->second.texture;
    }

    // Stores `texture`, evicting least-recently-used entries until both limits hold. Returns
    // false when `bytes` alone exceeds the byte budget; the texture is then dropped.
    // Textures that leave the cache (evicted, or replaced) are moved into `released` when it is
    // given. A backend whose GPU objects must outlive the frame that used them keeps them there.
    bool Insert(const std::string& path, Texture texture, std::size_t bytes,
                std::vector<Texture>* released = nullptr) {
        if (bytes > kMaxBytes) {
            return false;
        }
        Erase(path, released); // replacing an existing entry must not double-count its bytes
        while (!m_order.empty() && (m_entries.size() >= kMaxEntries || m_bytes + bytes > kMaxBytes)) {
            const std::string victim = m_order.back(); // a copy: Erase destroys the list node
            Erase(victim, released);
        }
        m_order.push_front(path);
        m_entries.emplace(path, Entry{std::move(texture), bytes, m_order.begin()});
        m_bytes += bytes;
        return true;
    }

    bool IsFailed(const std::string& path) const { return m_failed.count(path) != 0; }

    void MarkFailed(const std::string& path) {
        if (m_failed.size() >= kMaxFailedPaths) {
            m_failed.clear();
        }
        m_failed.insert(path);
    }

    // Drops every texture and failure record. Used on device loss: the GPU objects belong to the
    // old device and the next frame must reload what it needs.
    void Clear(std::vector<Texture>* released = nullptr) {
        if (released) {
            for (auto& item : m_entries) {
                released->push_back(std::move(item.second.texture));
            }
        }
        m_entries.clear();
        m_order.clear();
        m_failed.clear();
        m_bytes = 0;
    }

    std::size_t Size() const { return m_entries.size(); }
    std::size_t Bytes() const { return m_bytes; }

private:
    struct Entry {
        Texture texture;
        std::size_t bytes;
        std::list<std::string>::iterator position;
    };

    void Erase(const std::string& path, std::vector<Texture>* released) {
        const auto it = m_entries.find(path);
        if (it == m_entries.end()) {
            return;
        }
        m_bytes -= it->second.bytes;
        m_order.erase(it->second.position);
        if (released) {
            released->push_back(std::move(it->second.texture));
        }
        m_entries.erase(it);
    }

    std::unordered_map<std::string, Entry> m_entries;
    std::list<std::string> m_order; // front = most recently used
    std::unordered_set<std::string> m_failed;
    std::size_t m_bytes = 0;
};

} // namespace IzekipUI::Render
