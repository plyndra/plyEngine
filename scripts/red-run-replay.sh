#!/usr/bin/env bash
# red-run-replay.sh — replay a unit's grader-first red run at THIS tree.
#
# Why: every "N failures against a stub" figure was measured before the
# implementation landed, so no verifier read could re-run it at a merged SHA
# — every read has flagged it as historical. This script makes the figure
# re-runnable by construction: it hollows the unit's implementation in
# place exactly as the grader-first scaffold was, builds, counts the
# grader's FAIL lines, and restores the tree AND the build (the b4 read's
# O1: the first version left build/ stub-linked). Replay figures for M1b:
#   worldmap      (b3b): 69 failures
#   neighbour     (b3c): 29 failures
#   tile_animator (b4):  34 failures
#   prop_layer    (b5):  46 failures
#   ysort         (b6):  12 failures
#   light_pass    (b8a+b8b): 22 failures
#   particles     (b9):  9 failures
#   autotile_map  (b3d): 10 failures
#   benchmark     (b10): 8 failures
#   camera2d      (b7):  30 failures (re-anchored from 29 when the
#                     b7 read's O1 re-pin moved the suite - the REPLAY-DIFF
#                     tripwire fired exactly as designed)
# The unit's own suite must be unchanged since the figure was recorded; a
# mismatch here is a loud discrepancy to investigate, not to adjust.
#
# Usage: scripts/red-run-replay.sh [...|autotile_map|benchmark|all]  (default all)
# Exit: 0 when every replayed figure matches, 1 on a mismatch, 2 on a dirty
# tree, 3 on a build failure (tree restored either way).

set -uo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

target="${1:-all}"
impl="engine/src/WorldMap.cpp"
anim_impl="engine/src/TileAnimator.cpp"
map_impl="engine/src/MapLoader.cpp"
ysort_impl="engine/src/YSort.cpp"
camera_impl="engine/src/Camera2D.cpp"
light_impl="engine/src/LightMap.cpp"
post_impl="engine/src/PostPass.cpp"
particles_impl="engine/src/ParticleSystem.cpp"
autotile_impl="engine/src/AutotileMap.cpp"
bench_impl="engine/src/Benchmark.cpp"

[ -z "$(git status --porcelain)" ] || {
    echo "red-run-replay: working tree is dirty; refusing" >&2
    exit 2
}

restore() { git checkout -- "$impl" "$anim_impl" "$map_impl" "$ysort_impl" "$camera_impl" "$light_impl" "$post_impl" "$particles_impl" "$autotile_impl" "$bench_impl" >/dev/null 2>&1; }
trap restore EXIT

build_and_count() {
    local binary="$1"
    if ! cmake --build build --target "$binary" -j8 >/tmp/red-run-build.log 2>&1; then
        echo "red-run-replay: BUILD-FAIL for $binary (see /tmp/red-run-build.log)" >&2
        return 1
    fi
    "./build/$binary" >/tmp/red-run-out.log 2>&1
    # The SUITE's own failure count (its "tests: N failure(s)" summary), not
    # the FAIL-line count: a suite may print more than one line per failure
    # (load-shim diagnostics), and the recorded figure is the suite's count.
    sed -n 's/.*tests: \([0-9][0-9]*\) failure(s).*/\1/p' /tmp/red-run-out.log | head -1
}

report() {
    local name="$1" measured="$2" expected="$3"
    if [ "$measured" = "$expected" ]; then
        echo "REPLAY-OK  $name: $measured failures (expected $expected)"
    else
        echo "REPLAY-DIFF $name: $measured failures (expected $expected)  <-- DISCREPANCY" >&2
        return 1
    fi
}

