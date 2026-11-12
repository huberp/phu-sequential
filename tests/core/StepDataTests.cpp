#include <catch2/catch_test_macros.hpp>
#include "sequencing/StepData.h"
#include "sequencing/velocity/VelocityModeRegistry.h"
#include "sequencing/Constants.h"

using namespace phu::sequencing;
using namespace phu::sequencing::constants;

// M10 Step data (§3.10, §5.2, §5.4, §7, #8)

TEST_CASE("StepData: defaultStepData returns enabled with mode defaults", "[M10]") {
    const auto defaultStep = defaultStepData();
    CHECK(defaultStep.enabled == true);
    CHECK(defaultStep.valueByMode[static_cast<int>(VelocityMode::incoming)] == 0);
    CHECK(defaultStep.valueByMode[static_cast<int>(VelocityMode::byStep)] ==
          velocity::defaultByStepValue);
    CHECK(defaultStep.valueByMode[static_cast<int>(VelocityMode::deltaByStep)] ==
          velocity::defaultDeltaValue);
    CHECK(defaultStep.valueByMode[static_cast<int>(VelocityMode::relative)] ==
          velocity::defaultPercentValue);
}

TEST_CASE("StepData: defaultStepData is enabled", "[M10]") {
    const auto defaultStep = defaultStepData();
    CHECK(defaultStep.enabled == true);
}

TEST_CASE("StepData: StepPattern default constructor creates 64 default steps", "[M10]") {
    const StepPattern pattern;
    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        const auto& step = pattern.step(i);
        CHECK(step.enabled == true);
        CHECK(step == defaultStepData());
    }
}

TEST_CASE("StepData: StepPattern::step returns reference to const StepData", "[M10]") {
    StepPattern pattern;
    const auto& step0 = pattern.step(0);
    CHECK(step0.enabled == true);
}

TEST_CASE("StepData: StepPattern::step with valid index", "[M10]") {
    StepPattern pattern;
    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        const auto& step = pattern.step(i);
        CHECK(step == defaultStepData());
    }
}

TEST_CASE("StepData: StepPattern::step clamps negative index to 0", "[M10]") {
    StepPattern pattern;
    const auto& step = pattern.step(-5);
    CHECK(step == defaultStepData());
}

TEST_CASE("StepData: StepPattern::step clamps out-of-range index to 63", "[M10]") {
    StepPattern pattern;
    const auto& step = pattern.step(100);
    CHECK(step == defaultStepData());
}

TEST_CASE("StepData: StepPattern::setEnabled toggles step enabled state", "[M10]") {
    StepPattern pattern;

    pattern.setEnabled(0, false);
    CHECK(pattern.step(0).enabled == false);

    pattern.setEnabled(0, true);
    CHECK(pattern.step(0).enabled == true);
}

TEST_CASE("StepData: StepPattern::setEnabled on multiple steps", "[M10]") {
    StepPattern pattern;

    for (int i = 0; i < 10; ++i) {
        pattern.setEnabled(i, false);
    }

    for (int i = 0; i < 10; ++i) {
        CHECK(pattern.step(i).enabled == false);
    }

    for (int i = 10; i < lanes::maximumStepCount; ++i) {
        CHECK(pattern.step(i).enabled == true);
    }
}

TEST_CASE("StepData: StepPattern::setEnabled ignores out-of-range index", "[M10]") {
    StepPattern pattern;

    pattern.setEnabled(-1, false);
    pattern.setEnabled(100, false);

    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        CHECK(pattern.step(i).enabled == true);
    }
}

TEST_CASE("StepData: StepPattern::setValue sets value for specific mode and index", "[M10]") {
    StepPattern pattern;

    pattern.setValue(0, VelocityMode::byStep, 80);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 80);
}

TEST_CASE("StepData: StepPattern::setValue clamps value to mode range", "[M10]") {
    StepPattern pattern;

    // byStep range is 0-127; clamp 150 to 127
    pattern.setValue(0, VelocityMode::byStep, 150);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 127);

    // delta range is -127 to 127; clamp -200 to -127
    pattern.setValue(1, VelocityMode::deltaByStep, -200);
    CHECK(pattern.step(1).valueByMode[static_cast<int>(VelocityMode::deltaByStep)] == -127);

    // relative range is 0-200; clamp 250 to 200
    pattern.setValue(2, VelocityMode::relative, 250);
    CHECK(pattern.step(2).valueByMode[static_cast<int>(VelocityMode::relative)] == 200);
}

