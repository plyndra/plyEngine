// M7 reference demo: game logic for the single-screen platformer.
//
// This header is deliberately GL-free: tests/platformer.cpp runs it headless
// and --verify drives it through a scripted Autopilot, so both can assert
// real gameplay outcomes (coins collected, deaths, who reached the goal)
// instead of "pixels were drawn". Rendering lives in main.cpp.

#pragma once

#include "plyEngine/PhysicsBody.hpp"
#include "plyEngine/Tilemap.hpp"
#include "plyEngine/Transform.hpp"
#include "plyEngine/World.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace demo {

// --- Level dimensions (40x18 cells of 32px = 1280x576 world units) ----------
inline constexpr int kMapW = 40;
inline constexpr int kMapH = 18;
inline constexpr float kTile = 32.0f;
// The demo's internal resolution (a7): 1 world unit = 1 internal px, so the
// render target is the world plane itself and the present blit is the integer
// factor the window allows (1 at 1280x720, 2 at 2560x1440).
inline constexpr int kInternalW = kMapW * 32;
inline constexpr int kInternalH = kMapH * 32;
inline constexpr int kPlayerCount = 4;

// --- Gameplay tuning --------------------------------------------------------
inline constexpr float kMoveSpeed = 320.0f;   // world units / second
inline constexpr float kJumpSpeed = 620.0f;   // gives a ~120 unit apex
inline constexpr float kPitDeathY = 60.0f;    // centre height below = fell in
inline constexpr float kInvulnSeconds = 1.0f; // respawn grace period

// Level landmarks shared by gameplay, rendering and the Autopilot.
inline constexpr float kPitLeft = 17.0f * kTile;   // 544
inline constexpr float kPitRight = 21.0f * kTile;  // 672
// The goal triggers anywhere on top of the final staircase step.
inline constexpr float kGoalLeft = 36.0f * kTile;  // 1152
inline constexpr float kGoalRight = 40.0f * kTile; // 1280
inline constexpr float kGoalBottom = 11.0f * kTile; // 352
inline constexpr float kGoalTop = 13.0f * kTile;   // 416

/// One-shot sound cues the game emits; the host (main.cpp) plays them. The
/// game logic stays audio-free so it can be tested headlessly.
enum class Sfx { Jump, Coin, Death, Win };

/// Per-player command for one fixed step. The demo fills this from phone
/// slots, the keyboard, or the verify Autopilot — the game never cares.
struct PlayerInput {
    float x = 0.0f;           // -1..1 desired horizontal move
    bool jumpPressed = false; // edge: jump started this step
};

// Gameplay components (rendering tags sprites onto the same entities).
struct Coin {};
struct Goal {};
struct PlayerTag {
    int index = 0;
};

/// A patrolling hazard. Moved by the game (not physics) along [x0, x1].
struct Hazard {
    float x0 = 0.0f;
    float x1 = 0.0f;
    float speed = 60.0f;
    float dir = 1.0f;
    float y = 0.0f;         // fixed centre height
    float travelled = 0.0f; // distance moved, asserted by --verify
};

struct Player {
    ply::Entity entity{};
    glm::vec2 spawn{0.0f};
    int score = 0;
    int deaths = 0;
    float invuln = 0.0f;
};

enum class GameState { Playing, Paused, Won };

/// The whole reference game: level, players, coins, hazards and win state.
/// Owns no world — every method takes the World it should act on, so the
/// same object works in the demo, the headless test and --verify.
class Game {
public:
    Game() = default;

    /// Builds the level and spawns every gameplay entity. Idempotent only
    /// after reset(): it does not clear previous state.
    void build(ply::World& world);

    /// Tears down all gameplay entities and rebuilds (R / START restart).
    void reset(ply::World& world);

    /// One fixed step: input -> hazards -> physics -> pickups/deaths/goal.
    void step(ply::World& world, const std::array<PlayerInput, kPlayerCount>& input,
              float dt);

    void togglePause() {
        if (state == GameState::Playing) {
            state = GameState::Paused;
        } else if (state == GameState::Paused) {
            state = GameState::Playing;
        }
    }