replay_worldmap() {
    restore  # replay from the pristine implementation, never from a leftover
    # The exact b3b scaffold: the map's metadata copied, no chunk grid, no
    # cell storage, empty submissions, hash() real (a leaf utility). Every
    # E-b3.1 pin fails cleanly against it — that is the 69.
    cat > "$impl" << 'STUB'
// WorldMap (M1b b3b) — STUB, grader-first scaffold (kept by
// scripts/red-run-replay.sh so the 69-failure red run stays re-runnable).
#include "plyEngine/WorldMap.hpp"

#include "plyEngine/StructuralHash.hpp"

#include <algorithm>

namespace ply {

namespace {
void hashQuad(StructuralHasher& h, const WorldMapQuad& q) {
    h.i32(q.tileId);
    h.i32(q.rect.x);
    h.i32(q.rect.y);
    h.i32(q.rect.w);
    h.i32(q.rect.h);
}
}  // namespace

std::uint64_t WorldMapSubmission::hash() const {
    StructuralHasher h;
    h.byte(0);
    h.i32(static_cast<std::int32_t>(below.size()));
    for (const WorldMapQuad& q : below) hashQuad(h, q);
    h.byte(1);
    h.i32(static_cast<std::int32_t>(above.size()));
    for (const WorldMapQuad& q : above) hashQuad(h, q);
    return h.value();
}

// --- stub: copies the map's metadata, stores no cells, submits nothing -------

WorldMap WorldMap::build(const MapData& map, const TilesetSlice& tileset) {
    WorldMap m;
    m.width_ = map.width;
    m.height_ = map.height;
    m.tileWidth_ = tileset.tileWidth;
    m.tileHeight_ = tileset.tileHeight;
    m.layerFlags_.clear();
    for (const MapLayer& layer : map.layers) m.layerFlags_.push_back(layer.flags);
    // Stub: the chunk grid and cell storage do not exist yet.
    m.chunkColumns_ = 0;
    m.chunkRows_ = 0;
    m.chunks_.clear();
    return m;
}

int WorldMap::width() const { return width_; }
int WorldMap::height() const { return height_; }
int WorldMap::tileWidth() const { return tileWidth_; }
int WorldMap::tileHeight() const { return tileHeight_; }
std::size_t WorldMap::layerCount() const { return layerFlags_.size(); }
unsigned WorldMap::layerFlags(std::size_t layer) const {
    return layer < layerFlags_.size() ? layerFlags_[layer] : 0u;
}

int WorldMap::chunkColumns() const { return chunkColumns_; }
int WorldMap::chunkRows() const { return chunkRows_; }
std::size_t WorldMap::chunkCount() const { return chunks_.size(); }

int WorldMap::tileAt(std::size_t, int, int) const { return -1; }
void WorldMap::setTile(std::size_t, int, int, int) {}

bool WorldMap::chunkDirty(int, int) const { return false; }
std::size_t WorldMap::chunkBuilds() const { return chunkBuilds_; }

WorldMapSubmission WorldMap::submit(const UIRect&) { return WorldMapSubmission{}; }

int WorldMap::neighbourMaskAt(std::size_t, int, int) const { return kNoTileMask; }

TileQuery WorldMap::resolveAt(std::size_t, int, int, const AutoTileResolver&) const {
    return TileQuery{};
}

void WorldMap::buildChunk(Chunk&, int, int) const {}
const WorldMap::Chunk* WorldMap::chunkAt(int, int) const { return nullptr; }
WorldMap::Chunk* WorldMap::chunkAt(int, int) { return nullptr; }

}  // namespace ply
STUB
    local n
    n="$(build_and_count ply_worldmap_test)" || exit 3
    restore
    cmake --build build --target ply_worldmap_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report worldmap "$n" 69
}

replay_neighbour() {
    restore  # replay from the pristine implementation, never from a leftover
    # The b3c scaffold: the b3b implementation kept REAL (the neighbour suite
    # pins chunking too), the two query bodies hollowed — the centre always
    # "no terrain" and resolveAt always "no tile", exactly as the scaffold.
    cp "$impl" /tmp/red-run-neighbour-backup.cpp
    sed -i 's/const int centre = tileAt(layer, cx, cy);/const int centre = -1;/' "$impl"
    sed -i 's/return resolver.resolve(neighbourMaskAt(layer, cx, cy));/return TileQuery{};/' "$impl"
    if diff -q /tmp/red-run-neighbour-backup.cpp "$impl" >/dev/null; then
        echo "red-run-replay: NO-OP hollowing (source drifted; refusing)" >&2
        exit 2
    fi
    local n
    n="$(build_and_count ply_neighbour_test)" || exit 3
    restore
    cmake --build build --target ply_neighbour_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report neighbour "$n" 29
}

replay_tile_animator() {
    restore  # replay from the pristine implementation, never from a leftover
    # The b4 scaffold: metadata copied, every query hollow (unknown tile
    # loud, nothing animates) — exactly as the 34-failure figure was
    # measured (the guard/differential pins pass by construction).
    cat > "$anim_impl" << 'STUB'
// TileAnimator (M1b b4) — STUB, grader-first scaffold (kept by
// scripts/red-run-replay.sh so the 34-failure red run stays re-runnable).
#include "plyEngine/TileAnimator.hpp"

namespace ply {

TileAnimator TileAnimator::build(const TileSet& tileset) {
    TileAnimator a;
    a.tileset_ = tileset;
    return a;
}

int TileAnimator::phaseOf(int) const { return 0; }

int TileAnimator::frameIndexAt(int, long long, long long) const { return -1; }

UIRect TileAnimator::regionAt(int, long long, long long) const { return UIRect{}; }

long long TileAnimator::cycleOf(int) const { return 0; }

}  // namespace ply
STUB
    local n
    n="$(build_and_count ply_tile_animator_test)" || exit 3
    restore
    cmake --build build --target ply_tile_animator_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report tile_animator "$n" 34
}

