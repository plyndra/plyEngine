// ParticleSystem (M1b b9): seeded, fixed-step, visual-only. The contract —
// the LCG, the step order, the deterministic respawn, the visual-only rule
// — is documented in ParticleSystem.hpp; this file is that contract,
// implemented in exact integer arithmetic.
#include "plyEngine/ParticleSystem.hpp"

namespace ply {

namespace {
// The documented LCG (PCG's 64-bit multiplier/incumbent), advanced one draw
// per jitter request — deterministic on every host.
constexpr std::uint64_t kMultiplier = 6364136223846793005ULL;
constexpr std::uint64_t kIncrement = 1442695040888963407ULL;
}  // namespace

ParticleSystem::ParticleSystem(const Config& config) : config_(config) {
    rng_ = static_cast<std::uint64_t>(config_.seed);
    // Initial spawn: every particle at its emitter with a fresh seeded
    // jitter, initial velocity and full life.
    for (std::size_t e = 0; e < config_.emitters.size(); ++e) {
        const ParticleEmitter& emitter = config_.emitters[e];
        const int count = emitter.maxParticles > 0 ? emitter.maxParticles : 0;
        for (int i = 0; i < count; ++i) {
            PoolParticle particle;
            particle.emitter = e;
            particle.x = emitter.x + nextJitter(e, emitter.spreadX);
            particle.y = emitter.y + nextJitter(e, emitter.spreadY);
            particle.vx = emitter.velocityX;
            particle.vy = emitter.velocityY;
            particle.remainingLife = emitter.lifetime > 0 ? emitter.lifetime : 1;
            pool_.push_back(particle);
        }
    }
}

int ParticleSystem::nextJitter(std::size_t, int spread) {
    if (spread <= 0) return 0;
    rng_ = rng_ * kMultiplier + kIncrement;
    // The draw is a value in [-spread, spread] (2*spread+1 outcomes).
    const std::uint64_t outcomes = 2ULL * static_cast<std::uint64_t>(spread) + 1ULL;
    return static_cast<int>(rng_ % outcomes) - spread;
}

void ParticleSystem::step() {
    for (PoolParticle& particle : pool_) {
        const ParticleEmitter& emitter = config_.emitters[particle.emitter];
        // The documented order: position += velocity; velocity += gravity.
        particle.x += particle.vx;
        particle.y += particle.vy;
        // In flight the x velocity is constant; gravity bends y only.
        particle.vy += emitter.gravity;
        --particle.remainingLife;
        if (particle.remainingLife <= 0) {
            // Deterministic respawn: the emitter's point, a fresh seeded
            // jitter, the initial velocity, full life.
            particle.x = emitter.x + nextJitter(particle.emitter, emitter.spreadX);
            particle.y = emitter.y + nextJitter(particle.emitter, emitter.spreadY);
            particle.vx = emitter.velocityX;
            particle.vy = emitter.velocityY;
            particle.remainingLife = emitter.lifetime > 0 ? emitter.lifetime : 1;
        }
    }
    ++stepCount_;
}

long long ParticleSystem::stepCount() const { return stepCount_; }

std::vector<ParticleSystem::Particle> ParticleSystem::particles() const {
    std::vector<Particle> out;
    out.reserve(pool_.size());
    for (const PoolParticle& particle : pool_) {
        out.push_back({particle.x, particle.y, particle.remainingLife});
    }
    return out;
}

std::uint64_t ParticleSystem::stateHash() const {
    StructuralHasher hasher;
    hasher.u64(static_cast<std::uint64_t>(stepCount_));
    hasher.u64(pool_.size());
    for (const PoolParticle& particle : pool_) {
        hasher.i32(particle.x);
        hasher.i32(particle.y);
        hasher.i32(particle.remainingLife);
    }
    return hasher.value();
}

}  // namespace ply
