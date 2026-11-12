// ParticleSystem (M1b b9, E-b9.1): "same seed -> identical particle
// positions; gameplay state hash unchanged with particles on" — the
// seeded, fixed-step, visual-only clause.
//
// Pinned: same-seed identity (positions at lockstep checkpoints and the
// state hash), seed sensitivity, the deterministic respawn cycle (gravity
// literals including the respawn at end of life), the pool invariant, the
// jitter bound, the fixed-step purity (1000-step runs identical), and THE
// VISUAL-ONLY CLAUSE: a real gameplay artifact (a WorldMap submission
// hash) is byte-identical before and after driving the system.
//
// House rules: pure data. No GL, no window, no clock object, no float.

#include "plyEngine/ParticleSystem.hpp"

#include "plyEngine/MapLoader.hpp"
#include "plyEngine/WorldMap.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

int determinismMatch = 0, determinismRan = 0;
int motionMatch = 0, motionRan = 0;
int visualMatch = 0, visualRan = 0;

/// Safe indexed read: an OOB index reads as an impossible particle so a
/// hollow or broken system fails the pin instead of aborting — the a5.1
/// lesson (this suite's first red run segfaulted on particles()[0]; the
/// harness was guarded BEFORE the implementation landed — sixth strike).
ply::ParticleSystem::Particle particleAt(const std::vector<ply::ParticleSystem::Particle>& list,
                                         std::size_t index) {
    if (index < list.size()) return list[index];
    ply::ParticleSystem::Particle sentinel;
    sentinel.x = -999999;
    sentinel.y = -999999;
    sentinel.remainingLife = -1;
    return sentinel;
}

void pinAt(int& match, int& ran, bool cond, const char* what) {
    ++ran;
    if (cond) {
        ++match;
        return;
    }
    ++failures;
    std::fprintf(stderr, "  FAIL: %s\n", what);
}

ply::ParticleSystem::Config fixtureConfig(long long seed) {
    ply::ParticleSystem::Config config;
    config.seed = seed;
    // A: one particle, no jitter, pure gravity (the literal motion pins).
    config.emitters.push_back({100, 200, 0, 0, 0, 0, -2, 5, 1});
    // B: eight particles, seeded jitter, drift + gravity, longer life.
    config.emitters.push_back({320, 180, 10, 6, 3, -4, -1, 20, 8});
    return config;
}

void testSameSeed() {
    ply::ParticleSystem a(fixtureConfig(42));
    ply::ParticleSystem b(fixtureConfig(42));
    pinAt(determinismMatch, determinismRan,
          a.particles() == b.particles() && a.stateHash() == b.stateHash(),
          "same seed: identical spawn state");
    bool identical = true;
    for (int step = 1; step <= 100; ++step) {
        a.step();
        b.step();
        if (step == 1 || step == 25 || step == 100) {
            identical = identical && a.particles() == b.particles() &&
                        a.stateHash() == b.stateHash();
        }
    }
    pinAt(determinismMatch, determinismRan, identical,
          "same seed: identical positions at steps 1, 25, 100 (the E-b9.1 clause)");
    pinAt(determinismMatch, determinismRan, a.stepCount() == 100,
          "the fixed-step counter reads 100 after 100 steps");

    // Recomputation: a fresh pair behaves the same (pure function of
    // seed + config + stepCount).
    ply::ParticleSystem c(fixtureConfig(42));
    for (int step = 0; step < 100; ++step) c.step();
    pinAt(determinismMatch, determinismRan, c.stateHash() == a.stateHash(),
          "recomputation reproduces the state hash");

    // A different seed diverges (the seed matters).
    ply::ParticleSystem d(fixtureConfig(43));
    for (int step = 0; step < 100; ++step) d.step();
    pinAt(determinismMatch, determinismRan,
          d.particles() != a.particles() && d.stateHash() != a.stateHash(),
          "a different seed diverges");

    // Long fixed-step runs stay deterministic (the F1 lesson on counters).
    ply::ParticleSystem e(fixtureConfig(7));
    ply::ParticleSystem f(fixtureConfig(7));
    for (int step = 0; step < 1000; ++step) {
        e.step();
        f.step();
    }
    pinAt(determinismMatch, determinismRan,
          e.stateHash() == f.stateHash() && e.stepCount() == 1000,
          "1000-step runs identical (wide step counters)");
}

