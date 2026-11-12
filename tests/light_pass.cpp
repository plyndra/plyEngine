// LightMap + PostPass (M1b b8a+b8b as one family, E-b8.1/E-b8.2):
// "structural light hash from (clock, lights); noon vs cave fixtures differ"
// and "off/on hashes differ; off is the default in tests".
//
// Pinned: the two-keypoint day/night ambient schedule (literals at every
// quarter-day, wrap, and a clock past 2^40 — the F1 lesson); the additive
// pool falloff (intensity * (r²-d²)/r², integer, literals at the centre,
// half-radius and beyond); the [0,255] clamp; the structural light hash
// moving with the clock AND the lights, with the noon-vs-cave differential
// as the named clause; the vignette's default OFF, its factor field
// (centre untouched, corners exactly 255 - strength), the off/on hash
// differential, and the optional palette LUT participating in the hash.
//
// House rules: pure data. No GL, no window, no clock object, no float.

#include "plyEngine/LightMap.hpp"
#include "plyEngine/PostPass.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

// Live EVIDENCE counters (each incremented by the pins that ran).
int lightMatch = 0, lightRan = 0;
int hashMatch = 0, hashRan = 0;
int postMatch = 0, postRan = 0;

void pinAt(int& match, int& ran, bool cond, const char* what) {
    ++ran;
    if (cond) {
        ++match;
        return;
    }
    ++failures;
    std::fprintf(stderr, "  FAIL: %s\n", what);
}

// --- fixtures: noon and cave (the E-b8.1 differential) -----------------------

ply::LightMap::Config noonConfig() {
    ply::LightMap::Config config;
    config.mapWidth = 640;
    config.mapHeight = 360;
    config.noonTint = {255, 255, 255};
    config.midnightTint = {48, 48, 88};
    config.stepsPerDay = 240;
    return config;  // no pools: the sun is the schedule
}

ply::LightMap::Config caveConfig() {
    ply::LightMap::Config config;
    config.mapWidth = 640;
    config.mapHeight = 360;
    config.noonTint = {16, 16, 24};
    config.midnightTint = {4, 4, 8};
    config.stepsPerDay = 240;
    config.lights.push_back({320, 180, 120, 220});  // the lit radius
    return config;
}

// --- the ambient schedule and the pool field ---------------------------------

void testLightField() {
    const ply::LightMap noon(noonConfig());

    // The two-keypoint schedule, literal at every quarter day.
    pinAt(lightMatch, lightRan,
          noon.ambientAt(0) == ply::LightTint({48, 48, 88}), "midnight tint at step 0");
    pinAt(lightMatch, lightRan,
          noon.ambientAt(60) == ply::LightTint({151, 151, 171}),
          "quarter-day lerp literal (151,151,171)");
    pinAt(lightMatch, lightRan,
          noon.ambientAt(120) == ply::LightTint({255, 255, 255}), "noon tint at midday");
    pinAt(lightMatch, lightRan,
          noon.ambientAt(180) == ply::LightTint({152, 152, 172}),
          "three-quarter-day lerp literal (152,152,172 — trunc toward zero)");
    pinAt(lightMatch, lightRan, noon.ambientAt(240) == noon.ambientAt(0),
          "the day wraps (the tint is a function of step % stepsPerDay)");

    // The F1 lesson on the clock: a step past 2^40 lands on phase 16
    // exactly (75, 75, 110) — a narrowed clock fails.
    pinAt(lightMatch, lightRan,
          noon.ambientAt((1LL << 40)) == ply::LightTint({75, 75, 110}),
          "clock past 2^40 reduces exactly (phase 16)");

    // The additive pool: centre = intensity added, half-radius = the
    // (r²-d²)/r² literal, beyond the radius = the ambient alone.
    const ply::LightMap cave(caveConfig());
    pinAt(lightMatch, lightRan,
          cave.sampleAt(320, 180, 0) == ply::LightTint({224, 224, 228}),
          "pool centre: ambient + 220 ({224,224,228})");
    pinAt(lightMatch, lightRan,
          cave.sampleAt(380, 180, 0) == ply::LightTint({169, 169, 173}),
          "half radius: 220*(r²-d²)/r² = 165 ({169,169,173})");
    pinAt(lightMatch, lightRan,
          cave.sampleAt(0, 0, 0) == ply::LightTint({4, 4, 8}),
          "beyond the radius: the ambient alone");
    pinAt(lightMatch, lightRan,
          cave.sampleAt(320, 180, 0) != cave.sampleAt(0, 0, 0),
          "the pool visibly brightens its area");
}

