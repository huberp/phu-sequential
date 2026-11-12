#include <catch2/catch_test_macros.hpp>
#include "sequencing/velocity/VelocityModeRegistry.h"
#include "sequencing/Constants.h"

using namespace phu::sequencing;
using namespace phu::sequencing::constants;

// M09 Velocity modes and strategies (§3.9, §7, #8)

TEST_CASE("VelocityModes: velocityModeRegistry has exactly velocityModeCount entries",
          "[M09][REQ-16.4-1]") {
    CHECK(velocityModeRegistry.size() == velocityModeCount);
}

TEST_CASE("VelocityModes: registry entries have names, persistence keys, and compute functions",
          "[M09]") {
    for (const auto& definition : velocityModeRegistry) {
        CHECK(static_cast<std::string>(definition.name) != "");
        CHECK(static_cast<std::string>(definition.persistenceKey) != "");
        CHECK(definition.compute != nullptr);
    }
}

TEST_CASE("VelocityModes: incoming mode at index 0 with correct properties",
          "[M09][REQ-7.1-1]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::incoming)];
    CHECK(static_cast<std::string>(def.name) == "Incoming");
    CHECK(static_cast<std::string>(def.persistenceKey) == "incoming");
    CHECK(def.usesStepValues == false);
    CHECK(def.stepValueRange.minimum == 0);
    CHECK(def.stepValueRange.maximum == 0);
    CHECK(def.stepValueRange.defaultValue == 0);
}

TEST_CASE("VelocityModes: byStep mode at index 1 with correct properties", "[M09][REQ-7.1-2]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::byStep)];
    CHECK(static_cast<std::string>(def.name) == "By Step");
    CHECK(static_cast<std::string>(def.persistenceKey) == "byStep");
    CHECK(def.usesStepValues == true);
    CHECK(def.stepValueRange.minimum == midi::lowestVelocity);
    CHECK(def.stepValueRange.maximum == midi::highestVelocity);
    CHECK(def.stepValueRange.defaultValue == velocity::defaultByStepValue);
}

TEST_CASE("VelocityModes: deltaByStep mode at index 2 with correct properties",
          "[M09][REQ-7.1-3]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::deltaByStep)];
    CHECK(static_cast<std::string>(def.name) == "Delta by Step");
    CHECK(static_cast<std::string>(def.persistenceKey) == "deltaByStep");
    CHECK(def.usesStepValues == true);
    CHECK(def.stepValueRange.minimum == velocity::minimumDelta);
    CHECK(def.stepValueRange.maximum == velocity::maximumDelta);
    CHECK(def.stepValueRange.defaultValue == velocity::defaultDeltaValue);
}

TEST_CASE("VelocityModes: relative mode at index 3 with correct properties", "[M09][REQ-7.1-4]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::relative)];
    CHECK(static_cast<std::string>(def.name) == "Relative");
    CHECK(static_cast<std::string>(def.persistenceKey) == "relative");
    CHECK(def.usesStepValues == true);
    CHECK(def.stepValueRange.minimum == velocity::minimumPercent);
    CHECK(def.stepValueRange.maximum == velocity::maximumPercent);
    CHECK(def.stepValueRange.defaultValue == velocity::defaultPercentValue);
}

TEST_CASE("VelocityModes: registry persistence keys are unique", "[M09]") {
    for (size_t i = 0; i < velocityModeRegistry.size(); ++i) {
        for (size_t j = i + 1; j < velocityModeRegistry.size(); ++j) {
            CHECK(static_cast<std::string>(velocityModeRegistry[i].persistenceKey) !=
                  static_cast<std::string>(velocityModeRegistry[j].persistenceKey));
        }
    }
}

TEST_CASE("VelocityModes: velocityModeDefinition returns correct definition for each mode",
          "[M09]") {
    for (int i = 0; i < velocityModeCount; ++i) {
        const auto mode = static_cast<VelocityMode>(i);
        const auto& def = velocityModeDefinition(mode);
        CHECK(def.compute != nullptr);
    }
}