    // --- State (read by rendering, tests and --verify) ----------------------
    ply::Tilemap map{kMapW, kMapH, kTile};
    std::array<Player, kPlayerCount> players{};
    std::vector<ply::Entity> coins;    // live coin entities
    std::vector<ply::Entity> hazards;  // both saws
    ply::Entity goal{};
    GameState state = GameState::Playing;
    int winner = -1;         // player index, valid once state == Won
    int coinsCollected = 0;
    int initialCoinCount = 0;
    float time = 0.0f;       // seconds of gameplay simulated (pauses freeze it)

    /// Sound cues emitted since the last drain. The host plays and clears
    /// them; tests assert on them ("a coin sound was requested" is gameplay
    /// evidence, unlike "the audio device opened").
    std::vector<Sfx> sfx;

private:
    void buildLevelGeometry();
    void spawnPlayers(ply::World& world);
    void spawnCoins(ply::World& world);
    void spawnHazards(ply::World& world);
    void spawnGoal(ply::World& world);
    void killPlayer(ply::World& world, int index);

    ply::PhysicsSettings settings_;
};

namespace level {

inline glm::vec2 spawnPoint(int index) {
    // Spread the four players along the ground under the coin ledge so all of
    // them are visible at once and none blocks another's route.
    constexpr float kSpawnX[kPlayerCount] = {8.0f, 6.0f, 10.0f, 4.0f};
    const int i = (index >= 0 && index < kPlayerCount) ? index : 0;
    return glm::vec2{kSpawnX[i] * kTile + kTile * 0.5f, 110.0f};
}

}  // namespace level

// --- Autopilot ---------------------------------------------------------------

/// A deterministic scripted player used by --verify and tests/platformer.cpp.
/// It reads game state and emits PlayerInput, so it exercises the exact same
/// code path a phone player does (modulo the CompanionInput queue, which the
/// demo wires up in verify mode).
class Autopilot {
public:
    enum class Phase { P2Probe, ToCoin, FallInPit, SawDeath, ToGoal, Done };

    static constexpr int kP2ProbeSteps = 40;
    /// If a phase runs this long without advancing, the script is stuck and
    /// the run must fail verification.
    static constexpr int kMaxPhaseSteps = 4000;

    std::array<PlayerInput, kPlayerCount> update(const Game& game,
                                                 const ply::World& world);

    Phase phase = Phase::P2Probe;
    bool stuck = false;
    int steps = 0;
    int phaseSteps = 0;

private:
    void advance(Phase next) {
        phase = next;
        phaseSteps = 0;
    }
};

// --- Implementation ----------------------------------------------------------

inline void Game::buildLevelGeometry() {
    // Clear every cell first so build() and reset() converge to one layout.
    for (int y = 0; y < kMapH; ++y) {
        for (int x = 0; x < kMapW; ++x) {
            map.setSolid(x, y, false);
        }
    }

    // Floor and side walls. Out-of-bounds cells are solid by default, but
    // x=0 / x=39 are inside the grid and must be filled explicitly.
    for (int x = 0; x < kMapW; ++x) {
        map.setSolid(x, 0, true);
    }
    for (int y = 0; y < kMapH; ++y) {
        map.setSolid(0, y, true);
        map.setSolid(kMapW - 1, y, true);
    }

    // Ground row with the pit at x=17..20.
    for (int x = 1; x <= 25; ++x) {
        if (x >= 17 && x <= 20) {
            continue;
        }
        map.setSolid(x, 2, true);
    }

    // Left ledge (saw B patrols it; reachable by a skilled jump chain).
    for (int x = 1; x <= 4; ++x) {
        map.setSolid(x, 5, true);
    }

    // Right staircase: three solid risers, each 2-3 tiles above the last, so
    // "grounded and blocked ahead -> jump" climbs it deterministically.
    for (int x = 26; x < kMapW; ++x) {
        for (int y = 2; y <= 4; ++y) {   // step 1, tread top = 160
            map.setSolid(x, y, true);
        }
    }
    for (int x = 31; x < kMapW; ++x) {
        for (int y = 5; y <= 7; ++y) {   // step 2, tread top = 256
            map.setSolid(x, y, true);
        }
    }
    for (int x = 36; x < kMapW; ++x) {
        for (int y = 8; y <= 10; ++y) {  // step 3, tread top = 352
            map.setSolid(x, y, true);
        }
    }
}