void testClampAndClockField() {
    // The [0,255] clamp: 200 ambient + 200 pool = 400 -> 255.
    ply::LightMap::Config config;
    config.mapWidth = 640;
    config.mapHeight = 360;
    config.noonTint = {200, 200, 200};
    config.midnightTint = {200, 200, 200};
    config.stepsPerDay = 240;
    config.lights.push_back({0, 0, 100, 200});
    const ply::LightMap hot(config);
    pinAt(lightMatch, lightRan,
          hot.sampleAt(0, 0, 0) == ply::LightTint({255, 255, 255}),
          "over-range samples clamp to 255");
}

// --- the structural light hash (E-b8.1) --------------------------------------

void testLightHash() {
    const ply::LightMap noon(noonConfig());
    const ply::LightMap cave(caveConfig());

    // Reproducible on recomputation.
    pinAt(hashMatch, hashRan, noon.hashAt(0) == noon.hashAt(0),
          "the light hash is reproducible");

    // It is a function of (clock, lights): the clock moves it...
    pinAt(hashMatch, hashRan, noon.hashAt(0) != noon.hashAt(120),
          "the hash moves with the clock (midnight vs noon)");
    // ...and the lights move it.
    ply::LightMap::Config extra = caveConfig();
    extra.lights.push_back({10, 10, 60, 100});
    pinAt(hashMatch, hashRan,
          ply::LightMap(extra).hashAt(0) != cave.hashAt(0),
          "the hash moves with the lights");
    // The light VALUES are hashed, not merely their count: a pool nudged
    // from radius 120 to 121 produces identical samples at all five named
    // coordinates (both radii are far below every named point's distance
    // except the centre, which both fill identically) — only the explicit
    // light fields can separate the hashes. (Injection design found the
    // count-only mutation green; this pin isolates the values.)
    ply::LightMap::Config nudged = caveConfig();
    nudged.lights[0].radius = 121;
    pinAt(hashMatch, hashRan,
          ply::LightMap(nudged).hashAt(0) != cave.hashAt(0),
          "the hash covers the light VALUES (same count, same samples)");

    // THE E-b8.1 CLAUSE: the noon and cave fixtures differ — at the same
    // clock, and across the day.
    pinAt(hashMatch, hashRan, noon.hashAt(120) != cave.hashAt(120),
          "noon vs cave fixtures differ (the E-b8.1 clause)");
    pinAt(hashMatch, hashRan,
          noon.hashAt(0) != cave.hashAt(0) && noon.hashAt(60) != cave.hashAt(60),
          "noon vs cave differ across the day, not at one step");

    // The clock is hashed EXPLICITLY, not only through the schedule: with a
    // CONSTANT ambient (midnight == noon) the named samples are identical
    // at every step, so only the clock field itself can move the hash.
    // (Injection design found this pin missing — the step-field mutation
    // went green through the schedule — so the pin landed and now kills it.)
    ply::LightMap::Config flat = noonConfig();
    flat.midnightTint = flat.noonTint;
    const ply::LightMap steady(flat);
    pinAt(hashMatch, hashRan, steady.hashAt(0) != steady.hashAt(1),
          "the clock is hashed explicitly (constant-ambient fixture)");
}

// --- the PostPass (E-b8.2) ---------------------------------------------------

