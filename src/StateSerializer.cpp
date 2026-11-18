#include "StateSerializer.h"

#include "ParameterIds.h"
#include "ParameterReader.h"
#include "arpeggiator/ConfigurationBuilder.h"
#include "arpeggiator/LaneConfiguration.h"
#include "arpeggiator/LaneState.h"
#include "sequencing/NoteValues.h"
#include "sequencing/StepData.h"
#include "sequencing/TextGridParser.h"
#include "sequencing/sources/InputModeRegistry.h"
#include "sequencing/velocity/VelocityModeRegistry.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>

namespace phu::plugin {

namespace {

using namespace sequencing::constants;
using parameter_ids::LaneParameter;

constexpr auto rootType = "PhuSequential";
constexpr auto presetInfoType = "PresetInfo";
constexpr auto globalType = "Global";
constexpr auto laneType = "Lane";
constexpr auto textGridType = "TextGrid";
constexpr auto stepsType = "Steps";
constexpr auto stepsOnKey = "on";

juce::String toJuce(std::string_view text) {
    return juce::String::fromUTF8(text.data(), static_cast<int>(text.size()));
}

juce::String laneKey(LaneParameter p) {
    return toJuce(parameter_ids::laneParameterSuffixes[static_cast<std::size_t>(p)]);
}

// Parses an optionally signed decimal integer; saturates instead of overflowing.
bool parseInteger(const juce::String& text, std::int64_t& value) {
    const auto trimmed = text.trim();
    auto digits = trimmed;
    bool negative = false;
    if (digits.startsWithChar('-') || digits.startsWithChar('+')) {
        negative = digits.startsWithChar('-');
        digits = digits.substring(1);
    }
    if (digits.isEmpty() || !digits.containsOnly("0123456789")) {
        return false;
    }
    if (digits.length() > 15) {
        value = negative ? -1'000'000'000'000'000LL : 1'000'000'000'000'000LL;
        return true;
    }
    value = digits.getLargeIntValue();
    if (negative) {
        value = -value;
    }
    return true;
}

// Collects whether any field had to be clamped or defaulted because it was invalid.
struct Reader {
    bool clamped = false;

    int readInt(const juce::ValueTree& tree, const char* key, int defaultValue, int minimum, int maximum) {
        if (!tree.hasProperty(key)) {
            return defaultValue;
        }
        std::int64_t parsed = 0;
        if (!parseInteger(tree.getProperty(key).toString(), parsed)) {
            clamped = true;
            return defaultValue;
        }
        const auto limited = std::clamp<std::int64_t>(parsed, minimum, maximum);
        clamped = clamped || limited != parsed;
        return static_cast<int>(limited);
    }

    bool readBool(const juce::ValueTree& tree, const char* key, bool defaultValue) {
        return readInt(tree, key, defaultValue ? 1 : 0, 0, 1) != 0;
    }

