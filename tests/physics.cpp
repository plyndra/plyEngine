// Headless physics tests: AABB helpers, swept collision, move-and-slide,
// tilemap queries, spatial hashing and the ECS physics step. No GL required.

#include "plyEngine/Physics.hpp"
#include "plyEngine/PhysicsBody.hpp"
#include "plyEngine/SpatialHashGrid.hpp"
#include "plyEngine/Tilemap.hpp"
#include "plyEngine/Transform.hpp"

#include <cmath>
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

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

// A query backed by an explicit list of boxes.
ply::ColliderQuery boxQuery(const std::vector<ply::Aabb>& boxes) {
    return [&boxes](const ply::Aabb& area, std::vector<ply::Aabb>& out) {
        for (const ply::Aabb& box : boxes) {
            if (ply::overlaps(box, area)) {
                out.push_back(box);
            }
        }
    };
}

}  // namespace

int main() {
    // --- AABB basics -------------------------------------------------------
    {
        const ply::Aabb box = ply::makeAabb({10.0f, 20.0f}, {4.0f, 8.0f});
        check(near(box.min.x, 8.0f) && near(box.min.y, 16.0f), "makeAabb min");
        check(near(box.max.x, 12.0f) && near(box.max.y, 24.0f), "makeAabb max");
        check(near(box.center().x, 10.0f) && near(box.center().y, 20.0f), "center");
        check(near(box.size().x, 4.0f) && near(box.size().y, 8.0f), "size");
        check(near(box.halfSize().x, 2.0f) && near(box.halfSize().y, 4.0f), "half");
    }

    // --- overlapping is strict: touching edges do not overlap -------------
    {
        const ply::Aabb a = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        const ply::Aabb far = ply::makeAabb({10.0f, 0.0f}, {2.0f, 2.0f});
        const ply::Aabb close = ply::makeAabb({1.5f, 0.0f}, {2.0f, 2.0f});
        const ply::Aabb touching = ply::makeAabb({2.0f, 0.0f}, {2.0f, 2.0f});

        check(!ply::overlaps(a, far), "separated boxes do not overlap");
        check(ply::overlaps(a, close), "overlapping boxes overlap");
        check(!ply::overlaps(a, touching),
              "edge-touching boxes do not overlap (strict)");
        check(ply::contains(a, {0.0f, 0.0f}), "contains centre");
        check(!ply::contains(a, {5.0f, 0.0f}), "does not contain outside point");
    }

    // --- sweep: head-on hit -----------------------------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        const ply::Aabb wall = ply::makeAabb({10.0f, 0.0f}, {2.0f, 2.0f});

        const ply::SweepHit hit = ply::sweepAabb(body, {20.0f, 0.0f}, wall);
        check(hit.hit, "head-on sweep hits");
        check(near(hit.t, 0.4f, 1e-4f), "head-on hit time (8 units of 20)");
        check(near(hit.normal.x, -1.0f), "head-on normal points back along -x");
        check(near(hit.normal.y, 0.0f), "head-on normal has no y component");
    }

    // --- sweep: miss when passing above -----------------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 100.0f}, {2.0f, 2.0f});
        const ply::Aabb wall = ply::makeAabb({10.0f, 0.0f}, {2.0f, 2.0f});
        check(!ply::sweepAabb(body, {20.0f, 0.0f}, wall).hit,
              "sweep misses when passing above");
    }

    // --- sweep: miss when the obstacle is behind --------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        const ply::Aabb wall = ply::makeAabb({-10.0f, 0.0f}, {2.0f, 2.0f});
        check(!ply::sweepAabb(body, {20.0f, 0.0f}, wall).hit,
              "sweep misses when the obstacle is behind");
    }

    // --- sweep: too far to reach ------------------------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        const ply::Aabb wall = ply::makeAabb({100.0f, 0.0f}, {2.0f, 2.0f});
        check(!ply::sweepAabb(body, {20.0f, 0.0f}, wall).hit,
              "sweep misses when the delta is too short");
    }

    // --- sweep: already overlapping does not block ------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {4.0f, 4.0f});
        const ply::Aabb overlapping = ply::makeAabb({1.0f, 0.0f}, {4.0f, 4.0f});
        check(!ply::sweepAabb(body, {5.0f, 0.0f}, overlapping).hit,
              "overlapping start is ignored so bodies can escape");
    }

    // --- sweep: no tunnelling at any speed --------------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        const ply::Aabb thinWall = ply::makeAabb({50.0f, 0.0f}, {1.0f, 2.0f});
        // The wall's near face (Minkowski-expanded) sits at x = 48.5, so only
        // deltas longer than that can reach it.
        for (float speed = 10.0f; speed <= 100000.0f; speed *= 10.0f) {
            const ply::SweepHit hit = ply::sweepAabb(body, {speed, 0.0f}, thinWall);
            if (speed >= 49.0f) {
                check(hit.hit, "in-range delta always reports the thin wall");
            }
            if (hit.hit) {
                check(hit.t > 0.0f && hit.t <= 1.0f, "hit time normalised");
            }
        }
        // The key anti-tunnelling property: a huge delta still stops at 48.5.
        const ply::SweepHit far = ply::sweepAabb(body, {100000.0f, 0.0f}, thinWall);
        check(far.hit && near(far.t * 100000.0f, 48.5f, 0.01f),
              "huge delta stops exactly at the wall face");
    }

    // --- sweep: vertical ---------------------------------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 10.0f}, {2.0f, 2.0f});
        const ply::Aabb floor = ply::makeAabb({0.0f, 0.0f}, {20.0f, 2.0f});
        const ply::SweepHit hit = ply::sweepAabb(body, {0.0f, -20.0f}, floor);
        check(hit.hit, "downward sweep hits the floor");
        check(near(hit.normal.y, 1.0f), "floor normal points up");
    }

    // --- moveAndSlide: unobstructed --------------------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        const std::vector<ply::Aabb> none;
        const ply::MoveResult r = ply::moveAndSlide(body, {5.0f, 3.0f}, boxQuery(none));
        check(near(r.center.x, 5.0f) && near(r.center.y, 3.0f), "free movement");
        check(!r.hitX && !r.hitY, "no contacts when unobstructed");
    }

    // --- moveAndSlide: stops at a wall ------------------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        const std::vector<ply::Aabb> wall{ply::makeAabb({10.0f, 0.0f}, {2.0f, 10.0f})};
        const ply::MoveResult r = ply::moveAndSlide(body, {20.0f, 0.0f}, boxQuery(wall));
        check(r.hitX, "wall contact reported on X");
        check(r.center.x < 9.5f, "body stopped before the wall");
        // Wall spans x in [9,11]; body half-width 1, so its centre stops at 8.
        check(near(r.center.x, 8.0f, 0.01f), "body stops flush against wall");
        check(near(r.center.y, 0.0f), "no vertical movement");
    }

    // --- moveAndSlide: slides along a wall --------------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        const std::vector<ply::Aabb> wall{ply::makeAabb({10.0f, 0.0f}, {2.0f, 100.0f})};
        // Push diagonally into the wall: X is clamped, Y must still move.
        const ply::MoveResult r = ply::moveAndSlide(body, {20.0f, 5.0f}, boxQuery(wall));
        check(r.hitX, "diagonal into wall reports X contact");
        check(!r.hitY, "no Y contact");
        check(near(r.center.y, 5.0f), "still slides along Y while blocked on X");
    }

    // --- moveAndSlide: rests on the floor ---------------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 10.0f}, {2.0f, 2.0f});
        const std::vector<ply::Aabb> floor{ply::makeAabb({0.0f, 0.0f}, {100.0f, 2.0f})};
        const ply::MoveResult r = ply::moveAndSlide(body, {0.0f, -20.0f}, boxQuery(floor));
        check(r.hitY, "floor contact reported");
        // Floor spans y in [-1,1]; body half-height 1, so its centre stops at 2.
        check(near(r.center.y, 2.0f, 0.01f), "body rests on top of the floor");
    }

    // --- moveAndSlide: high speed does not tunnel -------------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        const std::vector<ply::Aabb> wall{ply::makeAabb({10.0f, 0.0f}, {0.5f, 10.0f})};
        const ply::MoveResult r = ply::moveAndSlide(body, {5000.0f, 0.0f}, boxQuery(wall));
        check(r.hitX, "huge delta still collides");
        check(r.center.x < 10.0f, "huge delta does not tunnel through the wall");
    }

    // --- moveAndSlide: corner resolved on both axes -----------------------
    {
        const ply::Aabb body = ply::makeAabb({0.0f, 0.0f}, {2.0f, 2.0f});
        // A tall wall to the right and a wide ceiling above, so diagonal motion
        // is blocked on X first and then on Y.
        const std::vector<ply::Aabb> blocks{
            ply::makeAabb({10.0f, 0.0f}, {2.0f, 40.0f}),   // wall:  x 9..11
            ply::makeAabb({0.0f, 10.0f}, {40.0f, 2.0f}),   // ceiling: y 9..11
        };
        const ply::MoveResult r = ply::moveAndSlide(body, {20.0f, 20.0f}, boxQuery(blocks));
        check(r.hitX && r.hitY, "both axes report contact in a corner");
        check(near(r.center.x, 8.0f, 0.01f), "X resolved against the wall");
        check(near(r.center.y, 8.0f, 0.01f), "Y resolved against the ceiling");
    }

    // --- tilemap: solidity and bounds ------------------------------------
    {
        ply::Tilemap map(8, 4, 16.0f);
        check(map.widthCells() == 8 && map.heightCells() == 4, "grid dimensions");
        check(map.solidCellCount() == 0, "starts empty");

        map.setSolid(2, 1, true);
        check(map.isSolid(2, 1), "cell set solid");
        check(!map.isSolid(3, 1), "neighbour stays empty");
        check(map.solidCellCount() == 1, "solid count increments");

        map.setSolid(2, 1, true);  // idempotent
        check(map.solidCellCount() == 1, "re-setting solid does not double count");

        map.setSolid(2, 1, false);
        check(!map.isSolid(2, 1), "cell cleared");
        check(map.solidCellCount() == 0, "solid count decrements");

        // Out-of-range writes are ignored; reads follow borderSolid.
        map.setSolid(-1, 0, true);
        map.setSolid(999, 0, true);
        check(map.solidCellCount() == 0, "out-of-range writes ignored");
        check(map.isSolid(-1, 0), "out of bounds solid by default");
        map.setBorderSolid(false);
        check(!map.isSolid(-1, 0), "border solidity is configurable");
    }

    // --- tilemap: query returns exactly the overlapped solid cells --------
    {
        ply::Tilemap map(4, 4, 10.0f);
        map.setSolid(0, 0, true);
        map.setSolid(1, 0, true);
        map.setSolid(3, 3, true);

        std::vector<ply::Aabb> out;
        // Covers cells (0,0) and (1,0): x 0..20, y 0..10.
        map.query(ply::Aabb{{0.0f, 0.0f}, {19.9f, 9.9f}}, out);
        check(out.size() == 2, "query finds the two solid cells in range");

        out.clear();
        // Covers only cell (3,3): x 30..40, y 30..40.
        map.query(ply::Aabb{{30.0f, 30.0f}, {39.9f, 39.9f}}, out);
        check(out.size() == 1, "query finds the far solid cell");

        out.clear();
        // Covers cell (1,1), which is empty.
        map.query(ply::Aabb{{15.0f, 15.0f}, {19.9f, 19.9f}}, out);
        check(out.empty(), "query of an empty region returns nothing");

        out.clear();
        // Ending exactly on a cell boundary must not include the next cell.
        map.query(ply::Aabb{{10.0f, 0.0f}, {20.0f, 10.0f}}, out);
        check(out.size() == 1, "boundary-exact query excludes the adjacent cell");
    }

    // --- spatial hash: insert and query ----------------------------------
    {
        ply::SpatialHashGrid grid(32.0f);
        grid.insert(1, ply::makeAabb({0.0f, 0.0f}, {10.0f, 10.0f}));
        grid.insert(2, ply::makeAabb({200.0f, 0.0f}, {10.0f, 10.0f}));

        std::vector<uint64_t> ids;
        grid.query(ply::makeAabb({0.0f, 0.0f}, {20.0f, 20.0f}), ids);
        bool hasFirst = false;
        bool hasSecond = false;
        for (uint64_t id : ids) {
            if (id == 1) hasFirst = true;
            if (id == 2) hasSecond = true;
        }
        check(hasFirst, "query finds the near item");
        check(!hasSecond, "query excludes the far item");

        std::vector<ply::Aabb> boxes;
        grid.queryBoxes(ply::makeAabb({0.0f, 0.0f}, {20.0f, 20.0f}), boxes);
        check(boxes.size() == 1, "queryBoxes de-duplicates");
    }

    // --- spatial hash: dedupes items spanning many cells -----------------
    {
        ply::SpatialHashGrid grid(16.0f);
        grid.insert(7, ply::makeAabb({0.0f, 0.0f}, {200.0f, 200.0f}));

        std::vector<uint64_t> ids;
        grid.query(ply::makeAabb({0.0f, 0.0f}, {200.0f, 200.0f}), ids);
        check(ids.size() > 1, "raw query returns one entry per cell");

        std::vector<ply::Aabb> boxes;
        grid.queryBoxes(ply::makeAabb({0.0f, 0.0f}, {200.0f, 200.0f}), boxes);
        check(boxes.size() == 1, "queryBoxes collapses to one item");

        grid.clear();
        check(grid.itemCount() == 0, "clear empties the grid");
    }

    // --- ECS physics: gravity then landing -------------------------------
    {
        ply::Tilemap map(40, 20, 32.0f);
        for (int x = 0; x < 40; ++x) {
            map.setSolid(x, 0, true);  // floor row
        }

        ply::World world;
        const ply::Entity e = world.spawn();
        world.add<ply::Transform>(
            e, ply::Transform{glm::vec2{100.0f, 300.0f}, 0.0f, glm::vec2{1.0f, 1.0f}});
        world.add<ply::GlobalTransform>(e, ply::GlobalTransform{});
        world.add<ply::Velocity>(e, ply::Velocity{});
        world.add<ply::Body>(e, ply::Body{glm::vec2{10.0f, 10.0f}});

        ply::PhysicsSettings settings;
        settings.gravity = {0.0f, -1000.0f};

        bool landed = false;
        for (int step = 0; step < 600; ++step) {
            ply::stepPhysics(world, map, 1.0f / 60.0f, settings);
            const ply::Body* body = world.get<ply::Body>(e);
            if (body && body->grounded) {
                landed = true;
                break;
            }
        }

        check(landed, "falling body lands on the tile floor");

        const ply::Body* body = world.get<ply::Body>(e);
        check(body && body->grounded, "grounded flag set after landing");

        const ply::Transform* t = world.get<ply::Transform>(e);
        check(t && t->position.y > 32.0f, "body rests above the floor row");
        check(t && t->position.y < 64.0f, "body did not sink into the floor");
    }

    // --- ECS physics: a wall stops horizontal motion ---------------------
    {
        ply::Tilemap map(40, 20, 32.0f);
        for (int y = 0; y < 20; ++y) {
            map.setSolid(10, y, true);  // vertical wall
        }

        ply::World world;
        const ply::Entity e = world.spawn();
        world.add<ply::Transform>(
            e, ply::Transform{glm::vec2{100.0f, 300.0f}, 0.0f, glm::vec2{1.0f, 1.0f}});
        world.add<ply::GlobalTransform>(e, ply::GlobalTransform{});
        world.add<ply::Velocity>(e, ply::Velocity{glm::vec2{600.0f, 0.0f}});
        world.add<ply::Body>(e, ply::Body{glm::vec2{10.0f, 10.0f}});

        ply::PhysicsSettings settings;
        settings.gravity = {0.0f, 0.0f};

        for (int step = 0; step < 300; ++step) {
            ply::stepPhysics(world, map, 1.0f / 60.0f, settings);
        }

        const ply::Transform* t = world.get<ply::Transform>(e);
        check(t && t->position.x < 10 * 32.0f, "wall blocked the body");
        check(t && t->position.x > 100.0f, "body advanced towards the wall");
    }

    // --- spatial hash: re-inserting an id moves it ------------------------
    {
        ply::SpatialHashGrid grid(32.0f);
        grid.insert(1, ply::makeAabb({0.0f, 0.0f}, {10.0f, 10.0f}));
        check(grid.itemCount() == 1, "one item after first insert");

        // Move the same id far away without clearing.
        grid.insert(1, ply::makeAabb({400.0f, 0.0f}, {10.0f, 10.0f}));
        check(grid.itemCount() == 1, "re-insert does not duplicate the item");

        std::vector<uint64_t> stale;
        grid.query(ply::makeAabb({0.0f, 0.0f}, {20.0f, 20.0f}), stale);
        check(stale.empty(), "old cell no longer reports the moved item");

        std::vector<uint64_t> current;
        grid.query(ply::makeAabb({400.0f, 0.0f}, {20.0f, 20.0f}), current);
        bool found = false;
        for (uint64_t id : current) {
            if (id == 1) found = true;
        }
        check(found, "new cell reports the moved item");

        // Repeated moves must not grow the bucket map without bound.
        const size_t before = grid.bucketCount();
        for (int i = 0; i < 1000; ++i) {
            grid.insert(1, ply::makeAabb({static_cast<float>(i) * 100.0f, 0.0f},
                                         {10.0f, 10.0f}));
        }
        check(grid.bucketCount() <= before + 2,
              "repeated re-inserts keep the bucket count bounded");
        check(grid.itemCount() == 1, "repeated re-inserts keep one item");
    }

    // --- ECS physics: zero dt is a no-op ---------------------------------
    {
        ply::Tilemap map(8, 8, 16.0f);
        ply::World world;
        const ply::Entity e = world.spawn();
        world.add<ply::Transform>(
            e, ply::Transform{glm::vec2{50.0f, 50.0f}, 0.0f, glm::vec2{1.0f, 1.0f}});
        world.add<ply::Body>(e, ply::Body{glm::vec2{5.0f, 5.0f}});
        ply::stepPhysics(world, map, 0.0f);
        const ply::Transform* t = world.get<ply::Transform>(e);
        check(t && near(t->position.y, 50.0f), "zero dt does not move bodies");
    }

    // --- determinism: identical setups produce identical results ---------
    {
        auto run = []() {
            ply::Tilemap map(40, 20, 32.0f);
            for (int x = 0; x < 40; ++x) map.setSolid(x, 0, true);
            map.setSolid(10, 1, true);

            ply::World world;
            const ply::Entity e = world.spawn();
            world.add<ply::Transform>(
                e, ply::Transform{glm::vec2{100.0f, 300.0f}, 0.0f, glm::vec2{1.0f, 1.0f}});
            world.add<ply::GlobalTransform>(e, ply::GlobalTransform{});
            world.add<ply::Velocity>(e, ply::Velocity{glm::vec2{120.0f, 0.0f}});
            world.add<ply::Body>(e, ply::Body{glm::vec2{10.0f, 10.0f}});

            for (int step = 0; step < 240; ++step) {
                ply::stepPhysics(world, map, 1.0f / 60.0f);
            }
            const ply::Transform* t = world.get<ply::Transform>(e);
            return t ? t->position : glm::vec2{0.0f};
        };

        const glm::vec2 a = run();
        const glm::vec2 b = run();
        check(a.x == b.x && a.y == b.y, "physics is deterministic across runs");
    }

    if (failures == 0) {
        std::printf("physics tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "physics tests: %d failure(s)\n", failures);
    return 1;
}
