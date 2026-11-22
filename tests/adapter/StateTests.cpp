#include <catch2/catch_test_macros.hpp>

#include "LaneEditHistory.h"
#include "ParameterLayout.h"
#include "PatternStore.h"
#include "PresetManager.h"
#include "StateMigration.h"
#include "StateSerializer.h"
#include "editor/LaneControlStrip.h"
#include "sequencing/Constants.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <cstdint>
#include <string>

// M22 adapter: state serialization, migration, presets (sec. 13, #14).

using namespace phu::plugin;
using namespace phu::arpeggiator;
using namespace phu::sequencing;
using namespace phu::sequencing::constants;

namespace {

struct DummyProcessor : juce::AudioProcessor {
    DummyProcessor() : juce::AudioProcessor(BusesProperties()) {}
    const juce::String getName() const override { return "Dummy"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

PluginStateData makeFilledState() {
    PluginStateData state;
    state.parameters.global = {true, false, 7};
    for (int i = 0; i < lanes::laneCount; ++i) {
        auto& lane = state.parameters.lanes[static_cast<std::size_t>(i)];
        lane.mute = (i % 2) == 0;
        lane.latch = (i % 3) == 0;
        lane.inputMode = i % 2 == 0 ? InputMode::channel : InputMode::chordFollow;
        lane.followIndex = i;
        lane.followChannel = 2 + i;
        lane.outputChannel = 16 - i;
        lane.transpose = i - 4;
        lane.gatePercent = 50 + i;
        lane.timingSource = i % 2 == 0 ? TimingSource::textGrid : TimingSource::baseTiming;
        lane.baseTimingIndex = i % 6;
        lane.subdivision = static_cast<Subdivision>(i % 3);
        lane.swingPercent = 10 * i;
        lane.stepCount = 5 + i;
        lane.velocityMode = static_cast<VelocityMode>(i % 4);

        state.document.setLaneColor(i, LaneColor{static_cast<std::uint8_t>(10 + i), static_cast<std::uint8_t>(20 + i),
                                                  static_cast<std::uint8_t>(30 + i)});
        state.document.setTextGridText(i, "1/8t, 1/8., 1/4");
        state.document.setTextGridText(i, "1/8,,");  // invalid: text kept, applied grid stays
        state.document.setStepEnabled(i, 0, false);
        state.document.setStepValue(i, 1, VelocityMode::byStep, 77 + i);
        state.document.setStepValue(i, 2, VelocityMode::deltaByStep, -20 - i);
        state.document.setStepValue(i, 3, VelocityMode::relative, 150 + i);
        state.document.enforceStepCountInvariant(i, lane);
    }
    return state;
}

juce::ValueTree currentTree() {
    return toValueTree(PluginStateData{}, StateKind::state);
}

juce::ValueTree& laneTree(juce::ValueTree& root, int lane1) {
    static juce::ValueTree holder;
    for (auto child : root) {
        if (child.hasType("Lane") && static_cast<int>(child.getProperty("index")) == lane1) {
            holder = child;
            return holder;
        }
    }
    FAIL("lane not found");
    return holder;
}

juce::File makeTempFolder() {
    return juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("phu-state-tests-" + juce::String::toHexString(juce::Random::getSystemRandom().nextInt64()));
}

}  // namespace

TEST_CASE("State: round trip is lossless", "[M22][REQ-13-1][REQ-8-2][REQ-4-3]") {
    const auto state = makeFilledState();

    const auto viaTree = fromValueTree(toValueTree(state, StateKind::state));
    CHECK(viaTree.outcome == LoadOutcome::loaded);
    CHECK(viaTree.data.parameters == state.parameters);
    CHECK(viaTree.data.document == state.document);

    const auto viaXml = fromXmlText(toXmlText(state, StateKind::state));
    CHECK(viaXml.outcome == LoadOutcome::loaded);
    CHECK(viaXml.data.parameters == state.parameters);
    CHECK(viaXml.data.document == state.document);
    CHECK(viaXml.data.document.lane(3).textGridText == "1/8,,");

    juce::MemoryBlock block;
    writeStateBlock(state, block);
    const auto viaBlock = readStateBlock(block.getData(), static_cast<int>(block.getSize()));
    CHECK(viaBlock.outcome == LoadOutcome::loaded);
    CHECK(viaBlock.data.document == state.document);
}

TEST_CASE("State: format version is present and lane color is not a host parameter", "[M22][REQ-13-5][REQ-4-4]") {
    const auto tree = toValueTree(PluginStateData{}, StateKind::preset, "Name");
    CHECK(static_cast<int>(tree.getProperty("formatVersion")) == state::currentFormatVersion);
    CHECK(tree.getProperty("kind").toString() == "preset");
    const auto presetLabel = tree.getChildWithName("PresetInfo").getProperty("name").toString();
    CHECK(presetLabel == "Name");

    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    for (auto* parameter : processor.getParameters()) {
        CHECK_FALSE(static_cast<juce::AudioProcessorParameterWithID*>(parameter)->getParameterID().containsIgnoreCase("color"));
    }
    auto root = currentTree();
    CHECK(laneTree(root, 1).hasProperty("color"));
}

TEST_CASE("State: unknown fields are ignored", "[M22][REQ-13-7]") {
    auto root = currentTree();
    root.setProperty("futureRootField", "x", nullptr);
    root.appendChild(juce::ValueTree("FutureChild"), nullptr);
    laneTree(root, 2).setProperty("futureField", 5, nullptr);

    const auto result = fromValueTree(root);
    CHECK(result.outcome == LoadOutcome::loaded);
    CHECK(result.data.parameters == defaultSequencerConfiguration());
}

TEST_CASE("State: invalid values are clamped", "[M22][REQ-13-8][REQ-4-7]") {
    auto root = currentTree();
    laneTree(root, 1).setProperty("gate", 999, nullptr);
    laneTree(root, 1).setProperty("outputChannel", 0, nullptr);
    laneTree(root, 1).setProperty("color", "#12", nullptr);
    laneTree(root, 2).setProperty("velocityMode", "bogus", nullptr);
    laneTree(root, 2).setProperty("transpose", "abc", nullptr);
    laneTree(root, 3).getChildWithName("Steps").setProperty("byStep", "500,x,-5", nullptr);

    const auto result = fromValueTree(root);
    CHECK(result.outcome == LoadOutcome::loadedWithClamping);
    CHECK(result.data.parameters.lanes[0].gatePercent == lanes::maximumGatePercent);
    CHECK(result.data.parameters.lanes[0].outputChannel == midi::firstChannel);
    CHECK(result.data.document.lane(0).color == defaultLaneColor(0));
    CHECK(result.data.parameters.lanes[1].velocityMode == VelocityMode::incoming);
    CHECK(result.data.parameters.lanes[1].transpose == lanes::defaultTranspose);
    const auto& steps = result.data.document.lane(2).pattern.steps;
    CHECK(steps.step(0).valueByMode[static_cast<std::size_t>(VelocityMode::byStep)] == midi::highestVelocity);
    CHECK(steps.step(1).valueByMode[static_cast<std::size_t>(VelocityMode::byStep)] == velocity::defaultByStepValue);
    CHECK(steps.step(2).valueByMode[static_cast<std::size_t>(VelocityMode::byStep)] == 0);
}

TEST_CASE("State: missing fields and old state without colors fall back to defaults", "[M22][REQ-4-7]") {
    auto root = currentTree();
    for (int i = 1; i <= lanes::laneCount; ++i) {
        laneTree(root, i).removeProperty("color", nullptr);
        laneTree(root, i).removeProperty("gate", nullptr);
    }
    const auto result = fromValueTree(root);
    CHECK(result.outcome == LoadOutcome::loaded);
    CHECK(result.data.document == PatternDocument{});
    CHECK(result.data.parameters == defaultSequencerConfiguration());
}

TEST_CASE("State: older version is migrated with a synthetic chain", "[M22][REQ-13-6]") {
    constexpr MigrationFunction chain[] = {
        [](juce::ValueTree tree) {
            // Synthetic v1 -> v2: "legacyGate" became "gate".
            for (auto child : tree) {
                if (child.hasType("Lane") && child.hasProperty("legacyGate")) {
                    child.setProperty("gate", child.getProperty("legacyGate"), nullptr);
                    child.removeProperty("legacyGate", nullptr);
                }
            }
            tree.setProperty("formatVersion", 2, nullptr);
            return tree;
        }};

    auto root = currentTree();
    root.setProperty("formatVersion", 1, nullptr);
    laneTree(root, 1).removeProperty("gate", nullptr);
    laneTree(root, 1).setProperty("legacyGate", 120, nullptr);

    const auto result = fromValueTree(root, chain, 2);
    CHECK(result.outcome == LoadOutcome::migrated);
    CHECK(result.data.parameters.lanes[0].gatePercent == 120);

    CHECK(fromValueTree(root, {}, 2).outcome == LoadOutcome::rejectedCorrupt);
}

TEST_CASE("State: newer version falls back to the default state", "[M22][REQ-13-9]") {
    auto root = toValueTree(makeFilledState(), StateKind::state);
    root.setProperty("formatVersion", state::currentFormatVersion + 1, nullptr);
    const auto result = fromValueTree(root);
    CHECK(result.outcome == LoadOutcome::rejectedNewerVersion);
    CHECK(result.data.parameters == defaultSequencerConfiguration());
    CHECK(result.data.document == PatternDocument{});
}

TEST_CASE("State: corrupt or empty data falls back to the default state", "[M22][REQ-13-10]") {
    const char garbage[] = "\x01\x02garbage";
    CHECK(readStateBlock(garbage, sizeof(garbage)).outcome == LoadOutcome::rejectedCorrupt);
    CHECK(readStateBlock(nullptr, 0).outcome == LoadOutcome::rejectedCorrupt);
    CHECK(fromXmlText("<not closed").outcome == LoadOutcome::rejectedCorrupt);
    CHECK(fromXmlText("<Other formatVersion=\"1\"/>").outcome == LoadOutcome::rejectedCorrupt);
    CHECK(fromXmlText("<PhuSequential/>").outcome == LoadOutcome::rejectedCorrupt);
    CHECK(fromXmlText("<PhuSequential formatVersion=\"abc\"/>").outcome == LoadOutcome::rejectedCorrupt);
    CHECK(fromXmlText("<PhuSequential formatVersion=\"0\"/>").outcome == LoadOutcome::rejectedCorrupt);
}

TEST_CASE("State: step data beyond the effective step count is reset on load", "[M22][REQ-5.4-1]") {
    auto root = currentTree();
    auto steps = laneTree(root, 1).getChildWithName("Steps");
    juce::StringArray on;
    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        on.add("0");
    }
    steps.setProperty("on", on.joinIntoString(","), nullptr);

