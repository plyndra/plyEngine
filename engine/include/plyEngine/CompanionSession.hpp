#pragma once

#include "plyEngine/CompanionProtocol.hpp"
#include "plyEngine/RoomCode.hpp"

#include <string>
#include <vector>

namespace ply::companion {

/// One player slot.
struct SlotInfo {
    int slot = 0;
    std::string name;
    int colourIndex = 0;
    bool claimed = false;
    bool connected = false;
    /// Monotonic time the slot was disconnected, or < 0 when connected.
    double disconnectedAt = -1.0;
    /// Seconds spent disconnected (derived by update()).
    double disconnectedFor = 0.0;
};

/// Player slots with a reconnect grace window.
///
/// A phone that drops (screen lock, tunnel, Wi-Fi hiccup) keeps its slot for
/// `reconnectGrace` seconds and can re-bind to it by name. After the window the
/// slot frees. Time is injected as a monotonic `now` in seconds, so the whole
/// state machine is deterministic and testable without real clocks.
class CompanionSession {
public:
    CompanionSession(int slotCount, double reconnectGrace, RoomCode room);

    int slotCount() const { return static_cast<int>(slots_.size()); }
    const RoomCode& room() const { return room_; }
    double reconnectGrace() const { return reconnectGrace_; }

    /// Attempts to claim a slot.
    /// Returns the slot index, or -1 with `error` set.
    /// A matching name re-binds to a slot still inside its grace window.
    int join(const std::string& roomCode, const std::string& playerName, double now,
             std::string& error);

    /// Marks a slot disconnected (grace starts at `now`) or connected.
    void setConnected(int slot, bool connected, double now);

    /// Recomputes grace timers and frees slots whose window elapsed.
    /// Call once per fixed step.
    void update(double now);

    const SlotInfo* slot(int index) const;
    int connectedCount() const;
    std::vector<SlotInfo> slots() const { return slots_; }

private:
    int unusedSlot() const;
    int slotByName(const std::string& name) const;
    void releaseSlot(SlotInfo& info);

    std::vector<SlotInfo> slots_;
    double reconnectGrace_ = 20.0;
    RoomCode room_;
};

}  // namespace ply::companion
