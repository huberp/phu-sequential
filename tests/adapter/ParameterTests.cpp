#include <catch2/catch_test_macros.hpp>

#include "ParameterIds.h"
#include "ParameterLayout.h"
#include "ParameterReader.h"
#include "arpeggiator/ConfigurationBuilder.h"
#include "sequencing/Constants.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <set>

// M20 adapter: parameter layout (IDs, ranges, defaults) and ParameterReader (sec. 12, 20, #13).

using namespace phu::plugin;
using namespace phu::sequencing::constants;
using phu::plugin::parameter_ids::LaneParameter;

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

juce::String laneId(int lane, LaneParameter p) { return parameter_ids::laneParameterId(lane, p); }

}  // namespace

TEST_CASE("Parameters: layout contains exactly the spec parameter set plus bypass",
          "[M20][REQ-12-1][REQ-12-2][REQ-12-4][REQ-12-6]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());

    std::set<std::string> expected{"thru", "consume", "chordInputChannel", "bypass"};
    for (int lane = 1; lane <= lanes::laneCount; ++lane) {
        for (const auto suffix : parameter_ids::laneParameterSuffixes) {
            expected.insert("lane" + std::to_string(lane) + "_" + std::string(suffix));
        }
    }
    CHECK(expected.size() == 3 + lanes::laneCount * 14 + 1);

    std::set<std::string> actual;
    for (auto* parameter : processor.getParameters()) {
        actual.insert(static_cast<juce::AudioProcessorParameterWithID*>(parameter)->getParameterID().toStdString());
    }
    CHECK(actual == expected);

    CHECK(laneId(2, LaneParameter::followIndex) == "lane3_followIndex");
    CHECK(parameter_ids::laneParameterName(2, LaneParameter::swing) == "Lane 3 Swing");
}

TEST_CASE("Parameters: default parameter values map to sec. 20 defaults",
          "[M20][REQ-12-3][REQ-20-3]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    ParameterReader reader(apvts);

    CHECK(phu::arpeggiator::buildConfiguration(reader.read()) == phu::arpeggiator::defaultSequencerConfiguration());
    CHECK(apvts.getParameter("bypass")->getValue() == 0.0f);
}

TEST_CASE("Parameters: ranges equal the constants", "[M20][REQ-12-3]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());

    const auto range = [&](const juce::String& id) { return apvts.getParameter(id)->getNormalisableRange(); };
    CHECK(range("chordInputChannel").start == 1.0f);
    CHECK(range("chordInputChannel").end == 16.0f);
    const auto transpose = range(laneId(0, LaneParameter::transpose));
    CHECK(transpose.start == -24.0f);
    CHECK(transpose.end == 24.0f);
    const auto gate = range(laneId(7, LaneParameter::gate));
    CHECK(gate.start == 1.0f);
    CHECK(gate.end == 200.0f);
    const auto steps = range(laneId(4, LaneParameter::stepCount));
    CHECK(steps.start == 1.0f);
    CHECK(steps.end == 64.0f);
    const auto follow = range(laneId(0, LaneParameter::followIndex));
    CHECK(follow.start == 0.0f);
    CHECK(follow.end == 15.0f);
    const auto swing = range(laneId(0, LaneParameter::swing));
    CHECK(swing.start == 0.0f);
    CHECK(swing.end == 100.0f);
    CHECK(range(laneId(0, LaneParameter::baseTiming)).end == 5.0f);
    CHECK(range(laneId(0, LaneParameter::velocityMode)).end == 3.0f);
}

TEST_CASE("Parameters: reader reflects host automation", "[M20][REQ-12-5]") {
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", createParameterLayout());
    ParameterReader reader(apvts);

    const auto set = [&](const juce::String& id, float value) {
        auto* parameter = apvts.getParameter(id);
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };
    set("thru", 1.0f);
    set(laneId(2, LaneParameter::transpose), -7.0f);
    set(laneId(2, LaneParameter::subdivision), 1.0f);
    set(laneId(2, LaneParameter::mute), 1.0f);

    const auto config = phu::arpeggiator::buildConfiguration(reader.read());
    CHECK(config.global.thru);
    CHECK(config.lanes[2].transpose == -7);
    CHECK(config.lanes[2].subdivision == phu::sequencing::Subdivision::triplet);
    CHECK(config.lanes[2].mute);
    CHECK_FALSE(config.lanes[1].mute);
}
