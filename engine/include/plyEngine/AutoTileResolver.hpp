#pragma once

#include "plyEngine/TileSet.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace ply {

// AutoTileResolver (M1b b2, ARCHITECTURE_TARGET.md §3): "(neighbour mask,
// layer, tileset) -> tile id, using the blob-47 table; resolved at load,
// cached". Pure data — no texture, no GL, no window, no clock — so the whole
// unit is gradable in ctest.
//
// Scope, stated precisely:
// - The lookup input is the neighbour mask; `layer` is NOT yet an input.
//   Layer flags land with b3a (TileLayer), and until a tileset can declare
//   per-layer roles there is exactly one autotile table per tileset, so a
//   layer parameter would be a second spelling of the same answer. When b3a
//   lands, the resolver gains the layer as a table selector, additively.
// - Variant policy (operator decision 2026-11-11): FIRST-BY-ID. When several
//   tiles declare the same canonical mask, the lowest tile id wins. This is
//   deterministic, needs no extra state, and a richer policy (coordinate
//   hash, map-authored pick) can replace it later without touching the
//   table shape, because the choice is a pure function of the tileset.
// - Caching shape (same decision): the table is built once, at load, from
//   the TileSet. A query after that is canonicalise + a bounded binary
//   search over the 47 entries + one array read: constant-bounded with no
//   per-cell map lookup, which is what the "O(1) per cell" claim means
//   precisely (it is not literally one index read).

/// Neighbour bit for "the cell in this direction is the same terrain".
/// The conventional 8-way numbering is NOT used; these values are the ones
/// the tileset fixture's masks encode (kBlobMasks in TileSet.hpp) and are
/// part of the file format's contract.
inline constexpr int kNeighbourN = 1;
inline constexpr int kNeighbourNE = 2;
inline constexpr int kNeighbourE = 4;
inline constexpr int kNeighbourSE = 8;
inline constexpr int kNeighbourS = 16;
inline constexpr int kNeighbourSW = 32;
inline constexpr int kNeighbourW = 64;
inline constexpr int kNeighbourNW = 128;

/// Canonicalises any 0..255 neighbour mask onto the blob-47 set by DROPPING
/// a diagonal bit whose two adjacent edge bits are not both set: a diagonal
/// neighbour that does not share both its edges with the cell cannot be
/// represented by the 47-tile set, and inventing the missing edges would
/// claim terrain that is not there. The result is always canonical (only
/// diagonal bits are removed, edges are untouched, so no new violation can
/// appear). Input outside 0..255 returns kNoTileMask.
int canonicalizeBlobMask(int mask);

/// One resolution attempt. `ok()` is false when the (canonicalised) mask has
/// no tile in this tileset — the caller must draw nothing and say so rather
/// than draw a hole silently. `mask` is the canonical mask that was looked
/// up, so an error message can name it.
struct TileQuery {
    int mask = kNoTileMask;
    int tileId = -1;

    bool ok() const { return tileId >= 0; }
};

/// The resolved-at-load blob-47 table for one tileset.
class AutoTileResolver {
public:
    /// Builds the table from every tile carrying an autotile role
    /// (mask != kNoTileMask). Tiles without a role are ignored. Mask values
    /// were already validated as canonical at load (b1.2/b1.3), so this
    /// cannot see an unknown mask through the loader; a hand-built TileSet
    /// that bypasses the loader is canonicalised defensively. Building
    /// cannot fail — an incomplete set is reported by complete() /
    /// firstMissingMask() / a failed resolve(), not by aborting the build.
    static AutoTileResolver build(const TileSet& tileset);

    /// Resolves a raw neighbour mask: canonicalise, then look up. Never
    /// throws; `ok()` false means "no tile for this configuration".
    TileQuery resolve(int neighbourMask) const;

    /// True when every one of the 47 canonical masks has a tile — the
    /// `blob47-complete` lint gate's data, exposed here so b5/b10 and the
    /// art lint check the same fact the resolver uses.
    bool complete() const;

    /// The first canonical mask with no tile, or -1 when complete. Named so
    /// an error message can say which case is missing instead of "incomplete".
    int firstMissingMask() const;

    /// Number of canonical masks with a tile (0..47).
    std::size_t resolvedCount() const;

    /// Distinct tile ids claiming a mask for `canonicalMask` (0 when the
    /// mask has no tile). Exposed so the first-by-id policy is observable
    /// and so b5's art lint can flag a mask that only one variant covers.
    std::size_t variantCount(int canonicalMask) const;

    /// The table entry for a canonical mask index (0..46), or -1.
    int tileAt(std::size_t canonicalIndex) const;

    const TileSet& tileset() const { return tileset_; }

private:
    std::array<int, kBlobMaskCount> table_{};
    // The variant count is a std::size_t, not an unsigned char: a mask with
    // more than 255 claimants wrapped the old byte counter to 0, which
    // re-triggered the first-wins branch and silently returned a later tile
    // (the F1 defect the b2 verifier read measured; pinned by
    // testLargeVariantSet). A count must not wrap at a value that also means
    // "no claimants yet".
    std::array<std::size_t, kBlobMaskCount> variants_{};
    TileSet tileset_{};
};

/// Convenience: the neighbour mask for a cell from eight same-terrain bits.
int neighbourMaskFromBits(bool n, bool ne, bool e, bool se, bool s, bool sw, bool w, bool nw);

}  // namespace ply
