// Headless test of the M7 reference demo's game logic (no GL, no window).
//
// The platformer's Game + Autopilot live in examples/companion/platformer.hpp
// precisely so this test can drive a full scripted playthrough and assert
// real gameplay outcomes: coins collected, deaths to pit and saw, independent
// second player, hazards patrolling, and a win for player 1.

#include "platformer.hpp"

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
    float p2FinalX = 0.0f;
    float sawTravel = 0.0f;
    int steps = 0;
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
        ++result.steps;
    }

    if (const ply::Transform* p2 =
            world.get<ply::Transform>(result.game.players[1].entity)) {
        result.p2FinalX = p2->position.x;
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
    check(run.p2FinalX < game.players[1].spawn.x - 60.0f,
          "player 2 moved independently");
    check(run.sawTravel > 100.0f, "hazards patrolled");
    check(game.coinsCollected >= 2, "coins left play");
    check(game.coinsCollected == game.players[0].score,
          "collected coins became player 1's score");
    std::printf("  scripted run: %d steps, p1 score %d, deaths %d, p2 x %.0f\n",
                run.steps, game.players[0].score, game.players[0].deaths,
                run.p2FinalX);

    // --- determinism: an identical run produces an identical outcome -------
    const RunResult again = runScripted();
    check(again.game.players[0].score == game.players[0].score,
          "score is deterministic across runs");
    check(again.game.players[0].deaths == game.players[0].deaths,
          "deaths are deterministic across runs");
    check(again.p2FinalX == run.p2FinalX, "player 2 path is deterministic");
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