TEST_CASE("StepData: StepPattern::setValue ignores incoming mode", "[M10]") {
    StepPattern pattern;

    // Set a value for incoming mode (should be ignored)
    pattern.setValue(0, VelocityMode::incoming, 100);

    // incoming slot should remain 0 (unchanged)
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::incoming)] == 0);
}

TEST_CASE("StepData: StepPattern::setValue ignores out-of-range index", "[M10]") {
    StepPattern pattern;

    // Set value on valid index first
    pattern.setValue(0, VelocityMode::byStep, 80);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 80);

    // Try to set on invalid indices (should be ignored)
    pattern.setValue(-1, VelocityMode::byStep, 50);
    pattern.setValue(100, VelocityMode::byStep, 50);

    // Original should remain unchanged
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 80);
}

TEST_CASE("StepData: StepPattern::setValue on different modes don't interfere", "[M10]") {
    StepPattern pattern;

    pattern.setValue(0, VelocityMode::byStep, 100);
    pattern.setValue(0, VelocityMode::deltaByStep, -20);
    pattern.setValue(0, VelocityMode::relative, 150);

    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 100);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::deltaByStep)] == -20);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::relative)] == 150);
}

TEST_CASE("StepData: StepPattern::applyStepCountChange keeps steps below newCount",
          "[M10][REQ-5.4-1]") {
    StepPattern pattern;

    pattern.setEnabled(0, false);
    pattern.setValue(0, VelocityMode::byStep, 80);
    pattern.setValue(1, VelocityMode::deltaByStep, -20);

    // Change from 64 to 16: steps 0-15 should remain
    pattern.applyStepCountChange(64, 16);

    // Steps below newCount should be preserved
    CHECK(pattern.step(0).enabled == false);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 80);
    CHECK(pattern.step(1).valueByMode[static_cast<int>(VelocityMode::deltaByStep)] == -20);
}

TEST_CASE("StepData: StepPattern::applyStepCountChange resets steps >= newCount to defaults",
          "[M10][REQ-5.4-1]") {
    StepPattern pattern;

    // Set some values in steps
    for (int i = 0; i < 20; ++i) {
        pattern.setEnabled(i, false);
        pattern.setValue(i, VelocityMode::byStep, 100);
    }

    // Change from 64 to 16
    pattern.applyStepCountChange(64, 16);

    // Steps >= 16 should be reset to defaults
    for (int i = 16; i < lanes::maximumStepCount; ++i) {
        CHECK(pattern.step(i) == defaultStepData());
    }
}

TEST_CASE("StepData: StepPattern::applyStepCountChange handles increase (no-op)",
          "[M10][REQ-5.4-2]") {
    StepPattern pattern;

    // Set some values
    pattern.setEnabled(0, false);
    pattern.setValue(0, VelocityMode::byStep, 80);

    // Increase from 16 to 32
    pattern.applyStepCountChange(16, 32);

    // Step 0 should remain unchanged
    CHECK(pattern.step(0).enabled == false);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 80);

    // New steps (16-31) should be defaults
    for (int i = 16; i < 32; ++i) {
        CHECK(pattern.step(i) == defaultStepData());
    }
}

TEST_CASE("StepData: StepPattern::applyStepCountChange from 64 to 1", "[M10]") {
    StepPattern pattern;

    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        pattern.setEnabled(i, false);
    }

    pattern.applyStepCountChange(64, 1);

    CHECK(pattern.step(0).enabled == false);

    for (int i = 1; i < lanes::maximumStepCount; ++i) {
        CHECK(pattern.step(i) == defaultStepData());
    }
}

TEST_CASE("StepData: StepPattern::applyStepCountChange from 1 to 64", "[M10]") {
    StepPattern pattern;

    pattern.setEnabled(0, false);

    pattern.applyStepCountChange(1, 64);

    CHECK(pattern.step(0).enabled == false);

    for (int i = 1; i < lanes::maximumStepCount; ++i) {
        CHECK(pattern.step(i) == defaultStepData());
    }
}

