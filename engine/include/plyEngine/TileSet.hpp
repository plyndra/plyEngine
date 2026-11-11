#pragma once

#include "plyEngine/MapLoader.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace ply {

// TileSet metadata schema (M1b b1.1, ADR 0003 + ARCHITECTURE_TARGET.md §3/§4):
// the b1 slice of the native JSON tileset format — the a9 static slice
// (MapLoader.hpp) extended with the per-tile metadata the roadmap names for
// `TileSet`: autotile mask, variant group, animation frames, per-tile phase
// offset, tile-local collision rects and palette id. Pure data — no texture,
// no GL, no window, no clock — so the whole unit is gradable in ctest.
//
// Grammar: same top-level keys as the a9 slice (name, image, tile_width,
// tile_height, tiles[]). Per tile, the a9 keys (id, region, solid) plus six
// OPTIONAL metadata keys, each with a default so an a9 slice document is a
// valid b1 document unchanged:
//
//   mask      int    neighbour mask in 0..255, or absent for no autotile role
//                    (kNoTileMask = -1). Bit n set = the neighbour in that
//                    direction belongs to the same terrain:
//                    1=N, 2=NE, 4=E, 8=SE, 16=S, 32=SW, 64=W, 128=NW.
//                    Validated against kBlobMasks (the 47 canonical blob
//                    masks): an in-range value outside the table is
//                    UnknownMask (b1.2) - no blob tile can represent it, so
//                    b2's resolver would never resolve it.
//   variant   string variant group name; empty = none. Tiles sharing a group
//                    are interchangeable fills for one terrain.
//   animation array  of {region, steps} frames; absent/empty = static art.
//                    steps >= 1 in fixed sim steps (FixedTimestep.hpp).
//   phase     int    per-tile phase offset in sim steps (TileAnimator, b4);
//                    default 0.
//   collision array  of tile-local px rects {x, y, w, h} that block movement;
//                    absent/empty = the whole tile blocks when solid, nothing
//                    when not. Refines, never replaces, the solid flag.
//   palette   string palette id from the asset manifests; empty = unassigned.
//
// Unknown keys (root or per tile) are ignored at load and dropped by emit,
// so a newer document still loads in an older build (a9's rule).
//
// Scope boundary: b1.1 lands the schema, the fixture and the round-trip;
// b1.2 the dedicated invalid-case battery and the canonical table; b1.3 the
// mask-table wiring (the shared canonical source above + the blob47 fixture
// whose tile i carries kBlobMasks[i]). The AutoTileResolver itself is b2.

/// No autotile role: the tile never participates in a blob resolution.
inline constexpr int kNoTileMask = -1;

/// The 47 canonical blob-47 neighbour masks, sorted ascending. A tile mask
/// other than kNoTileMask must be one of these — any other in-range value is
/// a configuration no blob tile can represent (a diagonal bit set without
/// both adjacent edges), so the resolver would never resolve it. The table
/// is pinned in the grader by its characterization: exactly the masks in
/// 0..255 where every set diagonal bit implies both adjacent edge bits
/// (NE ⇒ N∧E, SE ⇒ E∧S, SW ⇒ S∧W, NW ⇒ W∧N; bits 1=N, 2=NE, 4=E, 8=SE,
/// 16=S, 32=SW, 64=W, 128=NW).
inline constexpr int kBlobMaskCount = 47;
inline constexpr int kBlobMasks[kBlobMaskCount] = {
    0,   1,   4,   5,   7,   16,  17,  20,  21,  23,  28,  29,  31,
    64,  65,  68,  69,  71,  80,  81,  84,  85,  87,  92,  93,  95,
    112, 113, 116, 117, 119, 124, 125, 127, 193, 197, 199, 209, 213,
    215, 221, 223, 241, 245, 247, 253, 255,
};

// The mask table's SHARED canonical source (b1.3): the loader's UnknownMask
// check and b2's AutoTileResolver both go through these two functions, so
// the two sides cannot drift to two tables (plyengine::b13-b2-predecisions).
// b2 consumes blobMaskIndex as its canonical index in kBlobMasks order and
// must not re-derive a second table.

/// True when mask is one of the 47 canonical blob masks. kNoTileMask (-1,
/// "no autotile role") is NOT canonical - it is the absence of a role.
bool isCanonicalBlobMask(int mask);

/// The canonical index of mask in kBlobMasks (0..46), or -1 when the mask
/// is not canonical. The index is the resolver's table position: tile i of
/// the blob47 fixture carries kBlobMasks[i].
int blobMaskIndex(int mask);

/// One animation frame: a region in the tileset image and how many fixed
/// sim steps it holds before advancing.
struct TileAnimFrame {
    UIRect region;
    int steps = 0;  // >= 1 at load; NonPositiveSize otherwise

    bool operator==(const TileAnimFrame&) const = default;
};

/// Full tile metadata. `collision` rects are tile-local (origin = the tile's
/// top-left in art px) and clamp-checked against the tile size at load.
struct TileData {
    int id = 0;
    UIRect region;
    bool solid = false;
    int mask = kNoTileMask;
    std::string variant;
    std::vector<TileAnimFrame> animation;
    int phase = 0;
    std::vector<UIRect> collision;
    std::string palette;

    bool operator==(const TileData&) const = default;
};

/// The b1 tileset document. Tiles are sorted by id at load, so documents
/// that differ only in tile order (and key order) compare equal.
struct TileSet {
    std::string name;
    std::string image;
    int tileWidth = 0, tileHeight = 0;
    std::vector<TileData> tiles;

    bool operator==(const TileSet&) const = default;

    /// Tile lookup by id; nullptr when absent.
    const TileData* findTile(int id) const;
};

struct TileSetLoadResult {
    TileSet tileset;
    MapError error;

    bool ok() const { return error.ok(); }
    explicit operator bool() const { return ok(); }
};

// --- the function trio -----------------------------------------------------

/// Parses and validates one document, reusing the a9 error vocabulary plus
/// MaskOutOfRange for the schema's new range and UnknownMask for an
/// in-range mask no blob tile can represent (b1.2). Tile ids are rejected
/// below 0 (OutOfRangeTileId): layer cells use negative ids for empty, so a
/// tileset tile with a negative id could never be indexed. Returns a named
/// error and an empty result on failure. Unknown keys are ignored; emit
/// writes the canonical form (sorted keys, tiles by id) and omits every
/// metadata field still at its default, so an a9 slice document emits back
/// as exactly the a9 grammar.
TileSetLoadResult loadTileSetFromString(std::string_view document);
TileSetLoadResult loadTileSetFile(const std::string& path);

/// Canonical JSON: keys in sorted order (nlohmann's std::map), tiles in id
/// order, defaults omitted. load → emit → load is model-identical and emit
/// is a fixed point (E-b1.1).
std::string emitTileSet(const TileSet& tileset);

}  // namespace ply
