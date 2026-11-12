#pragma once

#include "plyEngine/AutoTileResolver.hpp"
#include "plyEngine/MapLoader.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace ply {

// WorldMap (M1b b3b, ARCHITECTURE_TARGET.md §3): "Chunked storage (e.g. 16×16
// cells), per-chunk dirty flags, view-frustum culling" — plus the two jobs
// b3a's layer flags exist for (kLayerOpaque coverage culling and the
// below/above-entities draw passes). Pure data — no texture, no GL, no
// window, no clock — so the whole unit is gradable in ctest.
//
// Scope, stated precisely:
// - Coordinate model is a9's, spelled once (the Minimap.hpp precedent): a
//   cell (cx, cy) covers {cx*tileWidth, (height-1-cy)*tileHeight,
//   tileWidth, tileHeight} world px; the world is y-up (OrthoCamera.hpp) and
//   row 0 is the map's TOP row. The camera rect passed to submit() is world
//   px, y-up, integer — a caller holding float world positions snaps them
//   first (a7's snapCameraAxis, the one sanctioned double).
// - A "tile" is a non-empty cell (id >= 0). Empty cells submit nothing.
//   "Quads submitted == tiles intersecting the camera rect" (E-b3.1) means
//   EXACTLY the non-empty cells whose world rect overlaps the camera rect in
//   positive area: edge-touching is not intersection (a zero-area sliver
//   draws no pixels), and the count is exact in both directions — no
//   visible tile dropped, no off-screen tile submitted.
// - Opaque coverage (kLayerOpaque): an opaque layer's non-empty cells fully
//   cover the same cell of every layer drawn BENEATH them — tiles share one
//   16 px grid, so same-cell coverage is full coverage. Covered cells submit
//   nothing. An EMPTY cell of an opaque layer covers nothing, and an opaque
//   layer never culls anything drawn after it. The exact-count criterion
//   above then counts tiles intersecting the camera rect AND actually drawn.
// - Passes (kLayerAboveEntities): a submission is two passes —
//   below-entities (ground, drawn under the entity layer) then
//   above-entities (canopies, roofs, drawn over it). MapLayer::aboveEntities()
//   is the selector: a layer with no above bit draws below, including a
//   layer whose flags are only kLayerAnimated or a hand-built 0. Within a
//   pass, layers keep authored order; draw order matters wherever two
//   layers carry the same cell, and every such pair shares a chunk and
//   keeps layer order, so overlaps resolve correctly.
// - Submission order (part of the contract, hashed by ADR 0014): within a
//   pass, layers in authored order; within a layer, chunks top-to-bottom,
//   left-to-right; within a chunk, cells row-major (top row first).
// - Chunking: kChunkSize × kChunkSize cells per chunk (ARCHITECTURE_TARGET.md
//   §3's "e.g. 16×16 cells"; 16 = one screen is 40×22.5 tiles at ADR 0012's
//   internal resolution, so a chunk is a small, cacheable slab of a screen).
//   Storage is per chunk; setTile marks exactly its chunk dirty; submit()
//   rebuilds each dirty chunk it touches exactly once and reuses clean ones
//   (chunkBuilds() is the observability counter — UILayoutCache::recomputes()
//   is the precedent). A dirty chunk outside the camera is not built.
// - Chunk seams, stated plainly: b3b pins TRAVERSAL at chunk seams — a
//   camera spanning a boundary submits exactly the tiles the brute-force
//   scan finds, none dropped and none duplicated across the seam. b3c adds
//   the RESOLUTION seam half (E-b3.2's "chunk-boundary fixture produces no
//   seam in resolved ids"): the cross-chunk neighbour queries below feed
//   AutoTileResolver, and a terrain region crossing a chunk border resolves
//   to exactly the ids it would resolve to with no chunks at all.
// - kLayerAnimated is inert until b4 (TileAnimator) — it is carried and
//   visible in layerFlags() but changes no submission here.

/// Cells per chunk side. The chunk grid is ceil(width/kChunkSize) ×
/// ceil(height/kChunkSize); edge chunks are partially filled (out-of-map
/// cells read as empty).
inline constexpr int kChunkSize = 16;

/// One submitted quad: the world-px rect it covers (a9's y-up cell rect),
/// the tile drawn, and the cell it came from (for structural hashing and
/// diagnostics).
struct WorldMapQuad {
    int tileId = -1;
    int cellX = 0, cellY = 0;
    /// The source layer index (b3d: a resolved submission must know which
    /// layer's terrain table to consult). Not part of the hash wires.
    std::size_t layer = 0;
    UIRect rect{};

    bool operator==(const WorldMapQuad&) const = default;
};

/// The quads one submit() draws, split by pass. Draw below, then the entity
/// layer, then above. Within each pass the order is the submission order
/// above.
struct WorldMapSubmission {
    std::vector<WorldMapQuad> below;
    std::vector<WorldMapQuad> above;

    /// Culled tile-id list hash (ADR 0014): FNV-1a 64 over the submission AS
    /// ORDERED — a pass marker and the pass length, then per quad (tileId,
    /// x, y, w, h) as little-endian i32. Deterministic on any host, moves
    /// when any submitted quad moves, appears, or disappears.
    std::uint64_t hash() const;
};

