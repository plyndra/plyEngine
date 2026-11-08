#include "plyEngine/Physics.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ply {

namespace {
constexpr float kEpsilon = 1e-8f;
}

Aabb makeAabb(const glm::vec2& center, const glm::vec2& size) {
    const glm::vec2 half = size * 0.5f;
    return Aabb{center - half, center + half};
}

Aabb makeAabbFromBottomLeft(const glm::vec2& bottomLeft, const glm::vec2& size) {
    return Aabb{bottomLeft, bottomLeft + size};
}

bool overlaps(const Aabb& a, const Aabb& b) {
    if (a.max.x <= b.min.x || b.max.x <= a.min.x) {
        return false;
    }
    if (a.max.y <= b.min.y || b.max.y <= a.min.y) {
        return false;
    }
    return true;
}

bool contains(const Aabb& box, const glm::vec2& point) {
    return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y &&
           point.y <= box.max.y;
}

SweepHit sweepAabb(const Aabb& moving, const glm::vec2& delta,
                   const Aabb& obstacle) {
    SweepHit result;

    // If we already overlap, do not block: let the body move out.
    if (overlaps(moving, obstacle)) {
        return result;
    }

    // Minkowski sum: shrink the moving box to a point and inflate the obstacle.
    const glm::vec2 half = moving.halfSize();
    const glm::vec2 origin = moving.center();
    const glm::vec2 expandedMin = obstacle.min - half;
    const glm::vec2 expandedMax = obstacle.max + half;

    float tEnter = -std::numeric_limits<float>::infinity();
    float tExit = std::numeric_limits<float>::infinity();
    glm::vec2 normal{0.0f, 0.0f};

    for (int axis = 0; axis < 2; ++axis) {
        const float d = delta[axis];
        const float o = origin[axis];
        const float lo = expandedMin[axis];
        const float hi = expandedMax[axis];

        if (std::fabs(d) < kEpsilon) {
            // No movement on this axis: the origin must already be inside the
            // slab, otherwise the sweep can never connect.
            if (o < lo || o > hi) {
                return result;
            }
            continue;
        }

        const float inv = 1.0f / d;
        float tNear = 0.0f;
        float tFar = 0.0f;
        float nearNormal = 0.0f;

        if (d > 0.0f) {
            tNear = (lo - o) * inv;
            tFar = (hi - o) * inv;
            nearNormal = -1.0f;  // pushed back along -axis
        } else {
            tNear = (hi - o) * inv;
            tFar = (lo - o) * inv;
            nearNormal = 1.0f;  // pushed back along +axis
        }

        if (tNear > tEnter) {
            tEnter = tNear;
            normal = glm::vec2{0.0f, 0.0f};
            normal[axis] = nearNormal;
        }
        tExit = std::min(tExit, tFar);
        if (tEnter > tExit) {
            return result;  // slabs never overlap
        }
    }

    if (tExit < 0.0f) {
        return result;  // obstacle is entirely behind the motion
    }
    if (tEnter < 0.0f || tEnter > 1.0f) {
        return result;  // already crossing at t=0, or too far away
    }

    result.hit = true;
    result.t = tEnter;
    result.normal = normal;
    return result;
}

MoveResult moveAndSlide(const Aabb& body, const glm::vec2& delta,
                        const ColliderQuery& query, float skin) {
    MoveResult result;
    glm::vec2 position = body.center();
    const glm::vec2 half = body.halfSize();
    glm::vec2 remaining = delta;

    auto boxAt = [&](const glm::vec2& c) { return Aabb{c - half, c + half}; };

    std::vector<Aabb> candidates;

    // --- X axis ------------------------------------------------------------
    if (std::fabs(remaining.x) > kEpsilon) {
        const Aabb current = boxAt(position);
        Aabb swept = current;
        if (remaining.x > 0.0f) {
            swept.max.x = current.max.x + remaining.x;
        } else {
            swept.min.x = current.min.x + remaining.x;
        }

        candidates.clear();
        query(swept, candidates);

        float bestT = 1.0f;
        glm::vec2 bestNormal{0.0f, 0.0f};
        for (const Aabb& candidate : candidates) {
            const SweepHit hit = sweepAabb(current, glm::vec2{remaining.x, 0.0f}, candidate);
            if (hit.hit && hit.t < bestT) {
                bestT = hit.t;
                bestNormal = hit.normal;
            }
        }

        if (bestT < 1.0f) {
            position.x += remaining.x * bestT + bestNormal.x * skin;
            result.hitX = true;
            remaining.x = 0.0f;
        } else {
            position.x += remaining.x;
        }
    }

    // --- Y axis ------------------------------------------------------------
    if (std::fabs(remaining.y) > kEpsilon) {
        const Aabb current = boxAt(position);
        Aabb swept = current;
        if (remaining.y > 0.0f) {
            swept.max.y = current.max.y + remaining.y;
        } else {
            swept.min.y = current.min.y + remaining.y;
        }

        candidates.clear();
        query(swept, candidates);

        float bestT = 1.0f;
        glm::vec2 bestNormal{0.0f, 0.0f};
        for (const Aabb& candidate : candidates) {
            const SweepHit hit = sweepAabb(current, glm::vec2{0.0f, remaining.y}, candidate);
            if (hit.hit && hit.t < bestT) {
                bestT = hit.t;
                bestNormal = hit.normal;
            }
        }

        if (bestT < 1.0f) {
            position.y += remaining.y * bestT + bestNormal.y * skin;
            result.hitY = true;
            remaining.y = 0.0f;
        } else {
            position.y += remaining.y;
        }
    }

    result.center = position;
    result.applied = position - body.center();
    return result;
}

}  // namespace ply