inline void Game::spawnPlayers(ply::World& world) {
    for (int i = 0; i < kPlayerCount; ++i) {
        Player& player = players[i];
        player = Player{};
        player.entity = world.spawn();
        player.spawn = level::spawnPoint(i);

        ply::Transform transform;
        transform.position = player.spawn;
        world.add<ply::Transform>(player.entity, transform);
        world.add<ply::GlobalTransform>(player.entity, ply::GlobalTransform{});
        world.add<ply::Velocity>(player.entity, ply::Velocity{});
        world.add<ply::Body>(player.entity, ply::Body{glm::vec2{14.0f, 14.0f}});
        world.add<PlayerTag>(player.entity, PlayerTag{i});
    }
}

inline void Game::spawnCoins(ply::World& world) {
    // (cell x, cell y): one on the ground walk, one before the pit, one on
    // each staircase tread. Ground coins sit at y=3 so a walking player
    // (centre 110) overlaps them; tread coins sit 14 above the tread.
    const std::array<std::array<float, 2>, 5> kCoins = {{
        {{11.0f, 3.0f * kTile + 16.0f}},   // 368, 112 — first coin
        {{15.0f, 3.0f * kTile + 16.0f}},   // 496, 112 — before the pit
        {{27.0f, 160.0f + 14.0f}},         // stair 1 tread
        {{32.0f, 256.0f + 14.0f}},         // stair 2 tread
        {{37.0f, 352.0f + 14.0f}},         // stair 3 tread, by the goal
    }};

    coins.clear();
    initialCoinCount = 0;
    for (const auto& c : kCoins) {
        const ply::Entity coin = world.spawn();
        ply::Transform transform;
        transform.position = glm::vec2{c[0] * kTile + kTile * 0.5f, c[1]};
        world.add<ply::Transform>(coin, transform);
        world.add<ply::GlobalTransform>(coin, ply::GlobalTransform{});
        world.add<Coin>(coin, Coin{});
        coins.push_back(coin);
        ++initialCoinCount;
    }
}

inline void Game::spawnHazards(ply::World& world) {
    hazards.clear();
    const Hazard kHazards[] = {
        // Saw A: ground strip right of the pit, directly on the goal route.
        Hazard{700.0f, 820.0f, 70.0f, 1.0f, 110.0f, 0.0f},
        // Saw B: the left ledge, off the critical path but very much alive.
        Hazard{60.0f, 140.0f, 55.0f, 1.0f, 192.0f + 14.0f, 0.0f},
    };

    for (const Hazard& spec : kHazards) {
        const ply::Entity saw = world.spawn();
        ply::Transform transform;
        transform.position = glm::vec2{(spec.x0 + spec.x1) * 0.5f, spec.y};
        world.add<ply::Transform>(saw, transform);
        world.add<ply::GlobalTransform>(saw, ply::GlobalTransform{});
        world.add<Hazard>(saw, spec);
        hazards.push_back(saw);
    }
}

inline void Game::spawnGoal(ply::World& world) {
    goal = world.spawn();
    ply::Transform transform;
    // Purely visual: the trigger is the rect constants, the sprite is a flag
    // standing on the top step (centre 22 above the tread for a 44-tall quad),
    // parked at the right edge so it doesn't cover the tread coin.
    transform.position = glm::vec2{kGoalRight - 24.0f, kGoalBottom + 22.0f};
    world.add<ply::Transform>(goal, transform);
    world.add<ply::GlobalTransform>(goal, ply::GlobalTransform{});
    world.add<Goal>(goal, Goal{});
}

inline void Game::build(ply::World& world) {
    buildLevelGeometry();
    spawnPlayers(world);
    spawnCoins(world);
    spawnHazards(world);
    spawnGoal(world);
    state = GameState::Playing;
    winner = -1;
    coinsCollected = 0;
    time = 0.0f;
}