class WorldMap {
public:
    /// Chunks a loaded map; tileWidth/tileHeight come from its tileset (the
    /// mapTileQuads signature). Every chunk starts dirty (its derived quad
    /// lists are not built until a submit() needs them).
    static WorldMap build(const MapData& map, const TilesetSlice& tileset);

    /// Map size in cells; cell (0, 0) is the map's TOP-left.
    int width() const;
    int height() const;
    /// Cell size in world px.
    int tileWidth() const;
    int tileHeight() const;
    std::size_t layerCount() const;
    /// The b3a flag bitmask of one layer; 0 when the index is out of range.
    unsigned layerFlags(std::size_t layer) const;

    /// Chunk grid: ceil(width / kChunkSize) × ceil(height / kChunkSize).
    int chunkColumns() const;
    int chunkRows() const;
    std::size_t chunkCount() const;

    /// Cell read (file coords: cy = 0 is the top row). Out-of-range layer or
    /// cell reads as -1 (empty) rather than asserting — this is a data
    /// module, and a caller probing past an edge gets "no tile".
    int tileAt(std::size_t layer, int cx, int cy) const;

    /// Cell write; marks exactly the containing chunk dirty. Out-of-range
    /// writes are ignored (same probing rule as tileAt).
    void setTile(std::size_t layer, int cx, int cy, int tileId);

    /// Dirty flag: true when a cell in chunk (chunkX, chunkY) changed since
    /// that chunk's derived quad lists were last built — including at build
    /// time, where no chunk's lists exist yet. Out-of-range reads false.
    bool chunkDirty(int chunkX, int chunkY) const;

    /// How many chunk builds have run since build(). submit() builds each
    /// dirty chunk it touches exactly once; repeat submits with no edits and
    /// submits over clean chunks must not move this (the "reuse, don't
    /// recompute" pin, UILayoutCache::recomputes() precedent).
    std::size_t chunkBuilds() const;

    /// Every quad that intersects `camera` (positive-area overlap) and is
    /// not covered by a later opaque layer, split into the two passes and
    /// ordered as documented above. Non-const because it maintains the
    /// chunk cache (UILayoutCache::rects() is the precedent).
    WorldMapSubmission submit(const UIRect& camera);

    // --- cross-chunk neighbour queries (b3c, E-b3.2) -----------------------

    /// The raw 8-neighbour same-terrain mask for the cell: bit kNeighbourN..
    /// kNeighbourNW (AutoTileResolver.hpp) set when that neighbour is in the
    /// map and holds the same non-negative tile id as this cell. Directions
    /// are map-grid: N is the row above (cy-1, world +y), NE is (cx+1,
    /// cy-1), and so on around the compass.
    ///
    /// - Same terrain is TILE-ID EQUALITY at this level: WorldMap holds the
    ///   a9 slice, which carries no variant data. TileSet.hpp variant groups
    ///   ("interchangeable fills for one terrain") arrive when the render
    ///   pipeline threads the full TileSet through; the no-seam property is
    ///   identity-rule-independent — it is about where the reads come from
    ///   (any cell of any chunk), not what they compare.
    /// - A neighbour OUTSIDE the map is not the same terrain: the map edge
    ///   is a terrain edge (pinned at corners).
    /// - The query reports the RAW truth: a same-terrain diagonal sets its
    ///   bit even when its edge neighbours differ. Canonicalisation onto the
    ///   blob-47 set is canonicalizeBlobMask's job (b2), at resolve time.
    /// - An empty centre or an out-of-range cell has no terrain and no mask:
    ///   kNoTileMask.
    int neighbourMaskAt(std::size_t layer, int cx, int cy) const;

    /// The autotiling composition this unit exists for:
    /// resolver.resolve(neighbourMaskAt(layer, cx, cy)) — one call so the
    /// render path cannot wire the two stages differently per caller. A
    /// failed resolve (absent mask, or an empty/out-of-range cell) is
    /// returned as-is: ok() false, tileId -1, the canonical mask named —
    /// loud failure, never a fallback tile (b2's contract: an unknown case
    /// errors rather than drawing a hole). `layer` is the map layer holding
    /// terrain; the RESOLVER stays per-tileset — its own layer selector
    /// still waits on per-layer tileset roles (b2's scope note).
    TileQuery resolveAt(std::size_t layer, int cx, int cy,
                        const AutoTileResolver& resolver) const;

private:
    struct Chunk {
        // Raw cells, layer-major: [layer][cy*kChunkSize + cx] within the
        // chunk; out-of-map cells are -1.
        std::vector<std::vector<int>> cells;
        // Derived per layer: the chunk's drawn quads (non-empty cells not
        // covered by a later opaque layer), in cell order within the chunk.
        // Rebuilt while dirty.
        std::vector<std::vector<WorldMapQuad>> quads;
        bool dirty = true;
    };

    void buildChunk(Chunk& chunk, int chunkX, int chunkY) const;
    const Chunk* chunkAt(int chunkX, int chunkY) const;
    Chunk* chunkAt(int chunkX, int chunkY);

    int width_ = 0, height_ = 0;
    int tileWidth_ = 0, tileHeight_ = 0;
    std::vector<unsigned> layerFlags_;
    int chunkColumns_ = 0, chunkRows_ = 0;
    // Chunk grid, row-major: [chunkY*chunkColumns_ + chunkX].
    std::vector<Chunk> chunks_;
    std::size_t chunkBuilds_ = 0;
};

}  // namespace ply
