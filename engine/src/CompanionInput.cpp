#include "plyEngine/CompanionInput.hpp"

namespace ply::companion {

namespace {
bool validSlot(int slot) { return slot >= 0 && slot < CompanionInput::kMaxSlots; }
}  // namespace

void CompanionInput::push(int slot, const ControllerState& state, uint32_t seq) {
    if (!validSlot(slot)) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(Event{slot, state, seq});
}

void CompanionInput::drain() {
    // Everything runs under the lock: drain() is the only writer of current_
    // and previous_, and readers may call state() from another thread.
    std::lock_guard<std::mutex> lock(mutex_);

    previous_ = current_;

    for (const Event& event : queue_) {
        if (!validSlot(event.slot)) {
            continue;
        }
        // Drop frames older than what we already applied for this slot, so a
        // delayed packet cannot rewind input.
        if (hasSeq_[event.slot] && event.seq != 0 && event.seq <= lastSeq_[event.slot]) {
            continue;
        }
        if (event.seq != 0) {
            lastSeq_[event.slot] = event.seq;
            hasSeq_[event.slot] = true;
        }
        current_[event.slot] = event.state;
    }

    queue_.clear();
}

ControllerState CompanionInput::state(int slot) const {
    if (!validSlot(slot)) {
        return {};
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return current_[slot];
}

ControllerState CompanionInput::previous(int slot) const {
    if (!validSlot(slot)) {
        return {};
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return previous_[slot];
}

bool CompanionInput::pressed(int slot, uint32_t button) const {
    if (!validSlot(slot)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return (current_[slot].buttons & button) != 0 &&
           (previous_[slot].buttons & button) == 0;
}

bool CompanionInput::released(int slot, uint32_t button) const {
    if (!validSlot(slot)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return (current_[slot].buttons & button) == 0 &&
           (previous_[slot].buttons & button) != 0;
}

void CompanionInput::clearSlot(int slot) {
    if (!validSlot(slot)) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    current_[slot] = {};
    previous_[slot] = {};
    hasSeq_[slot] = false;
    lastSeq_[slot] = 0;
}

size_t CompanionInput::pendingCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

}  // namespace ply::companion