    template <typename KeyAt>
    int readKey(const juce::ValueTree& tree, const char* key, int count, KeyAt keyAt, int defaultIndex) {
        if (!tree.hasProperty(key)) {
            return defaultIndex;
        }
        const auto text = tree.getProperty(key).toString().trim();
        for (int i = 0; i < count; ++i) {
            if (text == toJuce(keyAt(i))) {
                return i;
            }
        }
        clamped = true;
        return defaultIndex;
    }
};

// Splits "a,b,c" without allocating more than the pieces themselves.
juce::StringArray splitCsv(const juce::String& text) {
    juce::StringArray parts;
    parts.addTokens(text, ",", "");
    return parts;
}

juce::String stepsCsv(const arpeggiator::LanePattern& pattern, sequencing::VelocityMode mode, bool enabledColumn) {
    juce::StringArray parts;
    for (int i = 0; i < lanes::maximumStepCount; ++i) {
        const auto& step = pattern.steps.step(i);
        parts.add(enabledColumn ? juce::String(step.enabled ? 1 : 0)
                                : juce::String(step.valueByMode[static_cast<std::size_t>(mode)]));
    }
    return parts.joinIntoString(",");
}

juce::String colorText(arpeggiator::LaneColor color) {
    return juce::String::formatted("#%02X%02X%02X", color.red, color.green, color.blue);
}

bool parseColor(const juce::String& text, arpeggiator::LaneColor& color) {
    const auto trimmed = text.trim();
    if (trimmed.length() != 7 || !trimmed.startsWithChar('#') ||
        !trimmed.substring(1).containsOnly("0123456789abcdefABCDEF")) {
        return false;
    }
    const auto rgb = static_cast<std::uint32_t>(trimmed.substring(1).getHexValue32());
    color = {static_cast<std::uint8_t>((rgb >> 16) & 0xFF), static_cast<std::uint8_t>((rgb >> 8) & 0xFF),
             static_cast<std::uint8_t>(rgb & 0xFF)};
    return true;
}

int laneValue(const arpeggiator::LaneParameters& lane, LaneParameter p) {
    switch (p) {
        case LaneParameter::mute: return lane.mute ? 1 : 0;
        case LaneParameter::latch: return lane.latch ? 1 : 0;
        case LaneParameter::inputMode: return static_cast<int>(lane.inputMode);
        case LaneParameter::followIndex: return lane.followIndex;
        case LaneParameter::followChannel: return lane.followChannel;
        case LaneParameter::outputChannel: return lane.outputChannel;
        case LaneParameter::transpose: return lane.transpose;
        case LaneParameter::gate: return lane.gatePercent;
        case LaneParameter::timingSource: return static_cast<int>(lane.timingSource);
        case LaneParameter::baseTiming: return lane.baseTimingIndex;
        case LaneParameter::subdivision: return static_cast<int>(lane.subdivision);
        case LaneParameter::swing: return lane.swingPercent;
        case LaneParameter::stepCount: return lane.stepCount;
        case LaneParameter::velocityMode: return static_cast<int>(lane.velocityMode);
    }
    return 0;
}

juce::String laneAttribute(const arpeggiator::LaneParameters& lane, LaneParameter p) {
    const int value = laneValue(lane, p);
    switch (p) {
        case LaneParameter::inputMode: return toJuce(sequencing::inputModeRegistry[static_cast<std::size_t>(value)].persistenceKey);
        case LaneParameter::timingSource:
            return toJuce(arpeggiator::timingSourceRegistry[static_cast<std::size_t>(value)].persistenceKey);
        case LaneParameter::baseTiming: return toJuce(sequencing::baseTimingRegistry()[static_cast<std::size_t>(value)].name);
        case LaneParameter::subdivision:
            return toJuce(sequencing::subdivisionRegistry[static_cast<std::size_t>(value)].persistenceKey);
        case LaneParameter::velocityMode:
            return toJuce(sequencing::velocityModeRegistry[static_cast<std::size_t>(value)].persistenceKey);
        default: return juce::String(value);
    }
}

arpeggiator::ParameterSnapshot toParameterSnapshot(const arpeggiator::SequencerConfiguration& config) {
    arpeggiator::ParameterSnapshot snapshot;
    snapshot.thru = config.global.thru ? 1 : 0;
    snapshot.consume = config.global.consume ? 1 : 0;
    snapshot.chordInputChannel = config.global.chordInputChannel;
    for (std::size_t i = 0; i < config.lanes.size(); ++i) {
        const auto& lane = config.lanes[i];
        auto& raw = snapshot.lanes[i];
        raw.mute = laneValue(lane, LaneParameter::mute);
        raw.latch = laneValue(lane, LaneParameter::latch);
        raw.inputMode = laneValue(lane, LaneParameter::inputMode);
        raw.followIndex = lane.followIndex;
        raw.followChannel = lane.followChannel;
        raw.outputChannel = lane.outputChannel;
        raw.transpose = lane.transpose;
        raw.gate = lane.gatePercent;
        raw.timingSource = laneValue(lane, LaneParameter::timingSource);
        raw.baseTiming = lane.baseTimingIndex;
        raw.subdivision = laneValue(lane, LaneParameter::subdivision);
        raw.swing = lane.swingPercent;
        raw.stepCount = lane.stepCount;
        raw.velocityMode = laneValue(lane, LaneParameter::velocityMode);
    }
    return snapshot;
}

void setParameter(juce::AudioProcessorValueTreeState& apvts, const juce::String& id, int value) {
    if (auto* parameter = apvts.getParameter(id)) {
        parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(value)));
    }
}

