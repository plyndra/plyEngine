#include "plyEngine/UISlot.hpp"

namespace ply {

const char* toString(UISlotState state) {
    switch (state) {
        case UISlotState::Normal: return "Normal";
        case UISlotState::Selected: return "Selected";
        case UISlotState::Disabled: return "Disabled";
    }
    return "";  // unreachable; every enumerator is handled
}

std::string_view slotFrameRegionName(UISlotState state) {
    switch (state) {
        case UISlotState::Normal: return "slot.normal";
        case UISlotState::Selected: return "slot.selected";
        case UISlotState::Disabled: return "slot.disabled";
    }
    return {};  // unreachable; every enumerator is handled
}

std::string formatStackCount(int count) {
    if (count <= 1) return {};  // empty slot or lone item: no number
    if (count >= 1000) return "999+";
    return std::to_string(count);
}

}  // namespace ply
