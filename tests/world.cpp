// Headless ECS tests: no GL context required.

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

struct Position {
    float x = 0.0f, y = 0.0f;
};
struct Velocity {
    float dx = 0.0f, dy = 0.0f;
};
struct Health {
    int hp = 100;
};

}  // namespace

int main() {
    // --- spawn / destroy / alive -------------------------------------------
    {
        ply::World world;
        check(world.entityCount() == 0, "new world has no entities");

        const ply::Entity a = world.spawn();
        const ply::Entity b = world.spawn();
        check(a.valid() && b.valid(), "spawn returns valid handles");
        check(a != b, "handles are distinct");
        check(world.entityCount() == 2, "entityCount after 2 spawns");

        world.destroy(a);
        check(!world.alive(a), "destroyed entity is not alive");
        check(world.alive(b), "other entity still alive");
        check(world.entityCount() == 1, "entityCount after destroy");
    }

    // --- stale handles are rejected ----------------------------------------
    {
        ply::World world;
        const ply::Entity first = world.spawn();
        const uint32_t recycledIndex = first.index;
        world.destroy(first);

        const ply::Entity second = world.spawn();
        check(second.index == recycledIndex, "index is reused");
        check(second.generation != first.generation, "generation is bumped");

        check(!world.alive(first), "STALE handle rejected after recycle");
        check(world.alive(second), "new handle accepted");
        check(!world.has<Position>(first), "stale handle has no components");
    }

    // --- add / get / has / remove ------------------------------------------
    {
        ply::World world;
        const ply::Entity e = world.spawn();

        check(!world.has<Position>(e), "no Position initially");
        check(world.get<Position>(e) == nullptr, "get returns null when absent");

        world.add<Position>(e, Position{3.0f, 4.0f});
        check(world.has<Position>(e), "Position present after add");
        const Position* p = world.get<Position>(e);
        check(p != nullptr, "get non-null after add");
        check(p->x == 3.0f && p->y == 4.0f, "component value stored");

        world.remove<Position>(e);
        check(!world.has<Position>(e), "Position gone after remove");
        check(world.get<Position>(e) == nullptr, "get null after remove");
    }

    // --- adding a second component preserves the first (archetype move) ----
    {
        ply::World world;
        const ply::Entity e = world.spawn();

        world.add<Position>(e, Position{1.0f, 2.0f});
        world.add<Velocity>(e, Velocity{5.0f, 6.0f});

        const Position* p = world.get<Position>(e);
        const Velocity* v = world.get<Velocity>(e);
        check(p && p->x == 1.0f && p->y == 2.0f, "Position survived the move");
        check(v && v->dx == 5.0f && v->dy == 6.0f, "Velocity stored");

        world.remove<Position>(e);
        const Velocity* v2 = world.get<Velocity>(e);
        check(v2 && v2->dx == 5.0f, "Velocity survived removing Position");
        check(!world.has<Position>(e), "Position removed");
    }

    // --- each() visits exactly the matching entities -----------------------
    {
        ply::World world;
        std::vector<ply::Entity> both;
        std::vector<ply::Entity> posOnly;

        for (int i = 0; i < 4; ++i) {
            const ply::Entity e = world.spawn();
            world.add<Position>(e, Position{static_cast<float>(i), 0.0f});
            if (i % 2 == 0) {
                world.add<Velocity>(e, Velocity{0.0f, 1.0f});
                both.push_back(e);
            } else {
                posOnly.push_back(e);
            }
        }

        int posCount = 0;
        world.each<Position>([&](ply::Entity, Position&) { ++posCount; });
        check(posCount == 4, "each<Position> sees all 4");

        int bothCount = 0;
        world.each<Position, Velocity>(
            [&](ply::Entity, Position& p, Velocity& v) {
                ++bothCount;
                p.x += v.dx;
            });
        check(bothCount == 2, "each<Position,Velocity> sees only the 2 pairs");
        check(both.size() == 2 && posOnly.size() == 2, "test setup sanity");
    }

    // --- each() mutation persists ------------------------------------------
    {
        ply::World world;
        const ply::Entity e = world.spawn();
        world.add<Health>(e, Health{100});

        world.each<Health>([](ply::Entity, Health& h) { h.hp -= 30; });
        const Health* h = world.get<Health>(e);
        check(h && h->hp == 70, "mutation through each() persists");
    }

    // --- removal from the middle keeps other entities addressable ---------
    {
        ply::World world;
        std::vector<ply::Entity> entities;
        for (int i = 0; i < 6; ++i) {
            const ply::Entity e = world.spawn();
            world.add<Health>(e, Health{i});
            entities.push_back(e);
        }

        // Destroy a middle entity: the last row is swapped into its slot.
        world.destroy(entities[2]);

        bool othersIntact = true;
        for (int i = 0; i < 6; ++i) {
            if (i == 2) {
                continue;
            }
            const Health* h = world.get<Health>(entities[i]);
            if (!h || h->hp != i) {
                othersIntact = false;
            }
        }
        check(othersIntact, "swap-remove preserved every other entity");
        check(world.alive(entities[5]), "last entity still alive after swap");
    }

    // --- destroying everything leaves a consistent world ------------------
    {
        ply::World world;
        std::vector<ply::Entity> all;
        for (int i = 0; i < 8; ++i) {
            const ply::Entity e = world.spawn();
            world.add<Position>(e, Position{});
            all.push_back(e);
        }
        for (ply::Entity e : all) {
            world.destroy(e);
        }
        check(world.entityCount() == 0, "all destroyed");
        int visited = 0;
        world.each<Position>([&](ply::Entity, Position&) { ++visited; });
        check(visited == 0, "each sees nothing after mass destroy");
    }

    if (failures == 0) {
        std::printf("world tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "world tests: %d failure(s)\n", failures);
    return 1;
}