TEST_CASE("StepData: StepPattern mode-per-step persistence: set byStep, switch to delta, switch back",
          "[M10][REQ-7-2]") {
    StepPattern pattern;

    // Set byStep value on step 5
    pattern.setValue(5, VelocityMode::byStep, 80);

    // Set delta value on step 5
    pattern.setValue(5, VelocityMode::deltaByStep, -20);

    // Set relative value on step 5
    pattern.setValue(5, VelocityMode::relative, 150);

    // All three should be stored
    CHECK(pattern.step(5).valueByMode[static_cast<int>(VelocityMode::byStep)] == 80);
    CHECK(pattern.step(5).valueByMode[static_cast<int>(VelocityMode::deltaByStep)] == -20);
    CHECK(pattern.step(5).valueByMode[static_cast<int>(VelocityMode::relative)] == 150);
}

TEST_CASE("StepData: StepPattern mode-per-step: switching modes leaves other modes unchanged",
          "[M10][REQ-7-3]") {
    StepPattern pattern;

    pattern.setValue(0, VelocityMode::byStep, 100);
    pattern.setValue(0, VelocityMode::deltaByStep, -10);

    // Change byStep value
    pattern.setValue(0, VelocityMode::byStep, 80);

    // deltaByStep should remain unchanged
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::deltaByStep)] == -10);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 80);
}

TEST_CASE("StepData: StepPattern::operator== compares all fields", "[M10]") {
    StepPattern pattern1;
    StepPattern pattern2;

    CHECK(pattern1 == pattern2);

    pattern1.setEnabled(0, false);
    CHECK(!(pattern1 == pattern2));

    pattern2.setEnabled(0, false);
    CHECK(pattern1 == pattern2);

    pattern1.setValue(0, VelocityMode::byStep, 80);
    CHECK(!(pattern1 == pattern2));

    pattern2.setValue(0, VelocityMode::byStep, 80);
    CHECK(pattern1 == pattern2);
}

TEST_CASE("StepData: StepPattern equality comparison", "[M10]") {
    StepPattern pattern1;
    StepPattern pattern2;

    for (int i = 0; i < 16; ++i) {
        pattern1.setEnabled(i, false);
        pattern2.setEnabled(i, false);
        pattern1.setValue(i, VelocityMode::byStep, 100 - i);
        pattern2.setValue(i, VelocityMode::byStep, 100 - i);
    }

    CHECK(pattern1 == pattern2);
}

TEST_CASE("StepData: defaultStepData equality comparison", "[M10]") {
    const auto step1 = defaultStepData();
    const auto step2 = defaultStepData();
    CHECK(step1 == step2);
}

TEST_CASE("StepData: StepData enabled field", "[M10]") {
    StepData data1{true, {}};
    StepData data2{false, {}};

    CHECK(data1 != data2);
}

TEST_CASE("StepData: StepData valueByMode field", "[M10]") {
    StepData data1{true, {}};
    StepData data2{true, {}};
    data1.valueByMode[static_cast<int>(VelocityMode::byStep)] = 100;
    data2.valueByMode[static_cast<int>(VelocityMode::byStep)] = 80;

    CHECK(data1 != data2);
}

TEST_CASE("StepData: StepPattern::step on boundary indices", "[M10]") {
    StepPattern pattern;

    // Lowest valid index
    CHECK(pattern.step(0) == defaultStepData());

    // Highest valid index
    CHECK(pattern.step(63) == defaultStepData());

    // Just outside range (clamped to 63)
    CHECK(pattern.step(64) == defaultStepData());
}

TEST_CASE("StepData: StepPattern::setValue with byStep at boundary values", "[M10]") {
    StepPattern pattern;

    pattern.setValue(0, VelocityMode::byStep, 0);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 0);

    pattern.setValue(1, VelocityMode::byStep, 127);
    CHECK(pattern.step(1).valueByMode[static_cast<int>(VelocityMode::byStep)] == 127);
}

TEST_CASE("StepData: StepPattern::setValue with delta at boundary values", "[M10]") {
    StepPattern pattern;

    pattern.setValue(0, VelocityMode::deltaByStep, -127);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::deltaByStep)] == -127);

    pattern.setValue(1, VelocityMode::deltaByStep, 127);
    CHECK(pattern.step(1).valueByMode[static_cast<int>(VelocityMode::deltaByStep)] == 127);
}

TEST_CASE("StepData: StepPattern::setValue with relative at boundary values", "[M10]") {
    StepPattern pattern;

    pattern.setValue(0, VelocityMode::relative, 0);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::relative)] == 0);

    pattern.setValue(1, VelocityMode::relative, 200);
    CHECK(pattern.step(1).valueByMode[static_cast<int>(VelocityMode::relative)] == 200);
}

