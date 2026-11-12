// TileAnimator (M1b b4, ARCHITECTURE_TARGET.md §3): per-tile frame sequences
// advanced from the sim step, with a per-cell phase offset so water does not
// pulse in lockstep. The contract — clock semantics, per-cell phase, loud
// unknowns, the steps<=0 defence — is documented in TileAnimator.hpp; this
// file is that contract, implemented in exact integer arithmetic.
#include "plyEngine/TileAnimator.hpp"

namespace ply {

namespace {
/// (step + tilePhase + cellPhase) reduced modulo cycle into [0, cycle) —
/// exact for any long long (each addend is reduced before summing, so
/// nothing overflows), and correct for negative inputs. The b2 F1 lesson:
/// a time value must not wrap at a reachable value, so the type is wide and
/// the reduction is mod, never a truncating cast.
long long reduce(long long step, long long tilePhase, long long cellPhase,
                 long long cycle) {
    const long long a = step % cycle;
    const long long b = tilePhase % cycle;
    const long long c = cellPhase % cycle;
    return (a + b + c + 2 * cycle) % cycle;
}

/// Frame steps with the loader-rejected <=0 state defended (clamped to 1):
/// a hand-built TileSet can carry it; a loaded one cannot (NonPositiveSize).
long long frameSteps(const TileAnimFrame& frame) {
    return frame.steps > 0 ? frame.steps : 1;
}
}  // namespace

TileAnimator TileAnimator::build(const TileSet& tileset) {
    TileAnimator a;
    a.tileset_ = tileset;
    return a;
}

int TileAnimator::phaseOf(int tileId) const {
    const TileData* tile = tileset_.findTile(tileId);
    return tile != nullptr ? tile->phase : 0;
}

long long TileAnimator::cycleOf(int tileId) const {
    const TileData* tile = tileset_.findTile(tileId);
    if (tile == nullptr) return 0;
    long long cycle = 0;
    for (const TileAnimFrame& frame : tile->animation) cycle += frameSteps(frame);
    return cycle;
}

int TileAnimator::frameIndexAt(int tileId, long long step, long long cellPhase) const {
    const TileData* tile = tileset_.findTile(tileId);
    if (tile == nullptr) return -1;              // loud: never a plausible frame
    if (tile->animation.empty()) return 0;       // static art: exactly one frame
    long long cycle = 0;
    for (const TileAnimFrame& frame : tile->animation) cycle += frameSteps(frame);
    const long long clock = reduce(step, tile->phase, cellPhase, cycle);
    long long start = 0;
    for (std::size_t i = 0; i < tile->animation.size(); ++i) {
        start += frameSteps(tile->animation[i]);
        if (clock < start) return static_cast<int>(i);
    }
    return static_cast<int>(tile->animation.size() - 1);  // unreachable: clock < cycle
}

UIRect TileAnimator::regionAt(int tileId, long long step, long long cellPhase) const {
    const TileData* tile = tileset_.findTile(tileId);
    if (tile == nullptr) return UIRect{};
    if (tile->animation.empty()) return tile->region;
    const int frame = frameIndexAt(tileId, step, cellPhase);
    // Defensive bounds (the a5.1 lesson, applied to the engine too): a
    // broken index must read as "no art", not crash the caller.
    if (frame < 0 || static_cast<std::size_t>(frame) >= tile->animation.size()) {
        return UIRect{};
    }
    return tile->animation[frame].region;
}

}  // namespace ply
