// Headless test of the M7 reference demo's game logic (no GL, no window).
//
// The platformer's Game + Autopilot live in examples/companion/platformer.hpp
// precisely so this test can drive a full scripted playthrough and assert
// real gameplay outcomes: coins collected, deaths to pit and saw, independent
// second player, hazards patrolling, and a win for player 1.

#include "platformer.hpp"

#include <array>
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

constexpr float kStep = 1.0f / 60.0f;

struct RunResult {
    demo::Game game;
    demo::Autopilot autopilot;
    /// Horizontal distance each player ended up from its spawn.
    std::array<float, demo::kPlayerCount> displacement{};
    float sawTravel = 0.0f;
    int steps = 0;
    /// Sound cues emitted by the whole run, indexed by demo::Sfx.
    std::array<int, 4> cues{};
};

// Runs the scripted playthrough to completion (or until the script sticks).
RunResult runScripted() {
    RunResult result;
    ply::World world;
    result.game.build(world);

    for (int i = 0; i < 6000; ++i) {
        if (result.autopilot.phase == demo::Autopilot::Phase::Done) {
            break;
        }
        const auto input = result.autopilot.update(result.game, world);
        result.game.step(world, input, kStep);
        for (demo::Sfx cue : result.game.sfx) {
            result.cues[static_cast<size_t>(cue)] += 1;
        }
        result.game.sfx.clear();
        ++result.steps;
    }

    for (int i = 0; i < demo::kPlayerCount; ++i) {
        if (const ply::Transform* transform =
                world.get<ply::Transform>(result.game.players[i].entity)) {
            result.displacement[static_cast<size_t>(i)] =
                std::fabs(transform->position.x - result.game.players[i].spawn.x);
        }
    }
    for (ply::Entity saw : result.game.hazards) {
        if (const demo::Hazard* hazard = world.get<demo::Hazard>(saw)) {
            result.sawTravel += hazard->travelled;
        }
    }
    return result;
}

}  // namespace

int main() {
    // --- scripted playthrough asserts real gameplay outcomes ---------------
    const RunResult run = runScripted();
    const demo::Game& game = run.game;

    check(run.autopilot.phase == demo::Autopilot::Phase::Done && !run.autopilot.stuck,
          "autopilot completed without sticking");
    check(game.state == demo::GameState::Won, "game reached Won");
    check(game.winner == 0, "player 1 won");
    check(game.players[0].score >= 2, "player 1 collected coins");
    check(game.players[0].deaths >= 2, "player 1 died to pit and saw");
    // Four players, four input slots: every non-scripted player must move.
    check(demo::kPlayerCount == 4, "the demo has four players");
    for (int i = 1; i < demo::kPlayerCount; ++i) {
        const float moved = run.displacement[static_cast<size_t>(i)];
        if (moved < 40.0f) {
            std::fprintf(stderr, "  FAIL: player %d only moved %.1f\n", i + 1, moved);
            ++failures;
        }
    }
    check(run.sawTravel > 100.0f, "hazards patrolled");
    check(game.coinsCollected >= 2, "coins left play");
    // Coins are contested: with four players the total must equal the sum of
    // everyone's score, not just player 1's.
    int totalScore = 0;
    for (int i = 0; i < demo::kPlayerCount; ++i) {
        totalScore += game.players[i].score;
    }
    check(game.coinsCollected == totalScore,
          "collected coins became player scores");

    // Sound cues are gameplay evidence, not device evidence.
    check(run.cues[static_cast<size_t>(demo::Sfx::Jump)] > 0, "a jump cue was emitted");
    check(run.cues[static_cast<size_t>(demo::Sfx::Coin)] >= 2, "coin cues were emitted");
    check(run.cues[static_cast<size_t>(demo::Sfx::Death)] >= 2, "death cues were emitted");
    check(run.cues[static_cast<size_t>(demo::Sfx::Win)] == 1, "exactly one win cue");
    std::printf("  scripted run: %d steps, p1 score %d, deaths %d, "
                "moved=[%.0f %.0f %.0f %.0f], cues=[%d %d %d %d]\n",
                run.steps, game.players[0].score, game.players[0].deaths,
                run.displacement[0], run.displacement[1], run.displacement[2],
                run.displacement[3], run.cues[0], run.cues[1], run.cues[2],
                run.cues[3]);

    // --- determinism: an identical run produces an identical outcome -------
    const RunResult again = runScripted();
    check(again.game.players[0].score == game.players[0].score,
          "score is deterministic across runs");
    check(again.game.players[0].deaths == game.players[0].deaths,
          "deaths are deterministic across runs");
    check(again.displacement == run.displacement, "player paths are deterministic");
    check(again.cues == run.cues, "sound cues are deterministic");
    check(again.sawTravel == run.sawTravel, "hazard patrol is deterministic");

    // --- pause freezes the simulation --------------------------------------
    {
        ply::World world;
        demo::Game fresh;
        fresh.build(world);
        for (int i = 0; i < 30; ++i) {
            fresh.step(world, {}, kStep);
        }
        const float timeBefore = fresh.time;
        fresh.togglePause();
        check(fresh.state == demo::GameState::Paused, "togglePause pauses");
        const auto p1Before = world.get<ply::Transform>(fresh.players[0].entity)->position;
        for (int i = 0; i < 30; ++i) {
            fresh.step(world, {}, kStep);
        }
        check(fresh.time == timeBefore, "paused time does not advance");
        const auto p1After = world.get<ply::Transform>(fresh.players[0].entity)->position;
        check(p1Before.x == p1After.x && p1Before.y == p1After.y,
              "paused positions do not move");
        fresh.togglePause();
        fresh.step(world, {}, kStep);
        check(fresh.time > timeBefore, "unpausing resumes time");
    }

    // --- reset restores a playable level ------------------------------------
    {
        ply::World world;
        demo::Game fresh;
        fresh.build(world);
        fresh.reset(world);
        check(fresh.state == demo::GameState::Playing, "reset returns to Playing");
        check(fresh.winner == -1, "reset clears the winner");
        check(fresh.players[0].score == 0, "reset clears scores");
        check(static_cast<int>(fresh.coins.size()) == fresh.initialCoinCount,
              "reset restores every coin");
        check(world.entityCount() > 0, "reset leaves live entities");
    }

    if (failures == 0) {
        std::printf("platformer test: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "platformer test: %d failure(s)\n", failures);
    return 1;
}
