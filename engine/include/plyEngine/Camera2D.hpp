#pragma once

#include "plyEngine/UILayout.hpp"

namespace ply {

// Camera2D (M1b b7, ROADMAP_TARGET.md: "follow, deadzone, clamp to map,
// integer snap"; E-b7.1: "clamp at all 4 map edges; deadzone does not
// jitter"). Pure state + integer math: the ONE float in the unit is the
// target input, floored at the door — the sanctioned snap (a7's
// snapCameraAxis precedent: "the float stops at that snap").
//
// Coordinate model (the family's one spelling): the world is y-up (world
// px); the camera state is the VIEW CENTRE in integer world px (the
// OrthoCamera convention, "origin at view center"); the view rect is
// {x - viewWidth/2, y - viewHeight/2, viewWidth, viewHeight} and must stay
// inside [0, mapWidth] x [0, mapHeight] — the four-edge clamp (the one
// documented exception: a map smaller than the view centres instead).
//
// follow() runs three stages in a fixed order (each pinned):
//   1. SNAP: the fractional target is floored to integer world px (10.4 and
//      10.6 are the same target: both 10 — the no-jitter snap).
//   2. DEADZONE: the camera moves the MINIMUM integer distance that brings
//      the target back inside the deadzone rect centred on the camera — a
//      target that stays inside moves the camera not at all (the no-jitter
//      rule), and one that crosses an edge moves the camera to that edge.
//   3. CLAMP: the centre is clamped to [viewWidth/2, mapWidth - viewWidth/2]
//      on x (and the same on y) so the view never leaves the map; at a map
//      edge the clamp wins over the deadzone (the target may sit outside
//      the deadzone — nothing lies beyond the edge to show).
// A map smaller than the view on an axis centres the camera on that axis
// (there is no rect inside the map that fits the view).
class Camera2D {
public:
    struct Config {
        int mapWidth = 0, mapHeight = 0;         // world px
        int viewWidth = 640, viewHeight = 360;   // ADR 0012's internal res
        int deadzoneWidth = 0, deadzoneHeight = 0;  // 0 = follow exactly
    };

    explicit Camera2D(const Config& config);

    /// The view centre, integer world px.
    int x() const;
    int y() const;

    /// The view rect implied by the centre (inside the map by construction).
    UIRect viewRect() const;

    /// Follow the target once (world px, y-up; fractional input floored).
    void follow(double targetX, double targetY);

    /// Jump so the target is centred (no deadzone), then snap + clamp —
    /// the cut-scene/spawn path.
    void snapTo(double targetX, double targetY);

private:
    void clampToMap();

    Config config_{};
    int x_ = 0, y_ = 0;
};

}  // namespace ply