inline void Game::reset(ply::World& world) {
    auto destroyAll = [&](ply::Entity entity) {
        if (world.alive(entity)) {
            world.destroy(entity);
        }
    };
    for (Player& player : players) {
        destroyAll(player.entity);
    }
    for (ply::Entity coin : coins) {
        destroyAll(coin);
    }
    for (ply::Entity saw : hazards) {
        destroyAll(saw);
    }
    destroyAll(goal);
    build(world);
}

inline void Game::killPlayer(ply::World& world, int index) {
    Player& player = players[index];
    ++player.deaths;
    player.invuln = kInvulnSeconds;
    sfx.push_back(Sfx::Death);

    ply::Transform transform;
    transform.position = player.spawn;
    ply::setTransform(world, player.entity, transform);
    if (ply::Velocity* velocity = world.get<ply::Velocity>(player.entity)) {
        velocity->value = glm::vec2{0.0f, 0.0f};
    }
}

namespace detail {

inline bool centreOverlap(const glm::vec2& a, float halfA, const glm::vec2& b,
                          float halfB) {
    return std::fabs(a.x - b.x) < halfA + halfB &&
           std::fabs(a.y - b.y) < halfA + halfB;
}

}  // namespace detail

inline void Game::step(ply::World& world,
                       const std::array<PlayerInput, kPlayerCount>& input, float dt) {
    if (state != GameState::Playing) {
        // Frozen, but transforms stay fresh for rendering.
        ply::updateTransforms(world);
        return;
    }
    time += dt;

    // --- Hazards patrol on a deterministic triangle wave --------------------
    for (ply::Entity saw : hazards) {
        Hazard* hazard = world.get<Hazard>(saw);
        ply::Transform* transform = world.get<ply::Transform>(saw);
        if (!hazard || !transform) {
            continue;
        }
        float next = transform->position.x + hazard->dir * hazard->speed * dt;
        if (next > hazard->x1) {
            next = hazard->x1;
            hazard->dir = -1.0f;
        } else if (next < hazard->x0) {
            next = hazard->x0;
            hazard->dir = 1.0f;
        }
        hazard->travelled += std::fabs(next - transform->position.x);

        ply::Transform updated = *transform;
        updated.position.x = next;
        updated.rotation += hazard->dir * hazard->speed * dt / kTile;
        ply::setTransform(world, saw, updated);
    }

    // --- Players: input -> velocity, then one physics step for everyone -----
    for (int i = 0; i < kPlayerCount; ++i) {
        Player& player = players[i];
        if (player.invuln > 0.0f) {
            player.invuln -= dt;
        }
        if (ply::Velocity* velocity = world.get<ply::Velocity>(player.entity)) {
            velocity->value.x = input[i].x * kMoveSpeed;
        }
        if (input[i].jumpPressed) {
            if (const ply::Body* body = world.get<ply::Body>(player.entity)) {
                if (body->grounded) {
                    if (ply::Velocity* velocity = world.get<ply::Velocity>(player.entity)) {
                        velocity->value.y = kJumpSpeed;
                        sfx.push_back(Sfx::Jump);
                    }
                }
            }
        }
    }
    ply::stepPhysics(world, map, dt, settings_);

    // --- Pickups, deaths and the goal ---------------------------------------
    for (int i = 0; i < kPlayerCount; ++i) {
        Player& player = players[i];
        const ply::Transform* transform = world.get<ply::Transform>(player.entity);
        if (!transform) {
            continue;
        }
        const glm::vec2 position = transform->position;

        // Fell into the pit: any contact with the floor band is fatal.
        if (position.y < kPitDeathY) {
            killPlayer(world, i);
            continue;
        }

        // Hazards (respawn grace applies).
        if (player.invuln <= 0.0f) {
            bool touched = false;
            for (ply::Entity saw : hazards) {
                const ply::Transform* sawTransform = world.get<ply::Transform>(saw);
                if (sawTransform &&
                    detail::centreOverlap(position, 14.0f, sawTransform->position, 13.0f)) {
                    touched = true;
                    break;
                }
            }
            if (touched) {
                killPlayer(world, i);
                continue;
            }
        }

        // Coins.
        for (auto it = coins.begin(); it != coins.end();) {
            const ply::Transform* coinTransform = world.get<ply::Transform>(*it);
            if (!coinTransform || !world.alive(*it)) {
                it = coins.erase(it);
                continue;
            }
            if (detail::centreOverlap(position, 14.0f, coinTransform->position, 12.0f)) {
                world.destroy(*it);
                it = coins.erase(it);
                ++player.score;
                ++coinsCollected;
                sfx.push_back(Sfx::Coin);
            } else {
                ++it;
            }
        }

        // Goal: standing anywhere on the top step wins.
        if (state == GameState::Playing && position.x + 14.0f > kGoalLeft &&
            position.x - 14.0f < kGoalRight && position.y + 14.0f > kGoalBottom &&
            position.y - 14.0f < kGoalTop) {
            state = GameState::Won;
            winner = i;
            sfx.push_back(Sfx::Win);
        }
    }

    ply::updateTransforms(world);
}

