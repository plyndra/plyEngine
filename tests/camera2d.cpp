// Camera2D (M1b b7, E-b7.1): "clamp at all 4 map edges; deadzone does not
// jitter" — plus the roadmap row's integer snap and follow.
//
// Pinned: the four-edge clamp as literal view rects with the view's edge
// exactly on the map's edge; the deadzone's no-jitter rule (a target
// moving inside — including fractional jitter — leaves the camera
// EXACTLY unchanged; crossing an edge moves the camera the minimum integer
// distance to that edge); the floor snap (10.4 and 10.6 are the same
// target); clamp-wins-over-deadzone at the edges; the small-map centring
// rule; and wide coordinates exact (the F1 lesson: values past a narrow
// type's ceiling).
//
// House rules: pure data. No GL, no window, no clock. The single float is
// the follow/snapTo input, floored at the door (the sanctioned snap).

#include "plyEngine/Camera2D.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

// Live EVIDENCE counters (each incremented by the pins that ran).
int clampMatch = 0, clampRan = 0;
int snapMatch = 0, snapRan = 0;
int deadzoneMatch = 0, deadzoneRan = 0;
int edgeMatch = 0, edgeRan = 0;

void pinAt(int& match, int& ran, bool cond, const char* what) {
    ++ran;
    if (cond) {
        ++match;
        return;
    }
    ++failures;
    std::fprintf(stderr, "  FAIL: %s\n", what);
}

ply::Camera2D::Config defaultConfig() {
    ply::Camera2D::Config config;
    config.mapWidth = 1000;
    config.mapHeight = 800;
    config.viewWidth = 640;
    config.viewHeight = 360;
    config.deadzoneWidth = 200;
    config.deadzoneHeight = 120;
    return config;
}

// --- the four-edge clamp -----------------------------------------------------

void testClampFourEdges() {
    // No deadzone here: the clamp is the only actor. View 640x360 on a
    // 1000x800 map: centre x in [320, 680], y in [180, 620].
    ply::Camera2D::Config config = defaultConfig();
    config.deadzoneWidth = 0;
    config.deadzoneHeight = 0;

    {
        ply::Camera2D camera(config);
        camera.snapTo(-1000, 400);
        pinAt(clampMatch, clampRan, camera.x() == 320 && camera.y() == 400,
              "clamp at the LEFT edge: centre x 320");
        pinAt(clampMatch, clampRan,
              camera.viewRect() == ply::UIRect({0, 220, 640, 360}),
              "left clamp: the view's left edge is the map's (x 0)");
    }
    {
        ply::Camera2D camera(config);
        camera.snapTo(2000, 400);
        pinAt(clampMatch, clampRan, camera.x() == 680 && camera.y() == 400,
              "clamp at the RIGHT edge: centre x 680");
        pinAt(clampMatch, clampRan,
              camera.viewRect() == ply::UIRect({360, 220, 640, 360}),
              "right clamp: the view's right edge is the map's (x 1000)");
    }
    {
        ply::Camera2D camera(config);
        camera.snapTo(500, -1000);
        pinAt(clampMatch, clampRan, camera.x() == 500 && camera.y() == 180,
              "clamp at the TOP edge (world y min): centre y 180");
        pinAt(clampMatch, clampRan,
              camera.viewRect() == ply::UIRect({180, 0, 640, 360}),
              "top clamp: the view's bottom edge is the map's (y 0)");
    }
    {
        ply::Camera2D camera(config);
        camera.snapTo(500, 2000);
        pinAt(clampMatch, clampRan, camera.x() == 500 && camera.y() == 620,
              "clamp at the BOTTOM edge (world y max): centre y 620");
        pinAt(clampMatch, clampRan,
              camera.viewRect() == ply::UIRect({180, 440, 640, 360}),
              "bottom clamp: the view's top edge is the map's (y 800)");
    }

    // Every view rect stays inside the map, whatever the target.
    ply::Camera2D camera(config);
    bool inside = true;
    for (int step = -2; step <= 2; ++step) {
        camera.snapTo(500.0 + step * 600.0, 400.0 - step * 700.0);
        const ply::UIRect view = camera.viewRect();
        inside = inside && view.x >= 0 && view.y >= 0 &&
                 view.x + view.w <= config.mapWidth && view.y + view.h <= config.mapHeight;
    }
    pinAt(clampMatch, clampRan, inside, "the view rect never leaves the map");
}