replay_prop_layer() {
    restore  # replay from the pristine implementation, never from a leftover
    # The b5 scaffold: the props key accepted and ignored on parse, nothing
    # emitted, PropLayer and the validator iterating nothing — exactly the
    # pre-implementation state the 46-failure figure was measured against.
    cp "$map_impl" /tmp/red-run-prop-backup.cpp
    sed -i 's/if (props != doc.end()) {/if (false) {/' "$map_impl"
    sed -i 's/if (!map.props.empty()) {/if (false) {/' "$map_impl"
    sed -i 's/for (const Prop& prop : map.props) {/for (const Prop\& prop : std::vector<Prop>{}) {/g' "$map_impl"
    if diff -q /tmp/red-run-prop-backup.cpp "$map_impl" >/dev/null; then
        echo "red-run-replay: NO-OP hollowing (source drifted; refusing)" >&2
        exit 2
    fi
    local n
    n="$(build_and_count ply_prop_layer_test)" || exit 3
    restore
    cmake --build build --target ply_prop_layer_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report prop_layer "$n" 46
}

replay_ysort() {
    restore  # replay from the pristine implementation, never from a leftover
    # The b6 scaffold: the sort returns nothing (the hollow sort the
    # 12-failure figure was measured against).
    cp "$ysort_impl" /tmp/red-run-ysort-backup.cpp
    sed -i 's/return order;/return {};/' "$ysort_impl"
    if diff -q /tmp/red-run-ysort-backup.cpp "$ysort_impl" >/dev/null; then
        echo "red-run-replay: NO-OP hollowing (source drifted; refusing)" >&2
        exit 2
    fi
    local n
    n="$(build_and_count ply_ysort_test)" || exit 3
    restore
    cmake --build build --target ply_ysort_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report ysort "$n" 12
}

replay_camera2d() {
    restore  # replay from the pristine implementation, never from a leftover
    # The b7 scaffold: the camera never moves and the view rect is empty —
    # exactly the hollow state the 29-failure figure was measured against.
    cat > "$camera_impl" << 'STUB'
// Camera2D (M1b b7) — STUB, grader-first scaffold (kept by
// scripts/red-run-replay.sh so the 29-failure red run stays re-runnable).
#include "plyEngine/Camera2D.hpp"

namespace ply {

Camera2D::Camera2D(const Config& config) : config_(config) {}

int Camera2D::x() const { return x_; }
int Camera2D::y() const { return y_; }

UIRect Camera2D::viewRect() const { return UIRect{}; }

void Camera2D::follow(double, double) {}

void Camera2D::snapTo(double, double) {}

void Camera2D::clampToMap() {}

}  // namespace ply
STUB
    local n
    n="$(build_and_count ply_camera2d_test)" || exit 3
    restore
    cmake --build build --target ply_camera2d_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report camera2d "$n" 30
}

replay_light_pass() {
    restore  # replay from the pristine implementation, never from a leftover
    # The b8 scaffold: both modules hollow (the schedule/field/hash and the
    # vignette do nothing) — exactly the pre-implementation state.
    cat > "$light_impl" << 'STUB'
// LightMap (M1b b8a) — STUB, grader-first scaffold (kept by
// scripts/red-run-replay.sh so the red run stays re-runnable).
#include "plyEngine/LightMap.hpp"

namespace ply {

LightMap::LightMap(const Config& config) : config_(config) {}

LightTint LightMap::ambientAt(long long) const { return LightTint{}; }

LightTint LightMap::sampleAt(int, int, long long) const { return LightTint{}; }

std::uint64_t LightMap::hashAt(long long) const { return 0; }

}  // namespace ply
STUB
    cat > "$post_impl" << 'STUB'
// PostPass (M1b b8b) — STUB, grader-first scaffold (kept by
// scripts/red-run-replay.sh so the red run stays re-runnable).
#include "plyEngine/PostPass.hpp"

namespace ply {

PostPass::PostPass(const Config& config) : config_(config) {}

bool PostPass::vignetteEnabled() const { return false; }

int PostPass::vignetteFactorAt(int, int) const { return 255; }

std::uint64_t PostPass::hash() const { return 0; }

}  // namespace ply
STUB
    local n
    n="$(build_and_count ply_light_pass_test)" || exit 3
    restore
    cmake --build build --target ply_light_pass_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report light_pass "$n" 22
}