// --- Autopilot implementation ------------------------------------------------

inline std::array<PlayerInput, kPlayerCount> Autopilot::update(const Game& game,
                                                               const ply::World& world) {
    ++steps;
    ++phaseSteps;
    if (phaseSteps > kMaxPhaseSteps) {
        stuck = true;
        phase = Phase::Done;
    }

    std::array<PlayerInput, kPlayerCount> in{};

    // Transitions read the state produced by the previous step.
    switch (phase) {
        case Phase::P2Probe:
            if (phaseSteps >= kP2ProbeSteps) {
                advance(Phase::ToCoin);
            }
            break;
        case Phase::ToCoin:
            if (game.players[0].score >= 1) {
                advance(Phase::FallInPit);
            }
            break;
        case Phase::FallInPit:
            if (game.players[0].deaths >= 1) {
                advance(Phase::SawDeath);
            }
            break;
        case Phase::SawDeath:
            if (game.players[0].deaths >= 2) {
                advance(Phase::ToGoal);
            }
            break;
        case Phase::ToGoal:
            if (game.state == GameState::Won) {
                advance(Phase::Done);
            }
            break;
        case Phase::Done:
            break;
    }

    const ply::Transform* p1 = world.get<ply::Transform>(game.players[0].entity);
    const ply::Body* body1 = world.get<ply::Body>(game.players[0].entity);
    const bool grounded = body1 && body1->grounded;
    const float x = p1 ? p1->position.x : 0.0f;
    const float y = p1 ? p1->position.y : 0.0f;

    // Jump helpers (player 1 only; P2 just gets probed).
    const bool pitWindow =
        grounded && x >= kPitLeft - 110.0f && x <= kPitLeft + 8.0f;

    bool sawNear = false;
    if (!game.hazards.empty()) {
        if (const ply::Transform* saw = world.get<ply::Transform>(game.hazards[0])) {
            const float ahead = saw->position.x - x;
            sawNear = grounded && ahead > 0.0f && ahead < 110.0f &&
                      std::fabs(saw->position.y - y) < 40.0f;
        }
    }

    switch (phase) {
        case Phase::P2Probe:
            // Prove every non-scripted slot drives its own player: P2 walks
            // left, P3 left a shorter way, P4 right briefly. All stay clear of
            // the pit and the saws so the probe cannot be confused with a
            // gameplay failure.
            in[1].x = -1.0f;
            in[2].x = -1.0f;
            in[3].x = 1.0f;
            break;
        case Phase::ToCoin:
            in[0].x = 1.0f;
            // Keep the probes alive (and therefore visibly independent) while
            // the scripted player runs the level: hold them still at their
            // displaced positions.
            break;
        case Phase::FallInPit:
            // Deliberately no jump: walk off into the pit (death 1).
            in[0].x = 1.0f;
            break;
        case Phase::SawDeath:
            // Cross the pit, then walk into the saw (death 2). No stair jumps:
            // being stuck against the riser is fine, the saw comes to us.
            in[0].x = 1.0f;
            in[0].jumpPressed = pitWindow;
            break;
        case Phase::ToGoal:
            // Full policy: clear the pit, jump over the saw, climb the stairs.
            in[0].x = 1.0f;
            in[0].jumpPressed =
                pitWindow || sawNear || (grounded && body1 && body1->hitX);
            break;
        case Phase::Done:
            break;
    }

    return in;
}

}  // namespace demo