// --- integer snap (the E-a7.3 rule) -----------------------------------------

void testSnap() {
    ply::Camera2D::Config config = defaultConfig();
    config.deadzoneWidth = 0;
    config.deadzoneHeight = 0;
    ply::Camera2D camera(config);

    // 500.4 and 500.6 floor to the same target: one camera state.
    camera.snapTo(500.4, 400.6);
    const int x1 = camera.x();
    const int y1 = camera.y();
    camera.snapTo(500.6, 400.4);
    pinAt(snapMatch, snapRan, camera.x() == x1 && camera.y() == y1,
          "500.4 and 500.6 are one target (floor snap)");
    pinAt(snapMatch, snapRan, x1 == 500 && y1 == 400, "snap lands on the floor");

    // The E-a7.3 literal: two fractional targets that floor together give
    // one camera state — pinned INSIDE the clamp range (the b7 read's O1:
    // the original 10.4/10.6 pair was clamp-masked at 320 and carried no
    // floor-vs-round information; a round snap splits 410.4/410.6).
    camera.snapTo(410.4, 400.0);
    const int xa = camera.x();
    camera.snapTo(410.6, 400.0);
    pinAt(snapMatch, snapRan, camera.x() == xa && xa == 410,
          "410.4 vs 410.6 -> identical (a7.3 rule, in range)");

    // Zero deadzone: the camera centres on the target exactly (floored).
    camera.snapTo(577.9, 333.1);
    pinAt(snapMatch, snapRan, camera.x() == 577 && camera.y() == 333,
          "no deadzone: centre on the floored target");

    // The documented initial state: the map centre.
    ply::Camera2D fresh(defaultConfig());
    pinAt(snapMatch, snapRan, fresh.x() == 500 && fresh.y() == 400,
          "the camera starts at the map centre");
}

// --- the deadzone: no jitter, minimum move ----------------------------------

void testDeadzone() {
    ply::Camera2D camera(defaultConfig());  // deadzone 200x120
    camera.snapTo(500, 400);
    pinAt(deadzoneMatch, deadzoneRan, camera.x() == 500 && camera.y() == 400,
          "start centred at (500, 400)");

    // Inside the deadzone rect [400,600]x[340,460]: the camera NEVER moves.
    for (const auto& [tx, ty] : std::vector<std::pair<double, double>>{
             {450, 420}, {410, 350}, {590, 450}, {500.4, 400.6}, {499.6, 399.4}}) {
        camera.follow(tx, ty);
        pinAt(deadzoneMatch, deadzoneRan, camera.x() == 500 && camera.y() == 400,
              "target inside the deadzone (incl. fractional jitter): camera still");
    }

    // The float stops at the FLOOR in follow() too: 399.4 and 399.6 are the
    // same target (both floor to 399, crossing the edge to 499) — a round
    // snap would keep 399.6 at 500 and split the pair (the b5 F1 lesson:
    // injection design found the pin missing, so the pin landed first).
    {
        ply::Camera2D a(defaultConfig());
        a.snapTo(500, 400);
        a.follow(399.4, 400);
        ply::Camera2D b(defaultConfig());
        b.snapTo(500, 400);
        b.follow(399.6, 400);
        pinAt(deadzoneMatch, deadzoneRan,
              a.x() == 499 && b.x() == 499,
              "399.4 and 399.6 are one target in follow (floor, not round)");
    }

    // Crossing the right edge moves the camera the MINIMUM distance: the
    // target lands exactly on the deadzone edge (650 -> centre 550).
    camera.follow(650, 400);
    pinAt(deadzoneMatch, deadzoneRan, camera.x() == 550 && camera.y() == 400,
          "crossing the edge moves to the edge (centre 550, not 650)");
    camera.follow(650, 400);
    pinAt(deadzoneMatch, deadzoneRan, camera.x() == 550,
          "the target AT the edge is inside: no second move");
    camera.follow(660, 400);
    pinAt(deadzoneMatch, deadzoneRan, camera.x() == 560,
          "10 px past the edge moves the camera 10 px (minimum move)");
    camera.follow(655, 400);
    pinAt(deadzoneMatch, deadzoneRan, camera.x() == 560,
          "moving back inside moves nothing (no oscillation)");

    // The left edge, symmetric.
    camera.follow(400, 400);  // deadzone now [460, 660]; 400 is 60 left of 460
    pinAt(deadzoneMatch, deadzoneRan, camera.x() == 500,
          "left crossing moves the minimum (centre 500)");

    // Clamp wins over the deadzone at the map edge: the target wants 800,
    // the clamp allows 680.
    camera.follow(2000, 400);
    pinAt(deadzoneMatch, deadzoneRan, camera.x() == 680,
          "clamp wins over the deadzone at the right edge");
    camera.follow(900, 400);
    pinAt(deadzoneMatch, deadzoneRan, camera.x() == 680,
          "a further-out target still clamps at the edge");
}