arpeggiator::LaneParameters readLaneParameters(const juce::ValueTree& tree, Reader& reader) {
    const arpeggiator::LaneParameters defaults{};
    arpeggiator::LaneParameters lane;
    lane.mute = reader.readBool(tree, laneKey(LaneParameter::mute).toRawUTF8(), defaults.mute);
    lane.latch = reader.readBool(tree, laneKey(LaneParameter::latch).toRawUTF8(), defaults.latch);
    lane.inputMode = static_cast<sequencing::InputMode>(reader.readKey(
        tree, laneKey(LaneParameter::inputMode).toRawUTF8(), sequencing::inputModeCount,
        [](int i) { return sequencing::inputModeRegistry[static_cast<std::size_t>(i)].persistenceKey; },
        static_cast<int>(defaults.inputMode)));
    lane.followIndex = reader.readInt(tree, laneKey(LaneParameter::followIndex).toRawUTF8(), defaults.followIndex,
                                      lanes::minimumFollowIndex, lanes::maximumFollowIndex);
    lane.followChannel = reader.readInt(tree, laneKey(LaneParameter::followChannel).toRawUTF8(),
                                        defaults.followChannel, midi::firstChannel, midi::lastChannel);
    lane.outputChannel = reader.readInt(tree, laneKey(LaneParameter::outputChannel).toRawUTF8(),
                                        defaults.outputChannel, midi::firstChannel, midi::lastChannel);
    lane.transpose = reader.readInt(tree, laneKey(LaneParameter::transpose).toRawUTF8(), defaults.transpose,
                                    lanes::minimumTranspose, lanes::maximumTranspose);
    lane.gatePercent = reader.readInt(tree, laneKey(LaneParameter::gate).toRawUTF8(), defaults.gatePercent,
                                      lanes::minimumGatePercent, lanes::maximumGatePercent);
    lane.timingSource = static_cast<arpeggiator::TimingSource>(reader.readKey(
        tree, laneKey(LaneParameter::timingSource).toRawUTF8(), arpeggiator::timingSourceCount,
        [](int i) { return arpeggiator::timingSourceRegistry[static_cast<std::size_t>(i)].persistenceKey; },
        static_cast<int>(defaults.timingSource)));
    lane.baseTimingIndex = reader.readKey(
        tree, laneKey(LaneParameter::baseTiming).toRawUTF8(), static_cast<int>(sequencing::baseTimingRegistry().size()),
        [](int i) { return sequencing::baseTimingRegistry()[static_cast<std::size_t>(i)].name; },
        defaults.baseTimingIndex);
    lane.subdivision = static_cast<sequencing::Subdivision>(reader.readKey(
        tree, laneKey(LaneParameter::subdivision).toRawUTF8(), sequencing::subdivisionCount,
        [](int i) { return sequencing::subdivisionRegistry[static_cast<std::size_t>(i)].persistenceKey; },
        static_cast<int>(defaults.subdivision)));
    lane.swingPercent = reader.readInt(tree, laneKey(LaneParameter::swing).toRawUTF8(), defaults.swingPercent,
                                       lanes::minimumSwingPercent, lanes::maximumSwingPercent);
    lane.stepCount = reader.readInt(tree, laneKey(LaneParameter::stepCount).toRawUTF8(), defaults.stepCount,
                                    lanes::minimumStepCount, lanes::maximumStepCount);
    lane.velocityMode = static_cast<sequencing::VelocityMode>(reader.readKey(
        tree, laneKey(LaneParameter::velocityMode).toRawUTF8(), sequencing::velocityModeCount,
        [](int i) { return sequencing::velocityModeRegistry[static_cast<std::size_t>(i)].persistenceKey; },
        static_cast<int>(defaults.velocityMode)));
    return lane;
}

