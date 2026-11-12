#pragma once

#include "plyEngine/StructuralHash.hpp"

#include <cstdint>
#include <vector>

namespace ply {

// ParticleSystem (M1b b9, ROADMAP_TARGET.md: "ParticleSystem (seeded,
// fixed-step, visual-only)"; E-b9.1: "same seed -> identical particle
// positions; gameplay state hash unchanged with particles on"). Pure
// integer data with a seeded LCG: no GL, no window, no clock object, no
// float.
//
// Model (documented once, pinned literally):
// - SEEDED: all spawn jitter comes from a documented 64-bit LCG seeded
//   once per system; the same seed and config produce the same state at
//   every step, on any host (integer math only).
// - FIXED-STEP: the caller advances the system one sim step at a time
//   (FixedTimestep's counter); the state is a pure function of
//   (seed, config, stepCount). Each step: every live particle advances
//   position += velocity, velocity += gravity (y-up: a NEGATIVE gravity
//   pulls toward smaller y, i.e. down-screen), and its remaining lifetime
//   decreases by one; at end of life it respawns at its emitter with a
//   fresh seeded jitter — so the pool size is constant and the cycle is
//   deterministic.
// - VISUAL-ONLY: the system owns its own state and touches nothing else.
//   The E-b9.1 second clause pins exactly that: a gameplay state hash is
//   unchanged before and after driving the system.
// - The step count is long long (the b2 F1 lesson: never narrow enough to
//   wrap at a reachable value).

struct ParticleEmitter {
    int x = 0, y = 0;                 // the spawn point, world px (y-up)
    int spreadX = 0, spreadY = 0;     // +- spawn jitter (seeded)
    int velocityX = 0, velocityY = 0; // initial velocity, px per step
    int gravity = 0;                  // added to velocityY each step
    int lifetime = 1;                 // steps before respawn; <= 0 clamps to 1
    int maxParticles = 0;             // this emitter's pool size; <= 0 clamps
                                      // to an empty pool (defensive — the b9
                                      // read's O1: the clamps are implemented
                                      // and must be visible here too)
};

class ParticleSystem {
public:
    struct Config {
        long long seed = 0;
        std::vector<ParticleEmitter> emitters;
    };

    explicit ParticleSystem(const Config& config);

    /// Advance one fixed sim step.
    void step();

    /// The step count so far.
    long long stepCount() const;

    /// One particle's world position (pool order: emitter order, then the
    /// emitter's pool index).
    struct Particle {
        int x = 0, y = 0;
        int remainingLife = 0;

        bool operator==(const Particle&) const = default;
    };
    std::vector<Particle> particles() const;

    /// The ADR-0014 structural hash of the particle state (the step count
    /// and every particle's position and remaining life).
    std::uint64_t stateHash() const;

private:
    struct PoolParticle {
        int x = 0, y = 0;
        int vx = 0, vy = 0;
        int remainingLife = 0;
        std::size_t emitter = 0;
    };

    /// The next seeded spawn jitter value for emitter `index` (the LCG).
    int nextJitter(std::size_t emitterIndex, int spread);

    Config config_{};
    long long stepCount_ = 0;
    std::uint64_t rng_ = 0;
    std::vector<PoolParticle> pool_;
};

}  // namespace ply
