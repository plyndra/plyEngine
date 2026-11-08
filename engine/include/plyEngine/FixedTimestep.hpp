#pragma once

namespace ply {

/// Fixed-timestep accumulator. Pure math (no GL), so it is unit-testable.
///
/// advance() converts a variable frame delta into a whole number of fixed
/// simulation steps plus an interpolation alpha for rendering:
///   render_alpha = accumulator / fixedStep  in [0, 1)
class FixedTimestep {
public:
    struct Advance {
        int steps = 0;   ///< fixed steps to run this frame
        double alpha = 0.0;  ///< interpolation factor, [0, 1)
    };

    /// fixedStep: seconds per simulation step (e.g. 1.0/60.0).
    /// maxSubsteps: cap on steps per frame (spiral-of-death guard).
    /// maxFrameTime: frame deltas are clamped to this before accumulating.
    FixedTimestep(double fixedStep = 1.0 / 60.0, int maxSubsteps = 5,
                  double maxFrameTime = 0.25);

    Advance advance(double frameTime);
    void reset();

    double step() const { return fixedStep_; }
    int maxSubsteps() const { return maxSubsteps_; }
    double maxFrameTime() const { return maxFrameTime_; }
    double accumulator() const { return accumulator_; }

private:
    double fixedStep_;
    int maxSubsteps_;
    double maxFrameTime_;
    double accumulator_ = 0.0;
};

}  // namespace ply