void testMotionAndPool() {
    ply::ParticleSystem s(fixtureConfig(42));

    // The pool invariant: 1 + 8 particles, always.
    pinAt(motionMatch, motionRan, s.particles().size() == 9, "the pool holds 9 particles");

    // Emitter A: position += velocity, then velocity += gravity (documented
    // order); y: 200, 198, 194, 188 over four steps, then life 5 ends and
    // the particle RESPAWNS at the emitter (y 200) — the deterministic
    // respawn cycle.
    const int expectedY[5] = {200, 198, 194, 188, 200};
    bool motion = true;
    for (int step = 1; step <= 5; ++step) {
        s.step();
        motion = motion && particleAt(s.particles(), 0).y == expectedY[step - 1];
    }
    pinAt(motionMatch, motionRan, motion, "gravity literals + the respawn at life 5");

    // The respawn resets the clock: the cycle repeats identically.
    ply::ParticleSystem again(fixtureConfig(42));
    const int cycle2[5] = {200, 198, 194, 188, 200};
    bool second = true;
    for (int step = 1; step <= 10; ++step) {
        again.step();
        if (step > 5) second = second && particleAt(again.particles(), 0).y == cycle2[step - 6];
    }
    pinAt(motionMatch, motionRan, second, "the respawn cycle repeats identically");

    // The state hash covers REMAINING LIFE, not just positions: at step 1
    // two systems with lifetimes 5 and 6 share every position but differ in
    // life — only the life field separates their hashes. (Injection design
    // found the field unpinned; the pin landed first.)
    ply::ParticleSystem::Config longLife = fixtureConfig(42);
    longLife.emitters[0].lifetime = 6;
    ply::ParticleSystem l5(fixtureConfig(42));
    ply::ParticleSystem l6(longLife);
    l5.step();
    l6.step();
    const std::vector<ply::ParticleSystem::Particle> p5 = l5.particles();
    const std::vector<ply::ParticleSystem::Particle> p6 = l6.particles();
    bool samePositions = p5.size() == p6.size();
    for (std::size_t i = 0; samePositions && i < p5.size(); ++i) {
        samePositions = p5[i].x == p6[i].x && p5[i].y == p6[i].y;
    }
    pinAt(determinismMatch, determinismRan,
          samePositions && l5.stateHash() != l6.stateHash(),
          "the state hash covers remaining life (same positions, different life)");

    // The seeded jitter stays within the emitter's spread at spawn.
    ply::ParticleSystem j(fixtureConfig(42));
    bool bounded = true;
    const std::vector<ply::ParticleSystem::Particle> spawn = j.particles();

    // The seeded jitter actually VARIES (a stuck RNG giving every particle
    // one offset is deterministic and seeded too — injection design found
    // this pin missing, so it landed first): the eight spawns of emitter B
    // cover at least three distinct x values.
    std::vector<int> spawnX;
    for (const ply::ParticleSystem::Particle& particle : spawn) {
        if (particle.x != -999999) spawnX.push_back(particle.x);
    }
    std::sort(spawnX.begin(), spawnX.end());
    spawnX.erase(std::unique(spawnX.begin(), spawnX.end()), spawnX.end());
    pinAt(motionMatch, motionRan, spawnX.size() >= 3,
          "the seeded jitter varies (>= 3 distinct spawn x values)");
    for (std::size_t i = 1; i < spawn.size(); ++i) {
        bounded = bounded && (spawn[i].x >= 320 - 10 && spawn[i].x <= 320 + 10 &&
                              spawn[i].y >= 180 - 6 && spawn[i].y <= 180 + 6);
    }
    pinAt(motionMatch, motionRan, bounded, "spawn jitter stays within +-spread");

    // The pool size never changes while stepping.
    for (int step = 0; step < 50; ++step) s.step();
    pinAt(motionMatch, motionRan, s.particles().size() == 9,
          "the pool size is constant while stepping");
}

void testVisualOnly() {
    // THE E-b9.1 SECOND CLAUSE: a real gameplay artifact's hash is
    // unchanged with particles on. The artifact: a WorldMap's submission
    // (the b3b structural hash), hashed before and after driving the sim.
    ply::TilesetSlice tileset;
    tileset.name = "t";
    tileset.image = "t.png";
    tileset.tileWidth = 16;
    tileset.tileHeight = 16;
    ply::TileDef def;
    def.id = 3;
    def.region = {0, 0, 16, 16};
    tileset.tiles.push_back(def);

    ply::MapData map;
    map.name = "gameplay";
    map.tileset = "t";
    map.width = 8;
    map.height = 8;
    ply::MapLayer ground;
    ground.name = "ground";
    for (int i = 0; i < 64; ++i) ground.tiles.push_back(i % 7 == 0 ? -1 : 3);
    map.layers.push_back(ground);
    ply::WorldMap world = ply::WorldMap::build(map, tileset);
    const std::uint64_t before = world.submit({0, 0, 128, 128}).hash();

    ply::ParticleSystem particles(fixtureConfig(42));
    for (int step = 0; step < 200; ++step) particles.step();

    const std::uint64_t after = world.submit({0, 0, 128, 128}).hash();
    pinAt(visualMatch, visualRan, before == after,
          "gameplay state hash unchanged with particles on (the E-b9.1 clause)");
    pinAt(visualMatch, visualRan, before != 0 && after != 0,
          "the gameplay hash is a real measurement (non-zero)");
}

void printEvidence() {
    ply::ParticleSystem s(fixtureConfig(42));
    for (int step = 0; step < 100; ++step) s.step();
    std::printf(
        "EVIDENCE particles steps=100 seed=42 same=%d diff=%d gameplay=unchanged pool=%zu "
        "motion=%d/%d determinism=%d/%d hash=%016llx\n",
        1, 1, s.particles().size(), motionMatch, motionRan, determinismMatch, determinismRan,
        static_cast<unsigned long long>(s.stateHash()));
}

}  // namespace

int main() {
    testSameSeed();
    testMotionAndPool();
    testVisualOnly();

    if (failures == 0) {
        printEvidence();
        std::printf("particles tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "particles tests: %d failure(s)\n", failures);
    return 1;
}
