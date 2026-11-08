// Headless companion-protocol tests: framing, room codes, sessions and the
// fixed-step input queue. No GL, no network required.

#include "plyEngine/CompanionInput.hpp"
#include "plyEngine/CompanionProtocol.hpp"
#include "plyEngine/CompanionSession.hpp"
#include "plyEngine/RoomCode.hpp"

#include <cstdio>
#include <random>
#include <string>
#include <thread>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
    if (!cond) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

using namespace ply::companion;

}  // namespace

int main() {
    // --- protocol: envelope round-trip ------------------------------------
    {
        const std::string text = encodeMessage(kTypeJoin, 7, {{"room", "ABCD"}});
        Message msg;
        std::string error;
        check(decodeMessage(text, msg, error), "encoded frame decodes");
        check(msg.type == kTypeJoin, "type survives");
        check(msg.seq == 7, "seq survives");
        check(msg.version == kProtocolVersion, "version is stamped");
        check(msg.payload["room"] == "ABCD", "payload survives");
    }

    // --- protocol: rejects malformed input --------------------------------
    {
        Message msg;
        std::string error;

        check(!decodeMessage("{not json", msg, error), "bad JSON rejected");
        check(!error.empty(), "bad JSON sets an error");

        check(!decodeMessage("[1,2,3]", msg, error), "non-object rejected");

        check(!decodeMessage(R"({"type":"join"})", msg, error),
              "missing version rejected");

        check(!decodeMessage(R"({"v":1})", msg, error), "missing type rejected");

        check(!decodeMessage(R"({"v":1,"type":5})", msg, error),
              "non-string type rejected");
    }

    // --- protocol: version mismatch is rejected with a clear message ------
    {
        Message msg;
        std::string error;
        const std::string future = R"({"v":999,"type":"join"})";
        check(!decodeMessage(future, msg, error), "future version rejected");
        check(error.find("mismatch") != std::string::npos,
              "mismatch error mentions the problem");

        const std::string past = R"({"v":0,"type":"join"})";
        check(!decodeMessage(past, msg, error), "old version rejected");
    }

    // --- protocol: missing payload defaults to an empty object -----------
    {
        Message msg;
        std::string error;
        check(decodeMessage(R"({"v":1,"type":"ping"})", msg, error),
              "frame without payload is allowed");
        check(msg.payload.is_object(), "payload defaults to an object");
        check(msg.seq == 0, "seq defaults to 0");
    }

    // --- controller encoding clamps axes ---------------------------------
    {
        ControllerState wild;
        wild.x = 5.0f;
        wild.y = -9.0f;
        wild.buttons = kButtonA | kButtonB;

        const nlohmann::json payload = encodeController(wild);
        check(payload["x"].get<float>() == 1.0f, "x clamped to +1");
        check(payload["y"].get<float>() == -1.0f, "y clamped to -1");

        ControllerState back;
        std::string error;
        check(decodeController(payload, back, error), "controller decodes");
        check(back.buttons == (kButtonA | kButtonB), "buttons survive");
        check(back.x == 1.0f && back.y == -1.0f, "clamped values survive");
    }

    // --- controller: bad button type is rejected -------------------------
    {
        ControllerState out;
        std::string error;
        check(!decodeController({{"buttons", "nope"}}, out, error),
              "non-integer buttons rejected");
        check(!decodeController("nope", out, error), "non-object payload rejected");
    }

    // --- room codes -------------------------------------------------------
    {
        std::mt19937 rng(1234);
        const RoomCode code = RoomCode::generate(rng);
        check(code.valid(), "generated code is valid");
        check(static_cast<int>(code.value().size()) == RoomCode::kLength,
              "generated code has the right length");

        // The alphabet must not contain easily confused characters.
        const std::string alphabet = RoomCode::alphabet();
        for (char bad : {'I', 'O', 'i', 'o', '0', '1'}) {
            check(alphabet.find(bad) == std::string::npos,
                  "alphabet excludes ambiguous characters");
        }

        check(code.matches(code.value()), "code matches itself");
        std::string lower = code.value();
        for (char& c : lower) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        check(code.matches(lower), "matching is case-insensitive");
        check(!code.matches("ZZZZ") || code.value() == "ZZZZ",
              "a different code does not match");
    }

    // --- room code normalization -----------------------------------------
    {
        check(normalizeRoomCode("ab-cd") == "ABCD", "hyphens stripped, upper-cased");
        check(normalizeRoomCode(" AB CD ") == "ABCD", "spaces stripped and trimmed");
        check(normalizeRoomCode("ABCD") == "ABCD", "already normal");
        check(normalizeRoomCode("ABC").empty(), "too short is rejected");
        check(normalizeRoomCode("ABCDE").empty(), "too long is rejected");
        check(normalizeRoomCode("AB0D").empty(), "ambiguous 0 rejected");
        check(normalizeRoomCode("ABID").empty(), "ambiguous I rejected");
        check(normalizeRoomCode("").empty(), "empty rejected");
    }

    // --- session: join, full room, wrong code ----------------------------
    {
        const RoomCode room("ABCD");
        CompanionSession session(2, 10.0, room);

        std::string error;
        const int a = session.join("ABCD", "Alice", 0.0, error);
        check(a == 0, "first player gets slot 0");
        const int b = session.join("abcd", "Bob", 0.0, error);
        check(b == 1, "second player gets slot 1 (case-insensitive code)");
        check(session.connectedCount() == 2, "two connected");

        error.clear();
        check(session.join("ABCD", "Cara", 0.0, error) == -1, "room full");
        check(error == "the room is full", "full-room error message");

        error.clear();
        check(session.join("WXYZ", "Dan", 0.0, error) == -1, "wrong code rejected");
        check(!error.empty(), "wrong code sets an error");

        error.clear();
        check(session.join("ABCD", "  ", 0.0, error) == -1, "blank name rejected");
    }

    // --- session: duplicate name while connected -------------------------
    {
        CompanionSession session(3, 10.0, RoomCode("ABCD"));
        std::string error;
        check(session.join("ABCD", "Alice", 0.0, error) == 0, "Alice joins");
        error.clear();
        check(session.join("ABCD", "Alice", 0.0, error) == -1,
              "duplicate name while connected rejected");
    }

    // --- session: reconnect inside the grace window ----------------------
    {
        CompanionSession session(2, 10.0, RoomCode("ABCD"));
        std::string error;
        const int slot = session.join("ABCD", "Alice", 0.0, error);
        check(slot == 0, "Alice joins");

        session.setConnected(slot, false, 5.0);
        check(session.connectedCount() == 0, "disconnect counted");

        session.update(10.0);
        check(session.slot(slot) != nullptr && session.slot(slot)->claimed,
              "slot still reserved inside the grace window");

        const int again = session.join("ABCD", "Alice", 12.0, error);
        check(again == slot, "reconnect re-binds to the same slot");
        check(session.slot(slot)->connected, "reconnected slot is connected");
    }

    // --- session: grace expiry frees the slot ----------------------------
    {
        CompanionSession session(1, 5.0, RoomCode("ABCD"));
        std::string error;
        const int slot = session.join("ABCD", "Alice", 0.0, error);
        session.setConnected(slot, false, 1.0);

        session.update(3.0);
        check(session.slot(slot)->claimed, "still reserved at 2s");

        session.update(7.0);
        check(!session.slot(slot)->claimed, "slot freed after the grace window");

        // Someone else can now take it.
        const int other = session.join("ABCD", "Bob", 7.0, error);
        check(other == slot, "freed slot is reusable");

        // And the original name no longer rebinds to it.
        error.clear();
        check(session.join("ABCD", "Alice", 7.0, error) == -1, "room is full again");
    }

    // --- input queue: per-step semantics --------------------------------
    {
        CompanionInput input;

        ControllerState press;
        press.x = 1.0f;
        press.buttons = kButtonA;
        input.push(0, press, 1);
        check(input.pendingCount() == 1, "event queued");
        check(input.state(0).x == 0.0f, "state is not visible before drain");

        input.drain();
        check(input.pendingCount() == 0, "queue cleared by drain");
        check(input.state(0).x == 1.0f, "state applied at drain");
        check(input.pressed(0, kButtonA), "button edge detected on the first step");
        check(!input.pressed(0, kButtonB), "unrelated button not detected");

        input.drain();
        check(!input.pressed(0, kButtonA), "held button is not pressed twice");

        ControllerState release;
        input.push(0, release, 2);
        input.drain();
        check(input.released(0, kButtonA), "release edge detected");
        check(input.state(0).x == 0.0f, "axis cleared");
    }

    // --- input queue: stale sequence numbers are dropped -----------------
    {
        CompanionInput input;

        ControllerState newer;
        newer.x = 1.0f;
        input.push(0, newer, 10);
        input.drain();
        check(input.state(0).x == 1.0f, "newer frame applied");

        ControllerState stale;
        stale.x = -1.0f;
        input.push(0, stale, 5);  // older than what we already applied
        input.drain();
        check(input.state(0).x == 1.0f, "stale frame cannot rewind input");
    }

    // --- input queue: slots are independent -----------------------------
    {
        CompanionInput input;
        ControllerState a;
        a.x = 1.0f;
        ControllerState b;
        b.x = -1.0f;
        input.push(0, a, 1);
        input.push(1, b, 1);
        input.drain();
        check(input.state(0).x == 1.0f, "slot 0 independent");
        check(input.state(1).x == -1.0f, "slot 1 independent");

        input.clearSlot(0);
        check(input.state(0).x == 0.0f, "clearSlot resets the slot");
        check(input.state(1).x == -1.0f, "other slots untouched");
    }

    // --- input queue: out-of-range slots are ignored --------------------
    {
        CompanionInput input;
        ControllerState state;
        state.x = 1.0f;
        input.push(-1, state, 1);
        input.push(CompanionInput::kMaxSlots, state, 1);
        check(input.pendingCount() == 0, "invalid slots are not queued");
        input.drain();
        check(input.state(-1).x == 0.0f, "invalid slot reads as neutral");
    }

    // --- input queue: concurrent pushes then drain ----------------------
    {
        CompanionInput input;
        std::thread producer([&input] {
            for (int i = 0; i < 1000; ++i) {
                ControllerState state;
                state.x = 0.5f;
                input.push(0, state, static_cast<uint32_t>(i + 1));
            }
        });
        for (int i = 0; i < 200; ++i) {
            input.drain();
        }
        producer.join();
        input.drain();
        check(input.state(0).x == 0.5f, "final state applied after concurrent pushes");
        check(input.pendingCount() == 0, "queue drained");
    }

    if (failures == 0) {
        std::printf("companion tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "companion tests: %d failure(s)\n", failures);
    return 1;
}
