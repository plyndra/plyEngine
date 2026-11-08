// Headless fixed-timestep tests: no GL context required.

#include "plyEngine/FixedTimestep.hpp"

#include <cmath>
#include <cstdio>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

bool near(double a, double b, double eps = 1e-9) {
    return std::fabs(a - b) <= eps;
}

}  // namespace

int main() {
    const double step = 1.0 / 60.0;

    // --- one step per exact step-sized frame -------------------------------
    {
        ply::FixedTimestep ts;
        const auto a = ts.advance(step);
        check(a.steps == 1, "exact step produces 1 step");
        check(near(a.alpha, 0.0), "alpha is 0 after an exact step");
    }

    // --- 60 frames of 1/60s produce exactly 60 steps -----------------------
    {
        ply::FixedTimestep ts;
        int total = 0;
        for (int i = 0; i < 60; ++i) {
            total += ts.advance(step).steps;
        }
        check(total == 60, "60 frames of 1/60s == 60 steps");
    }

    // --- a large frame is split into multiple steps ------------------------
    {
        ply::FixedTimestep ts;
        const auto a = ts.advance(step * 3.0);
        check(a.steps == 3, "3x step frame produces 3 steps");
        check(near(a.alpha, 0.0), "alpha 0 with exact multiple");
    }

    // --- remaining time becomes the interpolation alpha --------------------
    {
        ply::FixedTimestep ts;
        const auto a = ts.advance(step * 1.5);
        check(a.steps == 1, "1.5 step frame produces 1 step");
        check(near(a.alpha, 0.5), "leftover time becomes alpha 0.5");
    }

    // --- alpha always stays in [0, 1) --------------------------------------
    {
        ply::FixedTimestep ts;
        bool inRange = true;
        for (int i = 0; i < 500; ++i) {
            const double dt = 0.001 * ((i % 37) + 1);  // irregular frame times
            const auto a = ts.advance(dt);
            if (a.alpha < 0.0 || a.alpha >= 1.0) {
                inRange = false;
            }
        }
        check(inRange, "alpha always within [0,1)");
    }

    // --- huge frame time is clamped (spiral-of-death guard) ---------------
    {
        ply::FixedTimestep ts(step, 5, 0.25);
        const auto a = ts.advance(10.0);  // 10 second stall
        check(a.steps <= 5, "substeps capped at maxSubsteps");
        check(a.steps >= 0, "non-negative steps");
    }

    // --- clamping prevents unbounded catch-up -----------------------------
    {
        ply::FixedTimestep ts(step, 5, 0.25);
        int total = 0;
        for (int i = 0; i < 100; ++i) {
            total += ts.advance(1.0).steps;  // every frame is a 1s stall
        }
        check(total <= 100 * 5, "catch-up is bounded by maxSubsteps per frame");
    }

    // --- zero and negative frame times are harmless -----------------------
    {
        ply::FixedTimestep ts;
        check(ts.advance(0.0).steps == 0, "zero dt produces no steps");
        check(ts.advance(-1.0).steps == 0, "negative dt produces no steps");
    }

    // --- reset clears the accumulator -------------------------------------
    {
        ply::FixedTimestep ts;
        ts.advance(step * 0.75);
        check(ts.accumulator() > 0.0, "accumulator accumulates");
        ts.reset();
        check(near(ts.accumulator(), 0.0), "reset clears accumulator");
    }

    // --- irregular frame times still average out correctly ----------------
    {
        ply::FixedTimestep ts;
        int total = 0;
        double simulated = 0.0;
        for (int i = 0; i < 1000; ++i) {
            const double dt = 0.005 + 0.001 * (i % 9);  // 5..13ms
            const auto a = ts.advance(dt);
            total += a.steps;
            simulated += a.steps * step;
        }
        // Total simulated time must not run ahead of wall time by a step.
        check(simulated <= 1000 * 0.013 + step, "never simulates more than elapsed");
    }

    if (failures == 0) {
        std::printf("timestep tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "timestep tests: %d failure(s)\n", failures);
    return 1;
}
