#pragma once

#include "plyEngine/CompanionProtocol.hpp"

#include <array>
#include <cstdint>
#include <mutex>
#include <vector>

namespace ply::companion {

/// Thread-safe bridge from network threads to the fixed-step simulation.
///
/// WebSocket callbacks push controller states from Crow's threads; the
/// simulation calls drain() once per fixed step. The simulation therefore only
/// ever reads state between steps, so it stays single-threaded and
/// deterministic, and it never takes a lock while simulating.
class CompanionInput {
public:
    static constexpr int kMaxSlots = 8;

    /// Called from a network thread. `seq` is the client's sequence number;
    /// out-of-order frames (older than the last applied for that slot) are
    /// dropped so a delayed packet cannot rewind input.
    void push(int slot, const ControllerState& state, uint32_t seq);

    /// Applies every queued event. Call at the start of a fixed step.
    void drain();

    /// State as of the last drain().
    ControllerState state(int slot) const;

    /// State as of the drain() before that; used for edge detection.
    ControllerState previous(int slot) const;

    /// True when the button went from up to down across the last drain().
    bool pressed(int slot, uint32_t button) const;
    /// True when the button went from down to up across the last drain().
    bool released(int slot, uint32_t button) const;

    /// Forgets a slot (player left).
    void clearSlot(int slot);

    /// Number of queued events not yet applied (diagnostics/tests).
    size_t pendingCount() const;

private:
    struct Event {
        int slot = 0;
        ControllerState state;
        uint32_t seq = 0;
    };

    mutable std::mutex mutex_;
    std::vector<Event> queue_;
    std::array<ControllerState, kMaxSlots> current_{};
    std::array<ControllerState, kMaxSlots> previous_{};
    std::array<uint32_t, kMaxSlots> lastSeq_{};
    std::array<bool, kMaxSlots> hasSeq_{};
};

}  // namespace ply::companion
