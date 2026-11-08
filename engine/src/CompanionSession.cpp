#include "plyEngine/CompanionSession.hpp"

#include <algorithm>

namespace ply::companion {

namespace {
constexpr int kColourCount = 6;
constexpr size_t kMaxNameLength = 24;

std::string trimName(const std::string& input) {
    const size_t first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const size_t last = input.find_last_not_of(" \t\r\n");
    std::string name = input.substr(first, last - first + 1);
    if (name.size() > kMaxNameLength) {
        name.resize(kMaxNameLength);
    }
    return name;
}
}  // namespace

CompanionSession::CompanionSession(int slotCount, double reconnectGrace, RoomCode room)
    : reconnectGrace_(reconnectGrace > 0.0 ? reconnectGrace : 0.0),
      room_(std::move(room)) {
    const int count = slotCount > 0 ? slotCount : 0;
    slots_.resize(static_cast<size_t>(count));
    for (int i = 0; i < count; ++i) {
        SlotInfo& info = slots_[static_cast<size_t>(i)];
        info.slot = i;
        info.colourIndex = i % kColourCount;
    }
}

void CompanionSession::releaseSlot(SlotInfo& info) {
    info.claimed = false;
    info.connected = false;
    info.name.clear();
    info.disconnectedAt = -1.0;
    info.disconnectedFor = 0.0;
}

int CompanionSession::unusedSlot() const {
    for (const SlotInfo& info : slots_) {
        if (!info.claimed) {
            return info.slot;
        }
    }
    return -1;
}

int CompanionSession::slotByName(const std::string& name) const {
    if (name.empty()) {
        return -1;
    }
    for (const SlotInfo& info : slots_) {
        if (info.claimed && info.name == name) {
            return info.slot;
        }
    }
    return -1;
}

int CompanionSession::join(const std::string& roomCode, const std::string& playerName,
                           double now, std::string& error) {
    if (!room_.valid()) {
        error = "no room is open";
        return -1;
    }
    if (!room_.matches(roomCode)) {
        error = "wrong room code";
        return -1;
    }

    const std::string name = trimName(playerName);
    if (name.empty()) {
        error = "a player name is required";
        return -1;
    }

    // Reconnect: same name, slot still inside its grace window.
    const int existing = slotByName(name);
    if (existing >= 0) {
        SlotInfo& info = slots_[static_cast<size_t>(existing)];
        if (info.connected) {
            error = "that name is already connected";
            return -1;
        }
        const double elapsed =
            info.disconnectedAt >= 0.0 ? now - info.disconnectedAt : 0.0;
        if (elapsed <= reconnectGrace_) {
            info.connected = true;
            info.disconnectedAt = -1.0;
            info.disconnectedFor = 0.0;
            return existing;
        }
        // Grace elapsed: free it and fall through to a fresh claim.
        releaseSlot(info);
    }

    const int freeSlot = unusedSlot();
    if (freeSlot < 0) {
        error = "the room is full";
        return -1;
    }

    SlotInfo& info = slots_[static_cast<size_t>(freeSlot)];
    info.claimed = true;
    info.connected = true;
    info.name = name;
    info.disconnectedAt = -1.0;
    info.disconnectedFor = 0.0;
    return freeSlot;
}

void CompanionSession::setConnected(int slotIndex, bool connected, double now) {
    if (slotIndex < 0 || slotIndex >= static_cast<int>(slots_.size())) {
        return;
    }
    SlotInfo& info = slots_[static_cast<size_t>(slotIndex)];
    if (!info.claimed || info.connected == connected) {
        return;
    }

    if (connected) {
        info.connected = true;
        info.disconnectedAt = -1.0;
        info.disconnectedFor = 0.0;
    } else {
        info.connected = false;
        info.disconnectedAt = now;
        info.disconnectedFor = 0.0;
    }
}

void CompanionSession::update(double now) {
    for (SlotInfo& info : slots_) {
        if (!info.claimed || info.connected) {
            continue;
        }
        if (info.disconnectedAt < 0.0) {
            info.disconnectedAt = now;
        }
        info.disconnectedFor = now - info.disconnectedAt;
        if (info.disconnectedFor > reconnectGrace_) {
            releaseSlot(info);
        }
    }
}

const SlotInfo* CompanionSession::slot(int index) const {
    if (index < 0 || index >= static_cast<int>(slots_.size())) {
        return nullptr;
    }
    return &slots_[static_cast<size_t>(index)];
}

int CompanionSession::connectedCount() const {
    int count = 0;
    for (const SlotInfo& info : slots_) {
        if (info.claimed && info.connected) {
            ++count;
        }
    }
    return count;
}

}  // namespace ply::companion