void readSteps(const juce::ValueTree& stepsTree, sequencing::StepPattern& steps, Reader& reader) {
    if (stepsTree.hasProperty(stepsOnKey)) {
        const auto parts = splitCsv(stepsTree.getProperty(stepsOnKey).toString());
        for (int i = 0; i < std::min(parts.size(), lanes::maximumStepCount); ++i) {
            std::int64_t parsed = 0;
            if (!parseInteger(parts[i], parsed) || (parsed != 0 && parsed != 1)) {
                reader.clamped = true;
                continue;
            }
            steps.setEnabled(i, parsed == 1);
        }
    }
    for (const auto& definition : sequencing::velocityModeRegistry) {
        if (!definition.usesStepValues) {
            continue;
        }
        const auto key = toJuce(definition.persistenceKey);
        if (!stepsTree.hasProperty(key)) {
            continue;
        }
        const auto mode = static_cast<sequencing::VelocityMode>(&definition - sequencing::velocityModeRegistry.data());
        const auto parts = splitCsv(stepsTree.getProperty(key).toString());
        for (int i = 0; i < std::min(parts.size(), lanes::maximumStepCount); ++i) {
            std::int64_t parsed = 0;
            if (!parseInteger(parts[i], parsed)) {
                reader.clamped = true;
                continue;
            }
            const int limited = sequencing::clampStepValue(mode, static_cast<int>(std::clamp<std::int64_t>(
                                                                     parsed, -1'000'000, 1'000'000)));
            reader.clamped = reader.clamped || limited != parsed;
            steps.setValue(i, mode, limited);
        }
    }
}

// Reads one Lane tree; missing or invalid fields fall back to the lane's sec. 20 defaults.
arpeggiator::LaneState readLane(const juce::ValueTree& lane, int laneIndex, Reader& reader) {
    auto state = arpeggiator::defaultLaneState(laneIndex);
    state.configuration = readLaneParameters(lane, reader);
    auto& document = state.document;

    if (lane.hasProperty("color") && !parseColor(lane.getProperty("color").toString(), document.color)) {
        reader.clamped = true;
        document.color = arpeggiator::defaultLaneColor(laneIndex);
    }

    if (const auto grid = lane.getChildWithName(textGridType); grid.isValid()) {
        document.textGridText = grid.getProperty("text").toString().toStdString();
        const auto appliedText = grid.getProperty("applied").toString().toStdString();
        if (const auto applied = sequencing::parseTextGrid(appliedText); applied.grid) {
            document.pattern.appliedTextGrid = *applied.grid;
        } else {
            document.pattern.appliedTextGrid = {};
            reader.clamped = reader.clamped || !appliedText.empty();
        }
    }

    if (const auto steps = lane.getChildWithName(stepsType); steps.isValid()) {
        readSteps(steps, document.pattern.steps, reader);
    }
    return state;
}

LoadResult reject(LoadOutcome outcome) {
    return LoadResult{outcome, PluginStateData{}};
}

}  // namespace

juce::ValueTree toValueTree(const PluginStateData& state, StateKind kind, const juce::String& presetName) {
    juce::ValueTree root(rootType);
    root.setProperty("formatVersion", sequencing::constants::state::currentFormatVersion, nullptr);
    root.setProperty("kind", kind == StateKind::preset ? "preset" : "state", nullptr);

    if (kind == StateKind::preset) {
        juce::ValueTree info(presetInfoType);
        info.setProperty("name", presetName, nullptr);
        root.appendChild(info, nullptr);
    }

    juce::ValueTree global(globalType);
    global.setProperty("thru", state.parameters.global.thru ? 1 : 0, nullptr);
    global.setProperty("consume", state.parameters.global.consume ? 1 : 0, nullptr);
    global.setProperty("chordInputChannel", state.parameters.global.chordInputChannel, nullptr);
    root.appendChild(global, nullptr);

    for (int i = 0; i < lanes::laneCount; ++i) {
        root.appendChild(laneToValueTree(arpeggiator::LaneState{state.parameters.lanes[static_cast<std::size_t>(i)],
                                                                state.document.lane(i)},
                                         i),
                         nullptr);
    }
    return root;
}

