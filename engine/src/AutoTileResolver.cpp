#include "plyEngine/AutoTileResolver.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace ply {

namespace {

/// The corner constraint, applied once per diagonal: keep a diagonal bit only
/// when BOTH of its adjacent edge bits are set. Derived from the same rule
/// that defines kBlobMasks (b1.2/b1.3), so canonicalisation and the table
/// cannot disagree about what a representable configuration is.
constexpr int dropUnbackedDiagonals(int mask) {
    int out = mask;
    if (out & kNeighbourNE && !((out & kNeighbourN) && (out & kNeighbourE))) {
        out &= ~kNeighbourNE;
    }
    if (out & kNeighbourSE && !((out & kNeighbourE) && (out & kNeighbourS))) {
        out &= ~kNeighbourSE;
    }
    if (out & kNeighbourSW && !((out & kNeighbourS) && (out & kNeighbourW))) {
        out &= ~kNeighbourSW;
    }
    if (out & kNeighbourNW && !((out & kNeighbourW) && (out & kNeighbourN))) {
        out &= ~kNeighbourNW;
    }
    return out;
}

}  // namespace

int canonicalizeBlobMask(int mask) {
    if (mask < 0 || mask > 255) return kNoTileMask;
    // Dropping diagonals can never create a new violation: edges are
    // untouched and only diagonals are removed, so one pass suffices and the
    // result is always canonical (pinned as a closure property in E-b2.1).
    return dropUnbackedDiagonals(mask);
}

int neighbourMaskFromBits(bool n, bool ne, bool e, bool se, bool s, bool sw, bool w, bool nw) {
    int mask = 0;
    if (n) mask |= kNeighbourN;
    if (ne) mask |= kNeighbourNE;
    if (e) mask |= kNeighbourE;
    if (se) mask |= kNeighbourSE;
    if (s) mask |= kNeighbourS;
    if (sw) mask |= kNeighbourSW;
    if (w) mask |= kNeighbourW;
    if (nw) mask |= kNeighbourNW;
    return mask;
}

AutoTileResolver AutoTileResolver::build(const TileSet& tileset) {
    // Resolved at load (the roadmap's words, and the operator's caching
    // decision): one pass over the tiles, one entry per canonical mask. Every
    // query after this is an array index, so the render path is O(1) per
    // cell with no map lookups.
    AutoTileResolver resolver;
    resolver.tileset_ = tileset;
    resolver.table_.fill(-1);
    resolver.variants_.fill(0);

    // The loader guarantees canonical masks, but a hand-built TileSet can
    // bypass it, so the mask is canonicalised defensively here as well —
    // through the same function the query path uses, not a second copy of
    // the rule.
    for (const TileData& tile : tileset.tiles) {
        if (tile.mask == kNoTileMask) continue;  // no autotile role
        const int mask = canonicalizeBlobMask(tile.mask);
        const int index = blobMaskIndex(mask);
        if (index < 0) continue;  // unreachable via the loader
        const std::size_t slot = static_cast<std::size_t>(index);
        if (resolver.variants_[slot] == 0) {
            resolver.table_[slot] = tile.id;
        } else if (tile.id < resolver.table_[slot]) {
            // FIRST-BY-ID (operator decision 2026-11-11): the lowest tile id
            // wins, so the choice is deterministic and needs no extra state.
            resolver.table_[slot] = tile.id;
        }
        ++resolver.variants_[slot];
    }
    return resolver;
}

TileQuery AutoTileResolver::resolve(int neighbourMask) const {
    TileQuery query;
    const int mask = canonicalizeBlobMask(neighbourMask);
    query.mask = mask;
    if (mask == kNoTileMask) return query;  // not a mask at all
    const int index = blobMaskIndex(mask);
    if (index < 0) return query;  // unreachable: canonicalisation lands in the table
    query.tileId = table_[static_cast<std::size_t>(index)];
    return query;
}

bool AutoTileResolver::complete() const {
    return firstMissingMask() == -1;
}

int AutoTileResolver::firstMissingMask() const {
    for (int i = 0; i < kBlobMaskCount; ++i) {
        if (table_[static_cast<std::size_t>(i)] < 0) return kBlobMasks[i];
    }
    return -1;
}

std::size_t AutoTileResolver::resolvedCount() const {
    std::size_t count = 0;
    for (const int tileId : table_) {
        if (tileId >= 0) ++count;
    }
    return count;
}

std::size_t AutoTileResolver::variantCount(int canonicalMask) const {
    const int index = blobMaskIndex(canonicalMask);
    if (index < 0) return 0;
    return variants_[static_cast<std::size_t>(index)];
}

int AutoTileResolver::tileAt(std::size_t canonicalIndex) const {
    if (canonicalIndex >= static_cast<std::size_t>(kBlobMaskCount)) return -1;
    return table_[canonicalIndex];
}

}  // namespace ply