replay_particles() {
    restore  # replay from the pristine implementation, never from a leftover
    # The b9 scaffold: the pool empty and every query hollow — exactly the
    # pre-implementation state the 9-failure figure was measured against.
    cat > "$particles_impl" << 'STUB'
// ParticleSystem (M1b b9) — STUB, grader-first scaffold (kept by
// scripts/red-run-replay.sh so the red run stays re-runnable).
#include "plyEngine/ParticleSystem.hpp"

namespace ply {

ParticleSystem::ParticleSystem(const Config& config) : config_(config) {}

void ParticleSystem::step() {}

long long ParticleSystem::stepCount() const { return 0; }

std::vector<ParticleSystem::Particle> ParticleSystem::particles() const { return {}; }

std::uint64_t ParticleSystem::stateHash() const { return 0; }

int ParticleSystem::nextJitter(std::size_t, int) { return 0; }

}  // namespace ply
STUB
    local n
    n="$(build_and_count ply_particles_test)" || exit 3
    restore
    cmake --build build --target ply_particles_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report particles "$n" 9
}

replay_autotile_map() {
    restore  # replay from the pristine implementation, never from a leftover
    # The b3d scaffold: no tables, no keys, nothing resolves — exactly the
    # pre-implementation state the 10-failure figure was measured against.
    cat > "$autotile_impl" << 'STUB'
// AutotileMap (M1b b3d) — STUB, grader-first scaffold (kept by
// scripts/red-run-replay.sh so the red run stays re-runnable).
#include "plyEngine/AutotileMap.hpp"

namespace ply {

AutotileMap AutotileMap::build(const MapData&, const TileSet&) { return AutotileMap{}; }

std::string AutotileMap::terrainKeyAt(std::size_t, int, int) const { return {}; }

int AutotileMap::terrainMaskAt(std::size_t, int, int) const { return kNoTileMask; }

TileQuery AutotileMap::resolvedAt(std::size_t, int, int) const { return TileQuery{}; }

WorldMapSubmission AutotileMap::submitResolved(const UIRect&) { return WorldMapSubmission{}; }

std::uint64_t AutotileMap::resolvedFieldHash(std::size_t) { return 0; }

}  // namespace ply
STUB
    local n
    n="$(build_and_count ply_autotile_map_test)" || exit 3
    restore
    cmake --build build --target ply_autotile_map_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report autotile_map "$n" 10
}

replay_benchmark() {
    restore  # replay from the pristine implementation, never from a leftover
    # The b10 scaffold: the harness emits nothing — exactly the
    # pre-implementation state the 8-failure figure was measured against.
    cat > "$bench_impl" << 'STUB'
// Benchmark (M1b b10) — STUB, grader-first scaffold (kept by
// scripts/red-run-replay.sh so the red run stays re-runnable).
#include "plyEngine/Benchmark.hpp"

namespace ply {

long long workP95(std::vector<long long>) { return 0; }

BenchmarkResult runBenchmark(AutotileMap&, const BenchmarkConfig&) {
    return BenchmarkResult{};
}

}  // namespace ply
STUB
    local n
    n="$(build_and_count ply_benchmark_test)" || exit 3
    restore
    cmake --build build --target ply_benchmark_test -j8 >/dev/null 2>&1 || {
        echo "red-run-replay: clean rebuild failed" >&2; exit 3; }
    report benchmark "$n" 8
}

status=0
case "$target" in
    worldmap)      replay_worldmap      || status=1 ;;
    neighbour)     replay_neighbour     || status=1 ;;
    tile_animator) replay_tile_animator || status=1 ;;
    prop_layer)    replay_prop_layer    || status=1 ;;
    ysort)         replay_ysort         || status=1 ;;
    camera2d)      replay_camera2d      || status=1 ;;
    light_pass)    replay_light_pass    || status=1 ;;
    particles)     replay_particles     || status=1 ;;
    autotile_map)  replay_autotile_map  || status=1 ;;
    benchmark)     replay_benchmark     || status=1 ;;
    all)
        replay_worldmap      || status=1
        replay_neighbour     || status=1
        replay_tile_animator || status=1
        replay_prop_layer    || status=1
        replay_ysort         || status=1
        replay_camera2d      || status=1
        replay_light_pass    || status=1
        replay_particles     || status=1
        replay_autotile_map  || status=1
        replay_benchmark     || status=1
        ;;
    *) echo "red-run-replay: unknown target '$target'" >&2; exit 2 ;;
esac
exit $status
