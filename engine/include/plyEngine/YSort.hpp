#pragma once

#include <cstddef>
#include <vector>

namespace ply {

// YSort (M1b b6, ARCHITECTURE_TARGET.md §3): "Sort key = feet Y (with a
// tiebreaker id) across props, NPCs, items, player" — the draw order of the
// y-sorted world layer ("WorldLayer (y-sorted): props, buildings(base),
// NPCs, player, items"). Pure data: no GL, no window, no clock, no float.
//
// Coordinate model (the WorldMap.hpp/PropLayer spelling, one rule): the
// world is y-UP — larger feet y is further back (up-screen), so BACK-TO-FRONT
// is DESCENDING feet y: the furthest-back item draws first, the front-most
// last. An item's sort key is its FEET (its pivot), never its art centre —
// a tall prop sorts at the ground line its art stands on (the b5 PropLayer
// contract: pivotY is the feet line).
//
// Tie-break: ascending id. Ids are the frame's identities (unique per
// frame), so the comparator is a TOTAL order and the output is identical on
// every run ("stable on repeat" is a property of the rule, not of the sort
// algorithm). Fully duplicate items (same feetY AND same id — outside the
// unique-id contract) keep insertion order, because the sort is stable.
//
// The output is a PERMUTATION of the input indices (each index exactly
// once, in draw order), so callers keep their own payloads and a broken
// sort cannot silently drop or duplicate an item.

/// One sortable thing in the world layer. `kind` is carried for callers and
/// fixtures (props/NPCs/items/player interleave in ONE order — the kind
/// never affects the sort).
struct YSortItem {
    int feetY = 0;
    int id = 0;

    bool operator==(const YSortItem&) const = default;
};

/// The draw order (back to front) as indices into `items`: descending
/// feetY, ties by ascending id. Empty input yields an empty order.
std::vector<std::size_t> ySortOrder(const std::vector<YSortItem>& items);

}  // namespace ply
