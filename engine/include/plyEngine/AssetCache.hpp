#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace ply {

/// A shared, reference-counted handle to a loaded asset.
/// The asset stays alive while at least one handle exists.
template <typename T>
class AssetHandle {
public:
    AssetHandle() = default;
    explicit AssetHandle(std::shared_ptr<const T> asset) : asset_(std::move(asset)) {}

    bool valid() const { return static_cast<bool>(asset_); }
    explicit operator bool() const { return valid(); }

    const T* get() const { return asset_.get(); }
    const T* operator->() const { return asset_.get(); }
    const T& operator*() const { return *asset_; }

    /// How many handles share this asset (including this one).
    long useCount() const { return asset_.use_count(); }

private:
    std::shared_ptr<const T> asset_;
};

/// A path-keyed asset cache with real reference counting: an asset is loaded
/// once, shared by every handle, and released when the last handle dies.
///
/// The cache stores weak references, so it never keeps assets alive on its own.
/// Expired entries are purged lazily on the next load().
template <typename T>
class AssetCache {
public:
    /// Produces the asset for a path, or nullptr on failure.
    using Loader = std::function<std::shared_ptr<T>(const std::string& path)>;

    void setLoader(Loader loader) { loader_ = std::move(loader); }
    const Loader& loader() const { return loader_; }

    /// Returns a handle, loading (or reusing) the asset as needed.
    /// Returns an invalid handle if there is no loader or loading failed.
    AssetHandle<T> load(const std::string& path) {
        purgeExpired();

        auto it = cache_.find(path);
        if (it != cache_.end()) {
            if (auto existing = it->second.lock()) {
                return AssetHandle<T>(std::move(existing));
            }
            cache_.erase(it);
        }

        if (!loader_) {
            return {};
        }
        std::shared_ptr<T> loaded = loader_(path);
        if (!loaded) {
            return {};
        }
        cache_.emplace(path, loaded);
        return AssetHandle<T>(std::move(loaded));
    }

    /// Number of assets currently alive through handles.
    size_t residentCount() const {
        size_t live = 0;
        for (const auto& entry : cache_) {
            if (!entry.second.expired()) {
                ++live;
            }
        }
        return live;
    }

    /// Number of path entries tracked (including expired ones not yet purged).
    size_t entryCount() const { return cache_.size(); }

    /// Drops all bookkeeping. Live handles keep their assets.
    void clear() { cache_.clear(); }

private:
    void purgeExpired() {
        for (auto it = cache_.begin(); it != cache_.end();) {
            if (it->second.expired()) {
                it = cache_.erase(it);
            } else {
                ++it;
            }
        }
    }

    Loader loader_;
    std::unordered_map<std::string, std::weak_ptr<T>> cache_;
};

}  // namespace ply