juce::ValueTree laneToValueTree(const arpeggiator::LaneState& state, int laneIndex) {
    const int index = std::clamp(laneIndex, 0, lanes::laneCount - 1);
    const auto& parameters = state.configuration;
    const auto& document = state.document;

    juce::ValueTree lane(laneType);
    lane.setProperty("index", index + 1, nullptr);
    for (int p = 0; p < parameter_ids::laneParameterCount; ++p) {
        const auto parameter = static_cast<LaneParameter>(p);
        lane.setProperty(laneKey(parameter), laneAttribute(parameters, parameter), nullptr);
    }
    lane.setProperty("color", colorText(document.color), nullptr);

    juce::ValueTree grid(textGridType);
    grid.setProperty("text", toJuce(document.textGridText), nullptr);
    grid.setProperty("applied", toJuce(sequencing::formatTextGrid(document.pattern.appliedTextGrid)), nullptr);
    lane.appendChild(grid, nullptr);

    juce::ValueTree steps(stepsType);
    steps.setProperty(stepsOnKey, stepsCsv(document.pattern, sequencing::VelocityMode::incoming, true), nullptr);
    for (std::size_t m = 0; m < sequencing::velocityModeRegistry.size(); ++m) {
        const auto& definition = sequencing::velocityModeRegistry[m];
        if (definition.usesStepValues) {
            steps.setProperty(toJuce(definition.persistenceKey),
                              stepsCsv(document.pattern, static_cast<sequencing::VelocityMode>(m), false), nullptr);
        }
    }
    lane.appendChild(steps, nullptr);
    return lane;
}

LaneStateLoadResult laneFromValueTree(const juce::ValueTree& laneTree, int laneIndex) {
    const int index = std::clamp(laneIndex, 0, lanes::laneCount - 1);
    LaneStateLoadResult result;
    if (!laneTree.isValid() || !laneTree.hasType(laneType)) {
        result.state = arpeggiator::defaultLaneState(index);
        result.valid = false;
        return result;
    }
    Reader reader;
    result.state = readLane(laneTree, index, reader);
    result.state.configuration = arpeggiator::clampLaneParameters(result.state.configuration);
    arpeggiator::PatternDocument scratch;
    scratch.setLane(index, result.state.document);
    scratch.enforceStepCountInvariant(index, result.state.configuration);
    result.state.document = scratch.lane(index);
    result.clamped = reader.clamped;
    return result;
}

LoadResult fromValueTree(const juce::ValueTree& input, std::span<const MigrationFunction> chain, int currentVersion) {
    if (!input.isValid() || !input.hasType(rootType) || !input.hasProperty("formatVersion")) {
        return reject(LoadOutcome::rejectedCorrupt);
    }
    std::int64_t version = 0;
    if (!parseInteger(input.getProperty("formatVersion").toString(), version) || version < 1) {
        return reject(LoadOutcome::rejectedCorrupt);
    }
    if (version > currentVersion) {
        return reject(LoadOutcome::rejectedNewerVersion);
    }

    const bool migrated = version < currentVersion;
    auto tree = input;
    if (migrated) {
        auto result = migrateToVersion(input.createCopy(), currentVersion, chain);
        if (!result) {
            return reject(LoadOutcome::rejectedCorrupt);
        }
        tree = *result;
    }

    Reader reader;
    LoadResult result;
    auto& data = result.data;

    if (const auto global = tree.getChildWithName(globalType); global.isValid()) {
        const arpeggiator::GlobalParameters defaults{};
        data.parameters.global.thru = reader.readBool(global, "thru", defaults.thru);
        data.parameters.global.consume = reader.readBool(global, "consume", defaults.consume);
        data.parameters.global.chordInputChannel = reader.readInt(
            global, "chordInputChannel", defaults.chordInputChannel, midi::firstChannel, midi::lastChannel);
    }

    for (const auto& lane : tree) {
        if (!lane.hasType(laneType)) {
            continue;
        }
        std::int64_t index = 0;
        if (!parseInteger(lane.getProperty("index").toString(), index) || index < 1 || index > lanes::laneCount) {
            continue;
        }
        const int laneIndex = static_cast<int>(index) - 1;
        auto& parameters = data.parameters.lanes[static_cast<std::size_t>(laneIndex)];
        auto laneState = readLane(lane, laneIndex, reader);
        parameters = laneState.configuration;

        data.document.setLane(laneIndex, std::move(laneState.document));
        data.document.enforceStepCountInvariant(laneIndex, parameters);
    }

    data.parameters = arpeggiator::clampConfiguration(data.parameters);
    result.outcome = reader.clamped ? LoadOutcome::loadedWithClamping
                                    : (migrated ? LoadOutcome::migrated : LoadOutcome::loaded);
    return result;
}