TEST_CASE("VelocityModes: velocityModeDefinition for out-of-range mode returns incoming",
          "[M09][REQ-17]") {
    const auto& def = velocityModeDefinition(static_cast<VelocityMode>(255));
    CHECK(static_cast<std::string>(def.name) == "Incoming");
}

TEST_CASE("VelocityModes: incoming mode ignores step value", "[M09][REQ-7.1-1]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::incoming)];
    const int result = def.compute(0, 90);
    CHECK(result == 90);

    const int result2 = def.compute(127, 90);
    CHECK(result2 == 90);
}

TEST_CASE("VelocityModes: byStep mode returns step value as is", "[M09][REQ-7.1-2]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::byStep)];
    const int result = def.compute(100, 20);
    CHECK(result == 100);
}

TEST_CASE("VelocityModes: delta mode adds step value to input velocity", "[M09][REQ-7.1-3]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::deltaByStep)];
    const int result = def.compute(-30, 100);
    CHECK(result == 70);
}

TEST_CASE("VelocityModes: relative mode multiplies input by percentage with rounding",
          "[M09][REQ-7.1-4]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::relative)];
    // (100 * 150 + 50) / 100 = 150.5 -> 150
    const int result150 = def.compute(150, 100);
    CHECK(result150 == 150);

    // (100 * 50 + 50) / 100 = 50.5 -> 50
    const int result50 = def.compute(50, 100);
    CHECK(result50 == 50);

    // (100 * 100 + 50) / 100 = 100.5 -> 100
    const int result100 = def.compute(100, 100);
    CHECK(result100 == 100);
}

TEST_CASE("VelocityModes: clampStepValue clamps to mode's minimum", "[M09]") {
    for (int i = 0; i < velocityModeCount; ++i) {
        const auto mode = static_cast<VelocityMode>(i);
        const auto& def = velocityModeDefinition(mode);
        const auto minVal = def.stepValueRange.minimum;
        const int clamped = clampStepValue(mode, minVal - 10);
        CHECK(clamped == minVal);
    }
}

TEST_CASE("VelocityModes: clampStepValue clamps to mode's maximum", "[M09]") {
    for (int i = 0; i < velocityModeCount; ++i) {
        const auto mode = static_cast<VelocityMode>(i);
        const auto& def = velocityModeDefinition(mode);
        const auto maxVal = def.stepValueRange.maximum;
        const int clamped = clampStepValue(mode, maxVal + 10);
        CHECK(clamped == maxVal);
    }
}

TEST_CASE("VelocityModes: clampStepValue passes through valid values", "[M09]") {
    for (int i = 0; i < velocityModeCount; ++i) {
        const auto mode = static_cast<VelocityMode>(i);
        const auto& def = velocityModeDefinition(mode);
        const auto mid = (def.stepValueRange.minimum + def.stepValueRange.maximum) / 2;
        const int clamped = clampStepValue(mode, mid);
        CHECK(clamped == mid);
    }
}

TEST_CASE("VelocityModes: clampStepValue for out-of-range mode uses incoming ranges", "[M09]") {
    const auto clamped = clampStepValue(static_cast<VelocityMode>(255), 50);
    CHECK(clamped == 0);  // Incoming range is {0, 0, 0}, so clamping 50 -> 0
}

TEST_CASE("VelocityModes: computeOutputVelocity for incoming mode returns input clamped",
          "[M09][REQ-7.1-1]") {
    const auto result = computeOutputVelocity(VelocityMode::incoming, 0, 90);
    REQUIRE(result.has_value());
    CHECK(result.value() == 90);
}

TEST_CASE("VelocityModes: computeOutputVelocity for byStep returns step value clamped",
          "[M09][REQ-7.1-2]") {
    const auto result = computeOutputVelocity(VelocityMode::byStep, 100, 20);
    REQUIRE(result.has_value());
    CHECK(result.value() == 100);
}

