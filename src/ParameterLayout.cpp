#include "ParameterLayout.h"
#include "ParameterIds.h"
#include "arpeggiator/LaneConfiguration.h"
#include "sequencing/Constants.h"
#include "sequencing/NoteValues.h"
#include "sequencing/sources/InputModeRegistry.h"
#include "sequencing/velocity/VelocityModeRegistry.h"

#include <memory>

namespace phu::plugin {

namespace {

using juce::AudioParameterBool;
using juce::AudioParameterChoice;
using juce::AudioParameterInt;
using juce::ParameterID;
using namespace sequencing::constants;
using parameter_ids::LaneParameter;

ParameterID makeId(const juce::String& id)
{
    return ParameterID{id, parameter_ids::parameterVersionHint};
}

template <typename Registry>
juce::StringArray choiceNames(const Registry& registry)
{
    juce::StringArray names;
    for (const auto& definition : registry) {
        names.add(juce::String(definition.name.data()));
    }
    return names;
}

juce::StringArray baseTimingChoiceNames()
{
    juce::StringArray names;
    for (const auto& definition : sequencing::baseTimingRegistry()) {
        names.add(juce::String(definition.name.data()));
    }
    return names;
}

void addLaneParameters(juce::AudioProcessorValueTreeState::ParameterLayout& layout, int lane)
{
    const arpeggiator::LaneParameters defaults{};
    const auto id = [lane](LaneParameter p) { return makeId(parameter_ids::laneParameterId(lane, p)); };
    const auto name = [lane](LaneParameter p) { return parameter_ids::laneParameterName(lane, p); };
    const auto addBool = [&](LaneParameter p, bool value) {
        layout.add(std::make_unique<AudioParameterBool>(id(p), name(p), value));
    };
    const auto addInt = [&](LaneParameter p, int minimum, int maximum, int value) {
        layout.add(std::make_unique<AudioParameterInt>(id(p), name(p), minimum, maximum, value));
    };
    const auto addChoice = [&](LaneParameter p, const juce::StringArray& names, int value) {
        layout.add(std::make_unique<AudioParameterChoice>(id(p), name(p), names, value));
    };

    addBool(LaneParameter::mute, defaults.mute);
    addBool(LaneParameter::latch, defaults.latch);
    addChoice(LaneParameter::inputMode, choiceNames(sequencing::inputModeRegistry),
              static_cast<int>(defaults.inputMode));
    addInt(LaneParameter::followIndex, lanes::minimumFollowIndex, lanes::maximumFollowIndex, defaults.followIndex);
    addInt(LaneParameter::followChannel, midi::firstChannel, midi::lastChannel, defaults.followChannel);
    addInt(LaneParameter::outputChannel, midi::firstChannel, midi::lastChannel, defaults.outputChannel);
    addInt(LaneParameter::transpose, lanes::minimumTranspose, lanes::maximumTranspose, defaults.transpose);
    addInt(LaneParameter::gate, lanes::minimumGatePercent, lanes::maximumGatePercent, defaults.gatePercent);
    addChoice(LaneParameter::timingSource, choiceNames(arpeggiator::timingSourceRegistry),
              static_cast<int>(defaults.timingSource));
    addChoice(LaneParameter::baseTiming, baseTimingChoiceNames(), defaults.baseTimingIndex);
    addChoice(LaneParameter::subdivision, choiceNames(sequencing::subdivisionRegistry),
              static_cast<int>(defaults.subdivision));
    addInt(LaneParameter::swing, lanes::minimumSwingPercent, lanes::maximumSwingPercent, defaults.swingPercent);
    addInt(LaneParameter::stepCount, lanes::minimumStepCount, lanes::maximumStepCount, defaults.stepCount);
    addChoice(LaneParameter::velocityMode, choiceNames(sequencing::velocityModeRegistry),
              static_cast<int>(defaults.velocityMode));
}

} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const arpeggiator::GlobalParameters defaults{};

    layout.add(std::make_unique<AudioParameterBool>(
        makeId(juce::String(parameter_ids::thru.data())), "Thru", defaults.thru));
    layout.add(std::make_unique<AudioParameterBool>(
        makeId(juce::String(parameter_ids::consume.data())), "Consume", defaults.consume));
    layout.add(std::make_unique<AudioParameterInt>(
        makeId(juce::String(parameter_ids::chordInputChannel.data())), "Chord Input Channel",
        midi::firstChannel, midi::lastChannel, defaults.chordInputChannel));

    for (int lane = 0; lane < lanes::laneCount; ++lane) {
        addLaneParameters(layout, lane);
    }

    layout.add(std::make_unique<AudioParameterBool>(
        makeId(juce::String(parameter_ids::bypass.data())), "Bypass", false));
    return layout;
}

} // namespace phu::plugin