void testPostPass() {
    ply::PostPass::Config config;  // defaults: vignette OFF
    pinAt(postMatch, postRan, config.vignette == false,
          "the vignette default is OFF (the b8b clause)");
    ply::PostPass off(config);
    pinAt(postMatch, postRan, !off.vignetteEnabled(), "the default pass reports off");
    pinAt(postMatch, postRan,
          off.vignetteFactorAt(0, 0) == 255 && off.vignetteFactorAt(320, 180) == 255 &&
              off.vignetteFactorAt(639, 359) == 255,
          "off: the factor is 255 everywhere");
    pinAt(postMatch, postRan, config.paletteLut.empty(), "the palette LUT is empty by default");

    // On (strength 128): the centre untouched, the corners exactly
    // 255 - strength on the (0,0) side (maxd² = 320²+180²), the other
    // corner 128 (integer truncation), and the edge midpoints literal.
    ply::PostPass::Config on = config;
    on.vignette = true;
    on.vignetteStrength = 128;
    ply::PostPass vig(on);
    pinAt(postMatch, postRan, vig.vignetteEnabled(), "the enabled pass reports on");
    pinAt(postMatch, postRan, vig.vignetteFactorAt(320, 180) == 255,
          "the vignette's centre is untouched (255)");
    pinAt(postMatch, postRan, vig.vignetteFactorAt(0, 0) == 127,
          "corner (0,0) lands exactly on 255 - strength");
    pinAt(postMatch, postRan, vig.vignetteFactorAt(639, 359) == 128,
          "corner (639,359) is the integer-truncated twin (128)");
    pinAt(postMatch, postRan, vig.vignetteFactorAt(320, 0) == 225,
          "top edge midpoint literal (225)");
    pinAt(postMatch, postRan, vig.vignetteFactorAt(0, 180) == 158,
          "left edge midpoint literal (158)");

    // The E-b8.2 CLAUSE: off/on hashes differ.
    pinAt(postMatch, postRan, off.hash() != vig.hash(),
          "off/on hashes differ (the E-b8.2 clause)");

    // The LUT participates in the hash when present (and is optional).
    ply::PostPass::Config lut = on;
    lut.paletteLut.assign(256, 0);
    const std::uint64_t lutHash = ply::PostPass(lut).hash();
    pinAt(postMatch, postRan, lutHash != vig.hash(),
          "a palette LUT present moves the hash");
    lut.paletteLut[7] = 9;
    pinAt(postMatch, postRan, ply::PostPass(lut).hash() != lutHash,
          "a LUT change moves the hash");

    // Strength 0 with the vignette on darkens nothing (factor 255).
    ply::PostPass::Config zero = on;
    zero.vignetteStrength = 0;
    ply::PostPass flat(zero);
    pinAt(postMatch, postRan,
          flat.vignetteFactorAt(0, 0) == 255 && flat.vignetteFactorAt(639, 359) == 255,
          "strength 0 darkens nothing");
}

// --- EVIDENCE ----------------------------------------------------------------

void printEvidence() {
    const ply::LightMap noon(noonConfig());
    const ply::LightMap cave(caveConfig());
    ply::PostPass::Config offConfig;  // defaults
    ply::PostPass::Config onConfig = offConfig;
    onConfig.vignette = true;
    onConfig.vignetteStrength = 128;
    const ply::PostPass off(offConfig);
    const ply::PostPass on(onConfig);
    std::printf(
        "EVIDENCE light_pass steps=240 light=%d/%d hash=%d/%d post=%d/%d "
        "noon_cave=%d on_off=%d vignette_default=%s lut=%s\n",
        lightMatch, lightRan, hashMatch, hashRan, postMatch, postRan,
        noon.hashAt(120) != cave.hashAt(120) ? 1 : 0,
        off.hash() != on.hash() ? 1 : 0,
        offConfig.vignette ? "on" : "off",
        offConfig.paletteLut.empty() ? "empty" : "present");
}

}  // namespace

int main() {
    testLightField();
    testClampAndClockField();
    testLightHash();
    testPostPass();

    if (failures == 0) {
        printEvidence();
        std::printf("light_pass tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "light_pass tests: %d failure(s)\n", failures);
    return 1;
}