    const auto result = fromValueTree(root);
    const auto& pattern = result.data.document.lane(0).pattern.steps;
    CHECK_FALSE(pattern.step(0).enabled);
    CHECK(pattern.step(lanes::defaultStepCount).enabled);
}

TEST_CASE("State: capture and apply round trip through the parameter tree", "[M22][REQ-13-1]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    PatternStore store;

    const auto state = makeFilledState();
    applyState(state, apvts, store);
    const auto captured = captureState(apvts, store);
    CHECK(captured.parameters == state.parameters);
    CHECK(captured.document == state.document);

    applyState(fromValueTree(juce::ValueTree{}).data, apvts, store);
    CHECK(captureState(apvts, store).parameters == defaultSequencerConfiguration());
}

TEST_CASE("Presets: user folder list, save, load and navigate", "[M22][REQ-13-2][REQ-13-4]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    PatternStore store;
    const auto folder = makeTempFolder();
    PresetManager manager(apvts, store, folder);

    CHECK(manager.listPresets().empty());
    CHECK(manager.currentPresetName().isEmpty());

    const auto state = makeFilledState();
    applyState(state, apvts, store);
    REQUIRE(manager.saveUserPreset("Beta/Preset"));
    REQUIRE(manager.saveUserPreset("alpha"));
    CHECK(manager.currentPresetName() == "alpha");

    const auto presets = manager.listPresets();
    REQUIRE(presets.size() == 2);
    const juce::String firstLabel = presets[0].name;
    const juce::String secondLabel = presets[1].name;
    CHECK(firstLabel == "alpha");
    CHECK(folder.getNumberOfChildFiles(juce::File::findFiles, "*.phuseq") == 2);

    applyState(PluginStateData{}, apvts, store);
    CHECK(manager.loadPreset(presets[0]) == LoadOutcome::loaded);
    const auto loaded = captureState(apvts, store);
    CHECK(loaded.parameters == state.parameters);
    CHECK(loaded.parameters.global.thru);
    CHECK(loaded.parameters.global.chordInputChannel == 7);
    CHECK(loaded.document == state.document);

    CHECK(manager.loadNext() == LoadOutcome::loaded);
    CHECK(manager.currentPresetName() == secondLabel);
    CHECK(manager.loadNext() == LoadOutcome::loaded);
    CHECK(manager.currentPresetName() == firstLabel);
    CHECK(manager.loadPrevious() == LoadOutcome::loaded);
    CHECK(manager.currentPresetName() == secondLabel);

    CHECK_FALSE(manager.saveUserPreset("   "));
    folder.deleteRecursively();
}

TEST_CASE("Presets: a newer-version preset file applies the default state", "[M22][REQ-13-9]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    PatternStore store;
    const auto folder = makeTempFolder();
    REQUIRE(folder.createDirectory().wasOk());
    PresetManager manager(apvts, store, folder);

    auto root = toValueTree(makeFilledState(), StateKind::preset, "future");
    root.setProperty("formatVersion", state::currentFormatVersion + 1, nullptr);
    REQUIRE(folder.getChildFile("future.phuseq").replaceWithText(root.toXmlString()));
    applyState(makeFilledState(), apvts, store);

    const auto presets = manager.listPresets();
    REQUIRE(presets.size() == 1);
    CHECK(manager.loadPreset(presets[0]) == LoadOutcome::rejectedNewerVersion);
    CHECK(captureState(apvts, store).parameters == defaultSequencerConfiguration());
    folder.deleteRecursively();
}

TEST_CASE("LaneState: per-lane ValueTree round trip incl. all 8 lanes", "[M26][M22][REQ-22-2]") {
    const auto state = makeFilledState();
    auto root = toValueTree(state, StateKind::state);

    int found = 0;
    for (int i = 0; i < lanes::laneCount; ++i) {
        const LaneState lane{state.parameters.lanes[static_cast<std::size_t>(i)], state.document.lane(i)};
        const auto tree = laneToValueTree(lane, i);
        const auto loaded = laneFromValueTree(tree, i);
        CHECK(loaded.valid);
        CHECK_FALSE(loaded.clamped);
        CHECK(loaded.state == lane);
        // Same serialization as the child inside the plugin state.
        CHECK(tree.isEquivalentTo(laneTree(root, i + 1)));
        ++found;
    }
    CHECK(found == lanes::laneCount);
    CHECK_FALSE(laneFromValueTree(juce::ValueTree("Other"), 3).valid);
    CHECK(laneFromValueTree(juce::ValueTree{}, 3).state == defaultLaneState(3));
}

TEST_CASE("LaneState: lane tree loading ignores unknown and clamps invalid fields", "[M26][M22][REQ-22-3]") {
    auto tree = laneToValueTree(defaultLaneState(1), 1);
    tree.setProperty("futureField", 1, nullptr);
    tree.appendChild(juce::ValueTree("FutureChild"), nullptr);
    CHECK(laneFromValueTree(tree, 1).state == defaultLaneState(1));
    CHECK_FALSE(laneFromValueTree(tree, 1).clamped);

    tree.setProperty("gate", 999, nullptr);
    const auto loaded = laneFromValueTree(tree, 1);
    CHECK(loaded.clamped);
    CHECK(loaded.state.configuration.gatePercent == lanes::maximumGatePercent);
}

TEST_CASE("LaneState: older lane is migrated, newer version falls back to default", "[M26][M22][REQ-22-3]") {
    constexpr MigrationFunction chain[] = {[](juce::ValueTree tree) {
        for (auto child : tree) {
            if (child.hasType("Lane") && child.hasProperty("legacyGate")) {
                child.setProperty("gate", child.getProperty("legacyGate"), nullptr);
                child.removeProperty("legacyGate", nullptr);
            }
        }
        tree.setProperty("formatVersion", 2, nullptr);
        return tree;
    }};

    auto lane = laneToValueTree(defaultLaneState(0), 0);
    lane.removeProperty("gate", nullptr);
    lane.setProperty("legacyGate", 120, nullptr);
    juce::ValueTree root("PhuSequential");
    root.setProperty("formatVersion", 1, nullptr);
    root.appendChild(lane, nullptr);

    const auto migrated = fromValueTree(root, chain, 2);
    CHECK(migrated.outcome == LoadOutcome::migrated);
    CHECK(migrated.data.parameters.lanes[0].gatePercent == 120);

    root.setProperty("formatVersion", state::currentFormatVersion + 1, nullptr);
    const auto newer = fromValueTree(root);
    CHECK(newer.outcome == LoadOutcome::rejectedNewerVersion);
    CHECK(newer.data.parameters == defaultSequencerConfiguration());
}

TEST_CASE("LaneState: apply writes parameters through the parameter tree and the document", "[M26][REQ-22-2]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    PatternStore store;

    auto lane = defaultLaneState(2);
    lane.configuration.transpose = 7;
    lane.configuration.mute = true;
    lane.document.color = LaneColor{9, 8, 7};
    lane.document.pattern.steps.setEnabled(1, false);

    applyLaneState(lane, 2, apvts, store);
    CHECK(captureLaneState(apvts, store, 2) == lane);
    CHECK(captureLaneState(apvts, store, 0) == defaultLaneState(0));
}

TEST_CASE("LaneEditHistory: commands run through APVTS and store, undo/redo and clear", "[M27][REQ-24-3][REQ-24-4]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    PatternStore store;
    LaneEditHistory history(apvts, store);
    const auto before = captureLaneState(apvts, store, 1);

    const phu::arpeggiator::CommandArguments args{{5}, {}};
    CHECK(history.execute("SetTranspose", 1, args));
    CHECK_FALSE(history.execute("NoSuchCommand", 1));
    CHECK(captureLaneState(apvts, store, 1).configuration.transpose == 5);
    CHECK(history.canUndo());

    history.undo();
    CHECK(captureLaneState(apvts, store, 1) == before);
    history.redo();
    CHECK(captureLaneState(apvts, store, 1).configuration.transpose == 5);

    history.clear();
    CHECK_FALSE(history.canUndo());
    CHECK_FALSE(history.canRedo());
}

TEST_CASE("LaneEditHistory: coalesced edits collapse into one undo step", "[M27][REQ-24-3]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    PatternStore store;
    LaneEditHistory history(apvts, store);
    const auto before = captureLaneState(apvts, store, 0);

    for (int text = 0; text < 3; ++text) {
        history.executeCoalesced("color", [text](phu::arpeggiator::LaneStateAccess& access) {
            return std::make_unique<phu::arpeggiator::SetLaneColorCommand>(access, 0, phu::arpeggiator::LaneColor{static_cast<std::uint8_t>(text), 1, 2});
        });
    }
    history.endGesture();
    CHECK(captureLaneState(apvts, store, 0).document.color == phu::arpeggiator::LaneColor{2, 1, 2});
    history.undo();
    CHECK(captureLaneState(apvts, store, 0) == before);
    CHECK_FALSE(history.canUndo());
}

TEST_CASE("Lane command selector executes repeatable lane edits with undo", "[M23][M27][REQ-24-3]") {
    juce::ScopedJuceInitialiser_GUI gui;
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    PatternStore store;
    LaneEditHistory history(apvts, store);
    juce::Component parent;
    phu::plugin::editor::LaneControlStrip strip(apvts, history, 1, parent);
    strip.layOut(phu::plugin::editor::layout::computeLaneRowRegions(
        {0, 0, 1000, phu::sequencing::constants::editor::laneRowHeight}));

    juce::ComboBox* selector = nullptr;
    juce::Component* base = nullptr;
    for (auto* child : parent.getChildren()) {
        if (child->getName() == "Lane commands") {
            selector = dynamic_cast<juce::ComboBox*>(child);
        } else if (auto* button = dynamic_cast<juce::TextButton*>(child); button != nullptr && button->getButtonText() == "Base") {
            base = button;
        }
    }
    REQUIRE(selector != nullptr);
    REQUIRE(base != nullptr);
    CHECK(selector->getY() > base->getBottom());
    CHECK(selector->getBottom() <= phu::sequencing::constants::editor::laneRowHeight);

    const auto itemId = [&](const juce::String& name) {
        for (int i = 0; i < selector->getNumItems(); ++i) {
            if (selector->getItemText(i) == name) {
                return selector->getItemId(i);
            }
        }
        return 0;
    };
    for (const auto* name : {"Fill random...", "Arithmetic sequence...", "Euclidean pattern..."}) {
        const int id = itemId(name);
        REQUIRE(id != 0);
        CHECK_FALSE(selector->isItemEnabled(id));
        selector->setSelectedId(id, juce::sendNotificationSync);
        CHECK_FALSE(history.canUndo());
    }
    CHECK(itemId("Set step on") == 0);
    CHECK(itemId("Set mute") == 0);

    REQUIRE(history.execute("SetStepCount", 1, {{4}, {}}));
    for (int step = 0; step < 4; ++step) {
        store.setStepEnabled(1, step, step == 1);
    }
    history.clear();
    const auto before = history.get(1);
    const auto other = history.get(0);
    const auto bits = [&] {
        std::string result;
        const auto lane = history.get(1);
        for (int step = 0; step < 4; ++step) {
            result += lane.document.pattern.steps.step(step).enabled ? '1' : '0';
        }
        return result;
    };
    const auto run = [&](const char* name, const char* expected) {
        const int id = itemId(name);
        REQUIRE(id != 0);
        REQUIRE(selector->isItemEnabled(id));
        selector->setSelectedId(id, juce::sendNotificationSync);
        CHECK(selector->getSelectedId() == 0);
        CHECK(bits() == expected);
        CHECK(history.get(0) == other);
        REQUIRE(history.canUndo());
        history.undo();
        CHECK(history.get(1) == before);
        history.redo();
        CHECK(bits() == expected);
        history.undo();
    };
    run("Reverse", "0010");
    run("Rotate left", "1000");
    run("Rotate right", "0010");
    run("Invert", "1011");
    run("Clear lane", "0000");
    run("Fill lane", "1111");

    const int rotate = itemId("Rotate right");
    selector->setSelectedId(rotate, juce::sendNotificationSync);
    selector->setSelectedId(rotate, juce::sendNotificationSync);
    CHECK(bits() == "0001");
    history.undo();
    CHECK(bits() == "0010");
    history.undo();
    CHECK(history.get(1) == before);
}

TEST_CASE("PatternStore: every edit and load is published to the audio side with a rising revision",
          "[M21][REQ-12]") {
    PatternStore store;
    auto& audio = store.audioSide();
    REQUIRE(audio.update());  // initial document published on construction
    auto lastRevision = audio.read().documentRevision;

    store.setStepEnabled(0, 1, false);
    store.setStepValue(0, 3, VelocityMode::byStep, 40);
    REQUIRE(audio.update());
    CHECK_FALSE(audio.read().lanes[0].steps.step(1).enabled);
    CHECK(audio.read().lanes[0].steps.step(3).valueByMode[static_cast<std::size_t>(VelocityMode::byStep)] == 40);
    CHECK(audio.read().documentRevision > lastRevision);
    lastRevision = audio.read().documentRevision;
    CHECK_FALSE(audio.update());

    // A loaded document carries its own, lower revision counter; the published one must still rise,
    // otherwise SequencerCore::setPatterns would ignore it as stale.
    for (int i = 0; i < 50; ++i) {
        store.setStepEnabled(1, 0, (i % 2) == 0);
    }
    REQUIRE(audio.update());
    lastRevision = audio.read().documentRevision;
    PatternDocument loaded;
    loaded.setStepEnabled(2, 5, false);
    REQUIRE(loaded.revision() < lastRevision);
    store.replaceDocument(loaded);
    REQUIRE(audio.update());
    CHECK(audio.read().documentRevision > lastRevision);
    CHECK_FALSE(audio.read().lanes[2].steps.step(5).enabled);
    CHECK(audio.read().lanes[0].steps.step(1).enabled);
    CHECK(store.copyDocument() == loaded);
}
