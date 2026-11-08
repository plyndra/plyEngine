// Headless transform-hierarchy tests: no GL context required.

#include "plyEngine/Transform.hpp"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

bool near(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) <= eps;
}

// World-space origin of an entity's GlobalTransform.
glm::vec2 origin(ply::World& world, ply::Entity e) {
    const ply::GlobalTransform* gt = world.get<ply::GlobalTransform>(e);
    if (!gt) {
        return {NAN, NAN};
    }
    return {gt->matrix[3][0], gt->matrix[3][1]};
}

// Creates an entity with Transform + GlobalTransform.
ply::Entity makeNode(ply::World& world, float x, float y) {
    const ply::Entity e = world.spawn();
    world.add<ply::Transform>(e, ply::Transform{glm::vec2{x, y}, 0.0f, glm::vec2{1.0f, 1.0f}});
    world.add<ply::GlobalTransform>(e, ply::GlobalTransform{});
    return e;
}

}  // namespace

int main() {
    // --- a root resolves to its local transform ---------------------------
    {
        ply::World world;
        const ply::Entity root = makeNode(world, 10.0f, 20.0f);
        ply::updateTransforms(world);

        const glm::vec2 o = origin(world, root);
        check(near(o.x, 10.0f) && near(o.y, 20.0f), "root at its local position");
    }

    // --- a child is positioned relative to its parent ---------------------
    {
        ply::World world;
        const ply::Entity parent = makeNode(world, 100.0f, 50.0f);
        const ply::Entity child = makeNode(world, 10.0f, 5.0f);
        ply::setParent(world, child, parent);
        ply::updateTransforms(world);

        const glm::vec2 c = origin(world, child);
        check(near(c.x, 110.0f) && near(c.y, 55.0f), "child is parent + local");

        check(world.has<ply::Children>(parent), "parent got a Children list");
        check(world.has<ply::Parent>(child), "child got a Parent link");
    }

    // --- three levels compose correctly -----------------------------------
    {
        ply::World world;
        const ply::Entity a = makeNode(world, 100.0f, 0.0f);
        const ply::Entity b = makeNode(world, 50.0f, 0.0f);
        const ply::Entity c = makeNode(world, 25.0f, 0.0f);

        ply::setParent(world, b, a);
        ply::setParent(world, c, b);
        ply::updateTransforms(world);

        const glm::vec2 o = origin(world, c);
        check(near(o.x, 175.0f), "grandchild accumulates both offsets");
    }

    // --- moving the parent moves the whole subtree ------------------------
    {
        ply::World world;
        const ply::Entity parent = makeNode(world, 0.0f, 0.0f);
        const ply::Entity child = makeNode(world, 10.0f, 0.0f);
        ply::setParent(world, child, parent);
        ply::updateTransforms(world);
        check(near(origin(world, child).x, 10.0f), "child starts at 10");

        ply::setTransform(world, parent,
                          ply::Transform{glm::vec2{100.0f, 0.0f}, 0.0f,
                                         glm::vec2{1.0f, 1.0f}});
        ply::updateTransforms(world);
        check(near(origin(world, child).x, 110.0f),
              "moving the parent moves the child");
    }

    // --- dirty propagation through setTransform ---------------------------
    {
        ply::World world;
        const ply::Entity root = makeNode(world, 0.0f, 0.0f);
        const ply::Entity mid = makeNode(world, 1.0f, 0.0f);
        const ply::Entity leaf = makeNode(world, 1.0f, 0.0f);
        ply::setParent(world, mid, root);
        ply::setParent(world, leaf, mid);
        ply::updateTransforms(world);
        check(near(origin(world, leaf).x, 2.0f), "leaf at 2 before move");

        ply::setTransform(world, root,
                          ply::Transform{glm::vec2{10.0f, 0.0f}, 0.0f,
                                         glm::vec2{1.0f, 1.0f}});
        ply::updateTransforms(world);
        check(near(origin(world, leaf).x, 12.0f),
              "leaf follows a grandparent move (dirty propagated)");
    }

    // --- markTransformDirty after raw mutation ----------------------------
    {
        ply::World world;
        const ply::Entity parent = makeNode(world, 0.0f, 0.0f);
        const ply::Entity child = makeNode(world, 5.0f, 0.0f);
        ply::setParent(world, child, parent);
        ply::updateTransforms(world);
        check(near(origin(world, child).x, 5.0f), "child at 5");

        // Mutate through get() and mark explicitly (documented pattern).
        ply::Transform* t = world.get<ply::Transform>(parent);
        t->position.x = 20.0f;
        ply::markTransformDirty(world, parent);
        ply::updateTransforms(world);
        check(near(origin(world, child).x, 25.0f),
              "explicit markTransformDirty propagates");
    }

    // --- rotation composes ------------------------------------------------
    {
        ply::World world;
        const ply::Entity parent = world.spawn();
        const float halfPi = 1.5707963f;
        world.add<ply::Transform>(
            parent, ply::Transform{glm::vec2{0.0f, 0.0f}, halfPi, glm::vec2{1.0f, 1.0f}});
        world.add<ply::GlobalTransform>(parent, ply::GlobalTransform{});

        const ply::Entity child = makeNode(world, 10.0f, 0.0f);
        ply::setParent(world, child, parent);
        ply::updateTransforms(world);

        // A 90 degree parent rotation should map child (10,0) to ~(0,10).
        const glm::vec2 o = origin(world, child);
        check(near(o.x, 0.0f, 1e-4f) && near(o.y, 10.0f, 1e-4f),
              "parent rotation rotates the child's offset");
    }

    // --- scale composes ---------------------------------------------------
    {
        ply::World world;
        const ply::Entity parent = world.spawn();
        world.add<ply::Transform>(
            parent, ply::Transform{glm::vec2{0.0f, 0.0f}, 0.0f, glm::vec2{2.0f, 2.0f}});
        world.add<ply::GlobalTransform>(parent, ply::GlobalTransform{});

        const ply::Entity child = makeNode(world, 10.0f, 0.0f);
        ply::setParent(world, child, parent);
        ply::updateTransforms(world);

        check(near(origin(world, child).x, 20.0f), "parent scale scales child offset");
    }

    // --- cycles are rejected ----------------------------------------------
    {
        ply::World world;
        const ply::Entity a = makeNode(world, 0.0f, 0.0f);
        const ply::Entity b = makeNode(world, 0.0f, 0.0f);
        const ply::Entity c = makeNode(world, 0.0f, 0.0f);

        ply::setParent(world, b, a);
        ply::setParent(world, c, b);
        ply::setParent(world, a, c);  // would close the loop

        check(!world.has<ply::Parent>(a), "cycle-forming parent link rejected");

        // Must terminate rather than hang or crash.
        ply::updateTransforms(world);
        check(true, "updateTransforms terminated with a rejected cycle");
    }

    // --- self-parenting is rejected ---------------------------------------
    {
        ply::World world;
        const ply::Entity e = makeNode(world, 0.0f, 0.0f);
        ply::setParent(world, e, e);
        check(!world.has<ply::Parent>(e), "self-parent rejected");
    }

    // --- reparenting moves the entity, not orphans it ---------------------
    {
        ply::World world;
        const ply::Entity p1 = makeNode(world, 100.0f, 0.0f);
        const ply::Entity p2 = makeNode(world, 0.0f, 100.0f);
        const ply::Entity child = makeNode(world, 10.0f, 0.0f);

        ply::setParent(world, child, p1);
        ply::updateTransforms(world);
        check(near(origin(world, child).x, 110.0f), "under p1");

        ply::setParent(world, child, p2);
        ply::updateTransforms(world);
        const glm::vec2 o = origin(world, child);
        check(near(o.x, 10.0f) && near(o.y, 100.0f), "reparented under p2");

        const ply::Children* c1 = world.get<ply::Children>(p1);
        check(c1 == nullptr, "old parent's Children list cleaned up");
    }

    // --- clearParent leaves local transform intact ------------------------
    {
        ply::World world;
        const ply::Entity parent = makeNode(world, 100.0f, 0.0f);
        const ply::Entity child = makeNode(world, 10.0f, 0.0f);
        ply::setParent(world, child, parent);
        ply::updateTransforms(world);

        ply::clearParent(world, child);
        ply::updateTransforms(world);

        check(!world.has<ply::Parent>(child), "Parent link removed");
        check(near(origin(world, child).x, 10.0f), "child keeps its local position");
        check(world.get<ply::Children>(parent) == nullptr,
              "parent's Children cleaned up when last child leaves");
    }

    // --- destroying a child does not corrupt the world --------------------
    {
        ply::World world;
        const ply::Entity parent = makeNode(world, 0.0f, 0.0f);
        const ply::Entity c1 = makeNode(world, 1.0f, 0.0f);
        const ply::Entity c2 = makeNode(world, 2.0f, 0.0f);
        ply::setParent(world, c1, parent);
        ply::setParent(world, c2, parent);
        ply::updateTransforms(world);

        world.destroy(c1);
        ply::updateTransforms(world);  // must not crash on the dead link

        check(world.alive(parent) && world.alive(c2), "survivors alive");
        check(near(origin(world, c2).x, 2.0f), "sibling still resolves");
    }

    // --- destroying a parent does not strand its children ------------------
    {
        ply::World world;
        const ply::Entity parent = makeNode(world, 100.0f, 0.0f);
        const ply::Entity child = makeNode(world, 10.0f, 0.0f);
        ply::setParent(world, child, parent);
        ply::updateTransforms(world);
        check(near(origin(world, child).x, 110.0f), "child under parent first");

        world.destroy(parent);
        ply::updateTransforms(world);

        // The orphan must be treated as a root: its own local transform, not a
        // stale world matrix from before the parent died.
        check(world.alive(child), "child survived parent destruction");
        check(near(origin(world, child).x, 10.0f),
              "orphaned child falls back to its local transform");
    }

    // --- dead children are pruned from Children lists ----------------------
    {
        ply::World world;
        const ply::Entity parent = makeNode(world, 0.0f, 0.0f);
        const ply::Entity c1 = makeNode(world, 1.0f, 0.0f);
        const ply::Entity c2 = makeNode(world, 2.0f, 0.0f);
        ply::setParent(world, c1, parent);
        ply::setParent(world, c2, parent);
        ply::updateTransforms(world);

        world.destroy(c1);
        ply::updateTransforms(world);

        const ply::Children* kids = world.get<ply::Children>(parent);
        check(kids != nullptr, "parent still has children");
        check(kids != nullptr && kids->value.size() == 1,
              "dead child pruned from Children list");
        check(near(origin(world, c2).x, 2.0f), "surviving sibling still resolves");
    }

    if (failures == 0) {
        std::printf("transform tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "transform tests: %d failure(s)\n", failures);
    return 1;
}
