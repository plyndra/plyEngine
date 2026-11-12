// YSort (M1b b6, ARCHITECTURE_TARGET.md §3): the y-sorted world layer's
// draw order. The contract — back-to-front descending feet y in the y-up
// world, ascending-id ties, permutation output — is documented in
// YSort.hpp; this file is that contract, implemented.
#include "plyEngine/YSort.hpp"

#include <algorithm>

namespace ply {

std::vector<std::size_t> ySortOrder(const std::vector<YSortItem>& items) {
    std::vector<std::size_t> order;
    order.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) order.push_back(i);
    // Back to front in y-up: DESCENDING feet y (the furthest back draws
    // first); ties by ASCENDING id. The id tiebreak makes the comparator a
    // total order on unique ids, so the output is identical on every run;
    // stable_sort additionally preserves insertion order for full
    // duplicates (outside the unique-id contract).
    std::stable_sort(order.begin(), order.end(),
                     [&items](std::size_t a, std::size_t b) {
                         if (items[a].feetY != items[b].feetY) {
                             return items[a].feetY > items[b].feetY;
                         }
                         return items[a].id < items[b].id;
                     });
    return order;
}

}  // namespace ply
