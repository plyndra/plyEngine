#!/usr/bin/env bash
# red-run-replay.sh — replay a unit's grader-first red run at THIS tree.
#
# Why: every "N failures against a stub" figure was measured before the
# implementation landed, so no verifier read could re-run it at a merged SHA
# — every read has flagged it as historical. This script makes the figure
# re-runnable by construction: it hollows the unit's implementation in
# place exactly as the grader-first scaffold was, builds, counts the
# grader's FAIL lines, and restores the tree. Replay figures for M1b:
#   worldmap  (b3b): 69 failures
#   neighbour (b3c): 29 failures
# The unit's own suite must be unchanged since the figure was recorded; a
# mismatch here is a loud discrepancy to investigate, not to adjust.
#
# Usage: scripts/red-run-replay.sh [worldmap|neighbour|all]   (default all)
# Exit: 0 when every replayed figure matches, 1 on a mismatch, 2 on a dirty
# tree, 3 on a build failure (tree restored either way).

set -uo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

target="${1:-all}"
impl="engine/src/WorldMap.cpp"

[ -z "$(git status --porcelain)" ] || {
    echo "red-run-replay: working tree is dirty; refusing" >&2
    exit 2
}

restore() { git checkout -- "$impl" >/dev/null 2>&1; }
trap restore EXIT

build_and_count() {
    local binary="$1"
    if ! cmake --build build --target "$binary" -j8 >/tmp/red-run-build.log 2>&1; then
        echo "red-run-replay: BUILD-FAIL for $binary (see /tmp/red-run-build.log)" >&2
        exit 3
    fi
    "./build/$binary" >/tmp/red-run-out.log 2>&1
    grep -c 'FAIL:' /tmp/red-run-out.log
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
    n="$(build_and_count ply_worldmap_test)"
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
    n="$(build_and_count ply_neighbour_test)"
    report neighbour "$n" 29
}

status=0
case "$target" in
    worldmap)  replay_worldmap  || status=1 ;;
    neighbour) replay_neighbour || status=1 ;;
    all)
        replay_worldmap  || status=1
        replay_neighbour || status=1
        ;;
    *) echo "red-run-replay: unknown target '$target'" >&2; exit 2 ;;
esac
exit $status
