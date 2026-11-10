// Headless Slot tests (M1a a5.2): E-a5.2 — slot state → region selection, and
// the stack-count formatting rules — graded against the fixture atlas so the
// mapping is checked against real data, not just names. No GL context, no
// window, no clock, no float.

#include "plyEngine/UISlot.hpp"
#include "plyEngine/UIAtlas.hpp"

#include <cstdio>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void check(bool condition, const char* what) {
    if (!condition) {
        std::fprintf(stderr, "  FAIL: %s\n", what);
        ++failures;
    }
}

void eq(long long got, long long want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got %lld, want %lld)\n", what, got, want);
        ++failures;
    }
}

void eqStr(std::string_view got, std::string_view want, const char* what) {
    if (got != want) {
        std::fprintf(stderr, "  FAIL: %s (got \"%.*s\", want \"%.*s\")\n", what,
                     static_cast<int>(got.size()), got.data(), static_cast<int>(want.size()),
                     want.data());
        ++failures;
    }
}

using ply::UISlotState;

// --- E-a5.2: state -> region selection --------------------------------------

void testStateToRegionName() {
    // The mapping is total: one distinct region name per state, pinned.
    eqStr(ply::slotFrameRegionName(UISlotState::Normal), "slot.normal", "Normal -> slot.normal");
    eqStr(ply::slotFrameRegionName(UISlotState::Selected), "slot.selected",
          "Selected -> slot.selected");
    eqStr(ply::slotFrameRegionName(UISlotState::Disabled), "slot.disabled",
          "Disabled -> slot.disabled");

    // The tokens are stable and distinct (the contract tests and the art lint
    // match on, same as the atlas error codes).
    eqStr(ply::toString(UISlotState::Normal), "Normal", "token Normal");
    eqStr(ply::toString(UISlotState::Selected), "Selected", "token Selected");
    eqStr(ply::toString(UISlotState::Disabled), "Disabled", "token Disabled");
    check(ply::toString(UISlotState::Normal) != ply::toString(UISlotState::Selected) &&
              ply::toString(UISlotState::Selected) != ply::toString(UISlotState::Disabled),
          "state tokens are distinct");
}

void testRegionsExistInFixture() {
    const ply::UIAtlasLoadResult result =
        ply::loadUIAtlasFile(std::string(PLY_TEST_DATA_DIR) + "/ui_atlas.json");
    check(result.ok(), "fixture atlas loads");
    if (!result.ok()) return;

    const ply::UIAtlas& atlas = result.atlas;
    // Every state's frame region exists in the fixture, with slot geometry.
    struct StateCase {
        UISlotState state;
        int x, y;
    };
    const StateCase cases[] = {
        {UISlotState::Normal, 0, 48},
        {UISlotState::Selected, 112, 48},
        {UISlotState::Disabled, 132, 48},
    };
    int found = 0;
    for (const StateCase& c : cases) {
        const ply::UIAtlasRegion* region =
            atlas.findRegion(ply::slotFrameRegionName(c.state));
        check(region != nullptr, "state's frame region exists in the fixture");
        if (region == nullptr) continue;
        ++found;
        eq(region->x, c.x, "frame region x");
        eq(region->y, c.y, "frame region y");
        eq(region->w, 20, "frame region is a 20x20 slot");
        eq(region->h, 20, "frame region is a 20x20 slot");
    }
    eq(found, 3, "all 3 state regions found in the fixture atlas");

    // A missing state region is discoverable: a slot frame name that is not in
    // the atlas finds nothing (this is what the art lint will flag).
    check(atlas.findRegion("slot.hovered") == nullptr, "unknown state name finds nothing");
}

// --- E-a5.2: count formatting -------------------------------------------------

void testCountFormatting() {
    // Pinned rules: <= 1 empty; 2..999 decimal; >= 1000 "999+".
    struct CountCase {
        int count;
        const char* label;
    };
    const CountCase cases[] = {
        {0, ""},   // empty slot
        {1, ""},   // a lone item shows no number
        {2, "2"},
        {10, "10"},
        {27, "27"},
        {999, "999"},
        {1000, "999+"},    // first value past the decimal range
        {999999, "999+"},  // far past it
        {-5, ""},          // impossible count degrades to empty, not garbage
    };
    for (const CountCase& c : cases) {
        eqStr(ply::formatStackCount(c.count), c.label, "stack count label");
    }
}

// One line of measured values, printed so a ledger entry can be a verbatim
// transcript instead of prose. Every value here is also asserted above.
void printEvidence(int regionsFound) {
    std::printf(
        "EVIDENCE ui_slot frame=Normal:slot.normal/Selected:slot.selected/Disabled:slot.disabled "
        "tokens=Normal/Selected/Disabled counts=0:'',1:'',27:'27',999:'999',1000:'999+' "
        "fixture_regions=%d/3\n",
        regionsFound);
}

}  // namespace

int main() {
    testStateToRegionName();

    int regionsFound = 0;
    {
        // testRegionsExistInFixture reports its count via a rerun here, so the
        // transcript can quote it without globals.
        const ply::UIAtlasLoadResult result =
            ply::loadUIAtlasFile(std::string(PLY_TEST_DATA_DIR) + "/ui_atlas.json");
        if (result.ok()) {
            for (const UISlotState state :
                 {UISlotState::Normal, UISlotState::Selected, UISlotState::Disabled}) {
                if (result.atlas.findRegion(ply::slotFrameRegionName(state)) != nullptr)
                    ++regionsFound;
            }
        }
    }
    testRegionsExistInFixture();
    testCountFormatting();

    if (failures == 0) {
        printEvidence(regionsFound);
        std::printf("ui_slot tests: PASS\n");
        return 0;
    }
    std::fprintf(stderr, "ui_slot tests: %d failure(s)\n", failures);
    return 1;
}
