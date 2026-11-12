// YSort (M1b b6, E-b6.1): "y-sort order array equals fixture; stable across
// runs (id tiebreak)" — ARCHITECTURE_TARGET.md §3's "Sort key = feet Y (with
// a tiebreaker id) across props, NPCs, items, player" and the must-prove "a
// fixture with crossing entities and a tall prop yields the expected order
// array; identical on repeat (ties broken by id)".
//
// Pinned: the literal order array of a crossing fixture (entities whose
// order differs from insertion order, a tall prop sorting at its FEET);
// descending feet y (back to front in y-up) with ascending-id ties;
// permutation completeness (each index exactly once — a broken sort cannot
// drop or duplicate an item); run-to-run identity; wide and negative feet y
// (the F1 lesson: values past any narrow type's ceiling); and the tall prop
// composited from PropLayer — its sort key is the feet line, not the art
// centre.
//
// House rules: pure data. No GL, no window, no clock, no float.

#include "plyEngine/YSort.hpp"

#include "plyEngine/MapLoader.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

namespace {

int failures = 0;

// Live EVIDENCE counters (each incremented by the pins that ran).
int orderMatch = 0, orderRan = 0;
int tieMatch = 0, tieRan = 0;
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

/// True when `got` is a permutation of 0..n-1 (each index exactly once).
bool isPermutation(const std::vector<std::size_t>& got, std::size_t n) {
    if (got.size() != n) return false;
    std::vector<bool> seen(n, false);
    for (std::size_t index : got) {
        if (index >= n || seen[index]) return false;
        seen[index] = true;
    }
    return true;
}

std::string showOrder(const std::vector<std::size_t>& order) {
    std::string out;
    for (std::size_t index : order) out += std::to_string(index) + ",";
    return out.empty() ? out : out.substr(0, out.size() - 1);
}

// --- the fixture: crossing entities and a tall prop --------------------------

// Input order is SCRAMBLED on purpose: ids 4,5,1,3,2 with feetY
// 30, 80, 50, 50, 80 — the draw order is neither insertion nor id order.
// Kinds interleave: prop(4), npc(5), player(1), item(3), npc(2).
const std::vector<ply::YSortItem>& fixture() {
    static const std::vector<ply::YSortItem> items = {
        {30, 4},  // the tall prop (feet 30; its art rises to 78)
        {80, 5},  // npc behind
        {50, 1},  // player
        {50, 3},  // item at the player's feet (tie -> id order)
        {80, 2},  // npc behind (tie with 5 -> id order)
    };
    return items;
}

void testOrderArray() {
    // The expected array, literal: back to front is DESCENDING feet y
    // (80s first, then 50s, then 30), ties by ASCENDING id:
    // id order 2, 5, 1, 3, 4 -> indices 4, 1, 2, 3, 0.
    const std::vector<std::size_t> expected = {4, 1, 2, 3, 0};
    const std::vector<std::size_t> got = ply::ySortOrder(fixture());
    pinAt(orderMatch, orderRan, got == expected,
          "fixture order array matches (crossing entities + tall prop)");
    pinAt(orderMatch, orderRan, isPermutation(got, fixture().size()),
          "the order is a permutation of the input indices");

    // Stable on repeat: the same input yields the same array, run to run.
    pinAt(orderMatch, orderRan,
          ply::ySortOrder(fixture()) == expected && ply::ySortOrder(fixture()) == got,
          "identical on repeat (the rule is total; ties are decided by id)");

    // The direction is back-to-front in the y-up world: the largest feet y
    // (furthest back) draws FIRST. A flip to ascending fails here. (Guarded
    // size-first: a hollow sort returns empty and must FAIL, not crash —
    // the a5.1 lesson, fifth strike on first red run.)
    pinAt(orderMatch, orderRan, got.size() == 5 && got.front() == 4 && got.back() == 0,
          "descending feet y: id 2 (y 80) first, the tall prop (y 30) last");

    // The tall prop sorts at its FEET, never its art centre (its art rises
    // from y 30 to 78 — a centre-based key would place it among the 50s).
    pinAt(orderMatch, orderRan, got.size() == 5 && got.back() == 0,
          "the tall prop is LAST: its feet (30), not its art centre (54)");
}

void testTies() {
    // Equal feet y -> ascending id, whatever the insertion order.
    const std::vector<ply::YSortItem> scrambled = {
        {50, 9}, {50, 2}, {50, 7}, {50, 1}, {50, 5},
    };
    const std::vector<std::size_t> got = ply::ySortOrder(scrambled);
    pinAt(tieMatch, tieRan,
          got == std::vector<std::size_t>({3, 1, 4, 2, 0}),
          "ties sort by ascending id (1, 2, 5, 7, 9)");
    pinAt(tieMatch, tieRan, isPermutation(got, scrambled.size()),
          "tie fixture is a permutation");

    // Fully duplicate items (same feetY AND id — outside the unique-id
    // contract) keep insertion order: the sort is stable.
    const std::vector<ply::YSortItem> dupes = {
        {50, 1}, {50, 1}, {50, 1}, {50, 1},
    };
    pinAt(tieMatch, tieRan,
          ply::ySortOrder(dupes) == std::vector<std::size_t>({0, 1, 2, 3}),
          "full duplicates keep insertion order (stable sort)");

    // Empty and single inputs.
    pinAt(edgeMatch, edgeRan, ply::ySortOrder({}).empty(), "empty input, empty order");
    pinAt(edgeMatch, edgeRan,
          ply::ySortOrder({{7, 1}}) == std::vector<std::size_t>({0}), "single item -> [0]");
}

void testWideAndNegativeKeys() {
    // The F1 lesson applied to the sort key: feet y values past every narrow
    // type's ceiling must order exactly (a short-narrowed comparator fails).
    const std::vector<ply::YSortItem> wide = {
        {1, 10}, {2147483647, 11}, {65536, 12}, {-2147483647 - 1, 13}, {300, 14},
    };
    // Descending: 2^31-1 (idx 1), 65536 (idx 2), 300 (idx 4), 1 (idx 0),
    // INT_MIN (idx 3).
    const std::vector<std::size_t> expected = {1, 2, 4, 0, 3};
    const std::vector<std::size_t> got = ply::ySortOrder(wide);
    pinAt(edgeMatch, edgeRan, got == expected, "wide and negative feet y order exactly");
    pinAt(edgeMatch, edgeRan, isPermutation(got, wide.size()), "wide fixture is a permutation");
}

void testTallPropComposition() {
    // The sort key comes from the b5 PropLayer contract: a tall prop's
    // pivotY (its feet), through the documented y flip. Two props (a 2x3
    // oak at doc (48, 96) -> feet world 32; a 1x1 sign at (120, 100) -> feet
    // 28) plus a player at feet 30: the oak (feet 32) draws first even
    // though the player's y 30 is inside the oak's art (32..80).
    ply::TilesetSlice ts;
    ts.name = "t";
    ts.image = "t.png";
    ts.tileWidth = 16;
    ts.tileHeight = 16;
    ply::TileDef def;
    def.id = 2;
    def.region = {0, 0, 16, 16};
    ts.tiles.push_back(def);

    ply::MapData map;
    map.name = "m";
    map.tileset = "t";
    map.width = 10;
    map.height = 8;  // mapHeightPx 128
    ply::MapLayer ground;
    ground.name = "ground";
    ground.tiles.assign(80, 0);
    map.layers.push_back(ground);
    ply::Prop oak;
    oak.name = "oak";
    oak.tile = 2;
    oak.x = 48;
    oak.y = 96;
    oak.widthTiles = 2;
    oak.heightTiles = 3;
    map.props.push_back(oak);
    ply::Prop sign;
    sign.name = "sign";
    sign.tile = 2;
    sign.x = 120;
    sign.y = 100;
    map.props.push_back(sign);

    const ply::PropLayer props = ply::PropLayer::build(map, ts);
    std::vector<ply::YSortItem> items = {
        {props.pivotY(0), 100},  // the oak (tall: art 32..80, feet 32)
        {30, 200},               // the player inside the oak's art
        {props.pivotY(1), 300},  // the sign (feet 28)
    };
    const std::vector<std::size_t> got = ply::ySortOrder(items);
    // Feet: oak 32, player 30, sign 28 -> back to front: oak, player, sign.
    pinAt(orderMatch, orderRan,
          got == std::vector<std::size_t>({0, 1, 2}),
          "PropLayer feet lines drive the sort (oak 32, player 30, sign 28)");
    pinAt(orderMatch, orderRan,
          props.pivotY(0) == 32 && items[0].feetY == 32,
          "the tall prop's key is its feet line (32), not its art centre (56)");
}

// --- EVIDENCE ----------------------------------------------------------------

void printEvidence() {
    const std::vector<std::size_t> order = ply::ySortOrder(fixture());
    std::printf("EVIDENCE ysort items=%zu order=%s orderpins=%d/%d ties=%d/%d edge=%d/%d "
                "repeat=identical permutation=1 tall=feet\n",
                fixture().size(), showOrder(order).c_str(), orderMatch, orderRan, tieMatch,
                tieRan, edgeMatch, edgeRan);
}

}  // namespace

int main() {
    testOrderArray();
    testTies();
    testWideAndNegativeKeys();
    testTallPropComposition();

    if (failures == 0) {
        printEvidence();
        std::printf("ysort tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "ysort tests: %d failure(s)\n", failures);
    return 1;
}