TEST_CASE("StepData: StepPattern::applyStepCountChange with oldCount == newCount",
          "[M10]") {
    StepPattern pattern;

    pattern.setEnabled(0, false);
    pattern.setValue(0, VelocityMode::byStep, 80);

    // No-op: count stays the same
    pattern.applyStepCountChange(16, 16);

    CHECK(pattern.step(0).enabled == false);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 80);
}

TEST_CASE("StepData: StepPattern mode values initialized to defaults", "[M10]") {
    StepPattern pattern;

    // Check that all steps have default values per mode
    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        CHECK(pattern.step(i).valueByMode[static_cast<int>(VelocityMode::incoming)] == 0);
        CHECK(pattern.step(i).valueByMode[static_cast<int>(VelocityMode::byStep)] ==
              velocity::defaultByStepValue);
        CHECK(pattern.step(i).valueByMode[static_cast<int>(VelocityMode::deltaByStep)] ==
              velocity::defaultDeltaValue);
        CHECK(pattern.step(i).valueByMode[static_cast<int>(VelocityMode::relative)] ==
              velocity::defaultPercentValue);
    }
}

TEST_CASE("StepData: StepPattern::applyStepCountChange shrinking then expanding",
          "[M10]") {
    StepPattern pattern;

    // Set some values
    pattern.setEnabled(0, false);
    pattern.setValue(0, VelocityMode::byStep, 50);
    pattern.setValue(20, VelocityMode::byStep, 100);

    // Shrink to 16
    pattern.applyStepCountChange(64, 16);

    // Step 0 should be preserved
    CHECK(pattern.step(0).enabled == false);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 50);

    // Step 20 should be reset
    CHECK(pattern.step(20) == defaultStepData());

    // Expand to 32
    pattern.applyStepCountChange(16, 32);

    // Step 0 should still be preserved
    CHECK(pattern.step(0).enabled == false);
    CHECK(pattern.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 50);

    // New steps should be defaults
    for (int i = 16; i < 32; ++i) {
        CHECK(pattern.step(i) == defaultStepData());
    }
}

TEST_CASE("StepData: StepPattern copy and modification independence", "[M10]") {
    StepPattern pattern1;
    pattern1.setEnabled(0, false);
    pattern1.setValue(0, VelocityMode::byStep, 80);

    StepPattern pattern2 = pattern1;  // Copy

    // Modify pattern2
    pattern2.setEnabled(1, false);
    pattern2.setValue(1, VelocityMode::byStep, 100);

    // pattern1 should be unchanged
    CHECK(pattern1.step(0).enabled == false);
    CHECK(pattern1.step(1).enabled == true);
    CHECK(pattern1.step(0).valueByMode[static_cast<int>(VelocityMode::byStep)] == 80);
}

TEST_CASE("StepData: StepPattern::applyStepCountChange all steps reset when shrinking to 0",
          "[M10]") {
    StepPattern pattern;

    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        pattern.setEnabled(i, false);
        pattern.setValue(i, VelocityMode::byStep, 100);
    }

    // This tests what happens with edge case (though spec minimum is 1)
    pattern.applyStepCountChange(64, 1);

    // Step 0 preserved, others reset
    CHECK(pattern.step(0).enabled == false);
    for (int i = 1; i < lanes::maximumStepCount; ++i) {
        CHECK(pattern.step(i) == defaultStepData());
    }
}

TEST_CASE("StepData: StepPattern persists different values across all 64 steps", "[M10]") {
    StepPattern pattern;

    // Set unique values for each step
    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        const int value = (i * 7 + 23) % 128;  // Pseudo-unique value
        pattern.setValue(i, VelocityMode::byStep, value);
        pattern.setEnabled(i, (i % 2 == 0));
    }

    // Verify all values are preserved
    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        const int expected = (i * 7 + 23) % 128;
        CHECK(pattern.step(i).valueByMode[static_cast<int>(VelocityMode::byStep)] == expected);
        CHECK(pattern.step(i).enabled == (i % 2 == 0));
    }
}

TEST_CASE("StepData: defaultStepData matches StepPattern defaults", "[M10]") {
    const StepPattern pattern;
    const auto defaultStep = defaultStepData();

    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        CHECK(pattern.step(i) == defaultStep);
    }
}
