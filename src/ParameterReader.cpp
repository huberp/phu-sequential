#include "ParameterReader.h"

#include <cmath>

namespace phu::plugin {

namespace {

std::atomic<float>* rawValue(juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    auto* value = apvts.getRawParameterValue(id);
    jassert(value != nullptr);
    return value;
}

int load(const std::atomic<float>* value) noexcept
{
    const float raw = value->load(std::memory_order_relaxed);
    return std::isfinite(raw) ? static_cast<int>(std::lround(raw)) : 0;
}

} // namespace

ParameterReader::ParameterReader(juce::AudioProcessorValueTreeState& apvts)
{
    thru_ = rawValue(apvts, juce::String(parameter_ids::thru.data()));
    consume_ = rawValue(apvts, juce::String(parameter_ids::consume.data()));
    chordInputChannel_ = rawValue(apvts, juce::String(parameter_ids::chordInputChannel.data()));
    for (int lane = 0; lane < sequencing::constants::lanes::laneCount; ++lane) {
        for (int p = 0; p < parameter_ids::laneParameterCount; ++p) {
            lanes_[static_cast<std::size_t>(lane)][static_cast<std::size_t>(p)] =
                rawValue(apvts, parameter_ids::laneParameterId(lane, static_cast<parameter_ids::LaneParameter>(p)));
        }
    }
}

arpeggiator::ParameterSnapshot ParameterReader::read() const noexcept
{
    arpeggiator::ParameterSnapshot snapshot;
    snapshot.thru = load(thru_);
    snapshot.consume = load(consume_);
    snapshot.chordInputChannel = load(chordInputChannel_);
    for (std::size_t lane = 0; lane < lanes_.size(); ++lane) {
        const auto& p = lanes_[lane];
        auto& raw = snapshot.lanes[lane];
        raw.mute = load(p[0]);
        raw.latch = load(p[1]);
        raw.inputMode = load(p[2]);
        raw.followIndex = load(p[3]);
        raw.followChannel = load(p[4]);
        raw.outputChannel = load(p[5]);
        raw.transpose = load(p[6]);
        raw.gate = load(p[7]);
        raw.timingSource = load(p[8]);
        raw.baseTiming = load(p[9]);
        raw.subdivision = load(p[10]);
        raw.swing = load(p[11]);
        raw.stepCount = load(p[12]);
        raw.velocityMode = load(p[13]);
    }
    return snapshot;
}

} // namespace phu::plugin
