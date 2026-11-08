// Headless asset-cache tests: no GL context required.

#include "plyEngine/AssetCache.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

// Fake asset that counts its own lifetime.
struct FakeAsset {
    static inline int alive = 0;
    static inline int created = 0;

    std::string path;
    explicit FakeAsset(std::string p) : path(std::move(p)) {
        ++alive;
        ++created;
    }
    ~FakeAsset() { --alive; }
};

std::shared_ptr<FakeAsset> makeLoader(const std::string& path) {
    return std::make_shared<FakeAsset>(path);
}

}  // namespace

int main() {
    // --- loading the same path twice returns the same object ---------------
    {
        FakeAsset::alive = 0;
        FakeAsset::created = 0;

        ply::AssetCache<FakeAsset> cache;
        cache.setLoader(makeLoader);

        auto a = cache.load("textures/a.png");
        auto b = cache.load("textures/a.png");

        check(a.valid() && b.valid(), "both handles valid");
        check(a.get() == b.get(), "same path yields the same asset instance");
        check(FakeAsset::created == 1, "loader called only once for a path");
        check(cache.residentCount() == 1, "one resident asset");

        // Shares the asset: a, b, plus the weak_ptr does not count.
        check(a.useCount() == 2, "two handles share the asset");
    }

    // --- distinct paths load distinct assets -------------------------------
    {
        FakeAsset::alive = 0;
        FakeAsset::created = 0;

        ply::AssetCache<FakeAsset> cache;
        cache.setLoader(makeLoader);

        auto a = cache.load("a.png");
        auto b = cache.load("b.png");
        check(a.get() != b.get(), "different paths differ");
        check(FakeAsset::created == 2, "loader called per distinct path");
        check(cache.residentCount() == 2, "two resident assets");
    }

    // --- dropping the last handle releases the asset -----------------------
    {
        FakeAsset::alive = 0;
        FakeAsset::created = 0;

        ply::AssetCache<FakeAsset> cache;
        cache.setLoader(makeLoader);

        {
            auto handle = cache.load("a.png");
            check(handle.valid(), "handle valid in scope");
            check(FakeAsset::alive == 1, "asset alive while a handle exists");
        }

        check(FakeAsset::alive == 0, "asset released when the last handle dies");
        check(cache.residentCount() == 0, "no resident assets after release");

        // Reloading after release must construct a fresh asset.
        auto again = cache.load("a.png");
        check(again.valid(), "reload valid");
        check(FakeAsset::created == 2, "reload constructs a new asset");
        check(FakeAsset::alive == 1, "one alive after reload");
    }

    // --- expired entries are purged lazily --------------------------------
    {
        FakeAsset::alive = 0;
        FakeAsset::created = 0;

        ply::AssetCache<FakeAsset> cache;
        cache.setLoader(makeLoader);

        // Hold handles so every entry stays live.
        std::vector<ply::AssetHandle<FakeAsset>> held;
        for (int i = 0; i < 5; ++i) {
            held.push_back(cache.load("f" + std::to_string(i) + ".png"));
        }
        check(cache.entryCount() == 5, "five live entries tracked");
        check(cache.residentCount() == 5, "five resident assets");
        check(FakeAsset::alive == 5, "five assets alive");

        held.clear();
        check(cache.residentCount() == 0, "none resident after dropping handles");
        check(FakeAsset::alive == 0, "all assets released");

        // A new load purges the stale entries rather than growing forever.
        auto live = cache.load("keep.png");
        check(live.valid(), "live load works after purge");
        check(cache.entryCount() == 1, "expired entries purged on next load");
    }

    // --- missing loader yields an invalid handle, not a crash -------------
    {
        ply::AssetCache<FakeAsset> cache;  // no loader
        auto handle = cache.load("a.png");
        check(!handle.valid(), "no loader -> invalid handle");
        check(handle.get() == nullptr, "invalid handle dereferences to null");
    }

    // --- a failing loader yields an invalid handle ------------------------
    {
        ply::AssetCache<FakeAsset> cache;
        cache.setLoader([](const std::string&) -> std::shared_ptr<FakeAsset> {
            return nullptr;
        });
        auto handle = cache.load("missing.png");
        check(!handle.valid(), "failed load -> invalid handle");
        check(cache.residentCount() == 0, "failed load is not cached");
    }

    // --- handles keep the asset alive independently of the cache ----------
    {
        FakeAsset::alive = 0;
        FakeAsset::created = 0;

        ply::AssetCache<FakeAsset>* heap = new ply::AssetCache<FakeAsset>();
        heap->setLoader(makeLoader);
        auto handle = heap->load("a.png");
        delete heap;  // cache destroyed while the handle lives

        check(handle.valid(), "handle survives cache destruction");
        check(FakeAsset::alive == 1, "asset alive after cache destroyed");
    }

    if (failures == 0) {
        std::printf("asset cache tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "asset cache tests: %d failure(s)\n", failures);
    return 1;
}
