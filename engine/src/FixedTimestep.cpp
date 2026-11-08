#include "plyEngine/FixedTimestep.hpp"

#include <cmath>

namespace ply {

FixedTimestep::FixedTimestep(double fixedStep, int maxSubsteps,
                             double maxFrameTime)
    : fixedStep_(fixedStep > 0.0 ? fixedStep : 1.0 / 60.0),
      maxSubsteps_(maxSubsteps > 0 ? maxSubsteps : 1),
      maxFrameTime_(maxFrameTime > 0.0 ? maxFrameTime : 0.25) {}

void FixedTimestep::reset() { accumulator_ = 0.0; }

FixedTimestep::Advance FixedTimestep::advance(double frameTime) {
    if (frameTime < 0.0) {
        frameTime = 0.0;
    }
    // Clamp before accumulating so a long stall (debugger, breakpoint, window
    // drag) cannot queue up an unbounded burst of catch-up steps.
    if (frameTime > maxFrameTime_) {
        frameTime = maxFrameTime_;
    }

    accumulator_ += frameTime;

    int steps = static_cast<int>(accumulator_ / fixedStep_);
    if (steps > maxSubsteps_) {
        steps = maxSubsteps_;
        // Drop the backlog we refused to simulate, otherwise it grows forever.
        accumulator_ = fixedStep_ * static_cast<double>(maxSubsteps_);
    }
    accumulator_ -= fixedStep_ * static_cast<double>(steps);

    if (steps < 0) {
        steps = 0;
    }
    if (accumulator_ < 0.0) {
        accumulator_ = 0.0;
    }

    Advance out;
    out.steps = steps;
    out.alpha = accumulator_ / fixedStep_;
    if (out.alpha < 0.0) {
        out.alpha = 0.0;
    }
    if (out.alpha >= 1.0) {
        out.alpha = 0.999999;
    }
    return out;
}

}  // namespace ply