juce::String toXmlText(const PluginStateData& state, StateKind kind, const juce::String& presetName) {
    return toValueTree(state, kind, presetName).toXmlString();
}

LoadResult fromXmlText(const juce::String& text) {
    const auto xml = juce::XmlDocument::parse(text);
    if (xml == nullptr) {
        return reject(LoadOutcome::rejectedCorrupt);
    }
    return fromValueTree(juce::ValueTree::fromXml(*xml));
}

void writeStateBlock(const PluginStateData& state, juce::MemoryBlock& block) {
    if (const auto xml = toValueTree(state, StateKind::state).createXml()) {
        juce::AudioProcessor::copyXmlToBinary(*xml, block);
    }
}

LoadResult readStateBlock(const void* data, int sizeInBytes) {
    if (data == nullptr || sizeInBytes <= 0) {
        return reject(LoadOutcome::rejectedCorrupt);
    }
    const auto xml = juce::AudioProcessor::getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr) {
        return reject(LoadOutcome::rejectedCorrupt);
    }
    return fromValueTree(juce::ValueTree::fromXml(*xml));
}

PluginStateData captureState(juce::AudioProcessorValueTreeState& apvts, const PatternStore& store) {
    PluginStateData state;
    state.parameters = arpeggiator::buildConfiguration(ParameterReader(apvts).read());
    state.document = store.copyDocument();
    return state;
}

arpeggiator::LaneState captureLaneState(juce::AudioProcessorValueTreeState& apvts, const PatternStore& store,
                                        int laneIndex) {
    const int index = std::clamp(laneIndex, 0, lanes::laneCount - 1);
    const auto config = arpeggiator::buildConfiguration(ParameterReader(apvts).read());
    return arpeggiator::LaneState{config.lanes[static_cast<std::size_t>(index)], store.copyDocument().lane(index)};
}

void applyLaneState(const arpeggiator::LaneState& state, int laneIndex, juce::AudioProcessorValueTreeState& apvts,
                    PatternStore& store) {
    const int index = std::clamp(laneIndex, 0, lanes::laneCount - 1);
    for (int p = 0; p < parameter_ids::laneParameterCount; ++p) {
        const auto parameter = static_cast<LaneParameter>(p);
        setParameter(apvts, parameter_ids::laneParameterId(index, parameter), laneValue(state.configuration, parameter));
    }
    auto document = store.copyDocument();
    document.setLane(index, state.document);
    document.enforceStepCountInvariant(index, state.configuration);
    store.replaceDocument(std::move(document));
}

void applyState(const PluginStateData& state, juce::AudioProcessorValueTreeState& apvts, PatternStore& store) {
    const auto snapshot = toParameterSnapshot(state.parameters);
    setParameter(apvts, juce::String(parameter_ids::thru.data()), snapshot.thru);
    setParameter(apvts, juce::String(parameter_ids::consume.data()), snapshot.consume);
    setParameter(apvts, juce::String(parameter_ids::chordInputChannel.data()), snapshot.chordInputChannel);
    for (int i = 0; i < lanes::laneCount; ++i) {
        const auto& raw = snapshot.lanes[static_cast<std::size_t>(i)];
        const std::array<int, parameter_ids::laneParameterCount> values{
            raw.mute, raw.latch, raw.inputMode, raw.followIndex, raw.followChannel, raw.outputChannel, raw.transpose,
            raw.gate, raw.timingSource, raw.baseTiming, raw.subdivision, raw.swing, raw.stepCount, raw.velocityMode};
        for (int p = 0; p < parameter_ids::laneParameterCount; ++p) {
            setParameter(apvts, parameter_ids::laneParameterId(i, static_cast<LaneParameter>(p)),
                         values[static_cast<std::size_t>(p)]);
        }
    }
    store.replaceDocument(state.document);
}

}  // namespace phu::plugin
