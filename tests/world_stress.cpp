// Stress tests for the ECS storage layer.
//
// The point of this file is to exercise the paths where the World's internal
// vectors reallocate, because that is when stale references become use-after-
// free. Run it under -DPLY_SANITIZE=ON to make such bugs loud.

#include "plyEngine/World.hpp"

#include <cstdio>
#include <vector>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

// Distinct component types, used to create many archetypes.
template <int N>
struct Tagged {
    int value = N;
};

template <int N>
void addTagged(ply::World& world, ply::Entity e) {
    world.add<Tagged<N>>(e, Tagged<N>{});
}

// Adds `count` distinct-tag components (one archetype hop each).
void addManyTags(ply::World& world, ply::Entity e, int count) {
    switch (count) {
        case 0: break;
        case 1: addTagged<0>(world, e); break;
        case 2: addTagged<0>(world, e); addTagged<1>(world, e); break;
        case 3: addTagged<0>(world, e); addTagged<1>(world, e); addTagged<2>(world, e); break;
        case 4: addTagged<0>(world, e); addTagged<1>(world, e); addTagged<2>(world, e); addTagged<3>(world, e); break;
        default: break;
    }
}

}  // namespace

int main() {
    // --- removing a component while archetypes reallocate -------------------
    //
    // World::remove<T> reads the source archetype, then calls ensureArchetype()
    // which may push_back into the archetype vector. Growing archetypes_ by
    // doubling means the reallocation boundary is hit for specific counts, so
    // sweep a range of "how many archetypes exist before the removal".
    for (int archetypesBefore = 1; archetypesBefore <= 80; ++archetypesBefore) {
        ply::World world;

        // Build up archetypesBefore distinct archetypes.
        for (int i = 0; i < archetypesBefore; ++i) {
            const ply::Entity filler = world.spawn();
            addManyTags(world, filler, (i % 4) + 1);
        }

        // An entity with two tags; removing the second creates a new archetype
        // and thus may reallocate.
        const ply::Entity subject = world.spawn();
        addTagged<0>(world, subject);
        addTagged<1>(world, subject);
        addTagged<2>(world, subject);

        world.remove<Tagged<1>>(subject);

        // The surviving components must be intact and readable.
        const Tagged<0>* a = world.get<Tagged<0>>(subject);
        const Tagged<2>* c = world.get<Tagged<2>>(subject);
        if (!a || a->value != 0 || !c || c->value != 2) {
            std::fprintf(stderr,
                         "  FAIL: survivors corrupted at archetypesBefore=%d\n",
                         archetypesBefore);
            ++failures;
            break;
        }
        if (world.has<Tagged<1>>(subject)) {
            std::fprintf(stderr, "  FAIL: removed component still present\n");
            ++failures;
            break;
        }
    }

    // --- destroying entities while archetypes reallocate -------------------
    for (int archetypesBefore = 1; archetypesBefore <= 80; ++archetypesBefore) {
        ply::World world;
        std::vector<ply::Entity> keep;

        for (int i = 0; i < archetypesBefore; ++i) {
            const ply::Entity e = world.spawn();
            addManyTags(world, e, (i % 4) + 1);
            keep.push_back(e);
        }

        // Destroy every third entity.
        for (size_t i = 0; i < keep.size(); i += 3) {
            world.destroy(keep[i]);
        }

        bool intact = true;
        for (size_t i = 0; i < keep.size(); ++i) {
            if (i % 3 == 0) {
                if (world.alive(keep[i])) {
                    intact = false;
                }
            } else if (!world.alive(keep[i])) {
                intact = false;
            }
        }
        if (!intact) {
            std::fprintf(stderr, "  FAIL: liveness wrong at archetypesBefore=%d\n",
                         archetypesBefore);
            ++failures;
            break;
        }
    }

    // --- component with a destructor must run exactly once ------------------
    {
        ply::World world;
        for (int i = 0; i < 50; ++i) {
            const ply::Entity e = world.spawn();
            world.add<std::vector<int>>(e, std::vector<int>(100, i));
        }
        check(world.entityCount() == 50, "50 entities with vectors");
    }

    if (failures == 0) {
        std::printf("world stress tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "world stress tests: %d failure(s)\n", failures);
    return 1;
}