// --- edges: small map, wide coordinates --------------------------------------

void testEdges() {
    // A map smaller than the view centres the camera on that axis.
    ply::Camera2D::Config small = defaultConfig();
    small.mapWidth = 320;
    small.mapHeight = 180;
    small.deadzoneWidth = 0;
    small.deadzoneHeight = 0;
    ply::Camera2D camera(small);
    camera.snapTo(-500, 9000);
    pinAt(edgeMatch, edgeRan, camera.x() == 160 && camera.y() == 90,
          "map smaller than the view: centred on the map");
    pinAt(edgeMatch, edgeRan,
          camera.viewRect() == ply::UIRect({-160, -90, 640, 360}),
          "small map: the view is larger than the map (centred, documented)");

    // The F1 lesson applied to coordinates: wide values order and clamp
    // exactly (a short-narrowed clamp fails).
    ply::Camera2D::Config wide = defaultConfig();
    wide.mapWidth = 100000;
    wide.mapHeight = 100000;
    wide.deadzoneWidth = 0;
    wide.deadzoneHeight = 0;
    ply::Camera2D big(wide);
    big.snapTo(99999.9, 12345.6);
    pinAt(edgeMatch, edgeRan, big.x() == 99680 && big.y() == 12345,
          "wide coordinates clamp and snap exactly (99680 = 100000 - 320)");
    big.snapTo(100000000.0, 0.0);
    pinAt(edgeMatch, edgeRan, big.x() == 99680 && big.y() == 180,
          "far targets clamp exactly at the wide edges");
}

// --- EVIDENCE ----------------------------------------------------------------

void printEvidence() {
    ply::Camera2D::Config config = defaultConfig();
    config.deadzoneWidth = 0;
    config.deadzoneHeight = 0;
    ply::Camera2D camera(config);
    camera.snapTo(500.4, 400.6);
    const int snapX = camera.x();
    camera.snapTo(500.6, 400.4);
    // The small-map probe, measured live (the b7 read's O2: the old
    // small_map=1 was a gated format-string constant).
    ply::Camera2D::Config small = defaultConfig();
    small.mapWidth = 320;
    small.mapHeight = 180;
    small.deadzoneWidth = 0;
    small.deadzoneHeight = 0;
    ply::Camera2D smallCamera(small);
    smallCamera.snapTo(-500, 9000);
    std::printf(
        "EVIDENCE camera2d view=640x360 clamp=%d/%d snap=%d/%d deadzone=%d/%d edge=%d/%d "
        "snap_repeat=%d small_cx=%d\n",
        clampMatch, clampRan, snapMatch, snapRan, deadzoneMatch, deadzoneRan, edgeMatch,
        edgeRan, camera.x() == snapX ? 1 : 0, smallCamera.x());
}

}  // namespace

int main() {
    testClampFourEdges();
    testSnap();
    testDeadzone();
    testEdges();

    if (failures == 0) {
        printEvidence();
        std::printf("camera2d tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "camera2d tests: %d failure(s)\n", failures);
    return 1;
}