TEST_CASE("VelocityModes: computeOutputVelocity for delta adds and clamps", "[M09][REQ-7.1-3]") {
    const auto result = computeOutputVelocity(VelocityMode::deltaByStep, -30, 100);
    REQUIRE(result.has_value());
    CHECK(result.value() == 70);
}

TEST_CASE("VelocityModes: computeOutputVelocity for relative multiplies and clamps",
          "[M09][REQ-7.1-4]") {
    const auto result150 = computeOutputVelocity(VelocityMode::relative, 150, 100);
    REQUIRE(result150.has_value());
    CHECK(result150.value() == 127);  // Clamp to 127

    const auto result50 = computeOutputVelocity(VelocityMode::relative, 50, 100);
    REQUIRE(result50.has_value());
    CHECK(result50.value() == 50);
}

TEST_CASE("VelocityModes: computeOutputVelocity clamps high to 127", "[M09][REQ-7.2-1]") {
    const auto result = computeOutputVelocity(VelocityMode::deltaByStep, 50, 120);
    REQUIRE(result.has_value());
    CHECK(result.value() == 127);
}

TEST_CASE("VelocityModes: computeOutputVelocity clamps low to 0 and is not sent", "[M09][REQ-7.2-2]") {
    const auto result = computeOutputVelocity(VelocityMode::deltaByStep, -50, 20);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("VelocityModes: computeOutputVelocity returns nullopt for zero velocity",
          "[M09][REQ-7.2-3]") {
    // byStep 0
    const auto result1 = computeOutputVelocity(VelocityMode::byStep, 0, 100);
    CHECK(result1.has_value() == false);

    // delta that results in 0
    const auto result2 = computeOutputVelocity(VelocityMode::deltaByStep, -20, 20);
    CHECK(result2.has_value() == false);

    // relative that results in 0
    const auto result3 = computeOutputVelocity(VelocityMode::relative, 0, 100);
    CHECK(result3.has_value() == false);
}

TEST_CASE("VelocityModes: byStep with input 20 and step 100 gives 100", "[M09][REQ-7.1-2]") {
    const auto result = computeOutputVelocity(VelocityMode::byStep, 100, 20);
    REQUIRE(result.has_value());
    CHECK(result.value() == 100);
}

TEST_CASE("VelocityModes: delta with input 100 and delta -30 gives 70", "[M09][REQ-7.1-3]") {
    const auto result = computeOutputVelocity(VelocityMode::deltaByStep, -30, 100);
    REQUIRE(result.has_value());
    CHECK(result.value() == 70);
}

TEST_CASE("VelocityModes: relative with input 100 and 150% clamped to 127", "[M09][REQ-7.1-4]") {
    const auto result = computeOutputVelocity(VelocityMode::relative, 150, 100);
    REQUIRE(result.has_value());
    CHECK(result.value() == 127);
}

TEST_CASE("VelocityModes: relative with input 100 and 50% gives 50", "[M09][REQ-7.1-4]") {
    const auto result = computeOutputVelocity(VelocityMode::relative, 50, 100);
    REQUIRE(result.has_value());
    CHECK(result.value() == 50);
}

TEST_CASE("VelocityModes: custom strategy via function pointer in locally built definition",
          "[M09][REQ-16.4-1]") {
    // Demonstrate that a strategy can be composed at runtime without modifying registry
    static constexpr auto customCompute = [](int stepValue, int inputVelocity) noexcept -> int {
        // Custom formula: always return 99
        (void)stepValue;
        (void)inputVelocity;
        return 99;
    };

    // Build a test definition locally
    VelocityModeDefinition testDef{
        "Test Strategy",
        "testStrategy",
        true,
        {0, 200, 100},
        customCompute
    };

    // Use it
    const int result = testDef.compute(50, 60);
    CHECK(result == 99);

    // Clamping/nullopt handling still works
    const auto outputResult = computeOutputVelocity(VelocityMode::byStep, 99, 100);
    // This uses the registered byStep, not our test def, just proves the pattern works
    REQUIRE(outputResult.has_value());
}

TEST_CASE("VelocityModes: incoming mode usesStepValues is false", "[M09]") {
    const auto& def = velocityModeDefinition(VelocityMode::incoming);
    CHECK(def.usesStepValues == false);
}

TEST_CASE("VelocityModes: all non-incoming modes usesStepValues is true", "[M09]") {
    for (int i = 1; i < velocityModeCount; ++i) {
        const auto& def = velocityModeDefinition(static_cast<VelocityMode>(i));
        CHECK(def.usesStepValues == true);
    }
}

TEST_CASE("VelocityModes: registry order matches VelocityMode enum order", "[M09]") {
    CHECK(static_cast<std::string>(velocityModeRegistry[0].persistenceKey) ==
          static_cast<std::string>(velocityModeRegistry[static_cast<int>(VelocityMode::incoming)].persistenceKey));
    CHECK(static_cast<std::string>(velocityModeRegistry[1].persistenceKey) ==
          static_cast<std::string>(velocityModeRegistry[static_cast<int>(VelocityMode::byStep)].persistenceKey));
    CHECK(static_cast<std::string>(velocityModeRegistry[2].persistenceKey) ==
          static_cast<std::string>(velocityModeRegistry[static_cast<int>(VelocityMode::deltaByStep)].persistenceKey));
    CHECK(static_cast<std::string>(velocityModeRegistry[3].persistenceKey) ==
          static_cast<std::string>(velocityModeRegistry[static_cast<int>(VelocityMode::relative)].persistenceKey));
}

TEST_CASE("VelocityModes: byStep mode has default 100", "[M09]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::byStep)];
    CHECK(def.stepValueRange.defaultValue == 100);
}

TEST_CASE("VelocityModes: delta mode has default 0", "[M09]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::deltaByStep)];
    CHECK(def.stepValueRange.defaultValue == 0);
}

TEST_CASE("VelocityModes: relative mode has default 100", "[M09]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::relative)];
    CHECK(def.stepValueRange.defaultValue == 100);
}

TEST_CASE("VelocityModes: relative rounding with various inputs", "[M09]") {
    // Test rounding formula: (input * percent + 50) / 100
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::relative)];

    // 100 * 101 + 50 = 10150, / 100 = 101.5 -> 101
    CHECK(def.compute(101, 100) == 101);

    // 100 * 99 + 50 = 9950, / 100 = 99.5 -> 99
    CHECK(def.compute(99, 100) == 99);

    // 127 * 100 + 50 = 12750, / 100 = 127.5 -> 127 (before clamp, but clamping will apply)
    // Result from compute is unclamped, so 127 stays
    CHECK(def.compute(100, 127) == 127);
}

TEST_CASE("VelocityModes: computeOutputVelocity edge case: 1 as minimum non-zero",
          "[M09][REQ-7.2-3]") {
    // Incoming with velocity 1
    const auto result = computeOutputVelocity(VelocityMode::incoming, 0, 1);
    REQUIRE(result.has_value());
    CHECK(result.value() == 1);

    // byStep with value 1
    const auto result2 = computeOutputVelocity(VelocityMode::byStep, 1, 50);
    REQUIRE(result2.has_value());
    CHECK(result2.value() == 1);
}

TEST_CASE("VelocityModes: byStep mode name is 'By Step'", "[M09]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::byStep)];
    CHECK(static_cast<std::string>(def.name) == "By Step");
}

TEST_CASE("VelocityModes: deltaByStep mode name is 'Delta by Step'", "[M09]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::deltaByStep)];
    CHECK(static_cast<std::string>(def.name) == "Delta by Step");
}

TEST_CASE("VelocityModes: relative mode name is 'Relative'", "[M09]") {
    const auto& def = velocityModeRegistry[static_cast<int>(VelocityMode::relative)];
    CHECK(static_cast<std::string>(def.name) == "Relative");
}
