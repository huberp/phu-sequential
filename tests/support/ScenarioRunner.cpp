#include "ScenarioRunner.h"

#include <array>

namespace phu::arpeggiator::test {

using namespace phu::sequencing;

ScenarioRunner::ScenarioRunner(SequencerConfiguration config) noexcept
    : config_(config), output_(constants::capacity::outputEventsPerBlock) {}

BlockResult ScenarioRunner::runChunk(const BlockInput& input) {
    output_.clear();
    const BlockResult result = core_.processBlock(input, config_, output_);
    for (const auto& event : output_.events()) {
        captured_.push_back(CapturedEvent{sampleCursor_ + event.sampleOffset, event.kind, event.channel,
                                           event.data1, event.data2});
    }
    sampleCursor_ += input.numSamples;
    return result;
}

bool ScenarioRunner::allNotesClosed() const noexcept {
    // open[channel][pitch]: number of Note-Ons not yet matched by a Note-Off, keyed exactly as
    // the registry does (per channel + pitch, across every owner).
    std::array<std::array<int, constants::midi::pitchCount>, constants::midi::channelCount> open{};
    for (const auto& event : captured_) {
        if (event.channel < 1 || event.channel > constants::midi::channelCount) {
            continue;
        }
        const int channelIndex = event.channel - 1;
        if (event.pitch < 0 || event.pitch >= constants::midi::pitchCount) {
            continue;
        }
        if (event.kind == MidiEventKind::noteOn) {
            ++open[static_cast<std::size_t>(channelIndex)][static_cast<std::size_t>(event.pitch)];
        } else if (event.kind == MidiEventKind::noteOff) {
            int& count = open[static_cast<std::size_t>(channelIndex)][static_cast<std::size_t>(event.pitch)];
            if (count <= 0) {
                return false;  // Note-Off without a preceding open Note-On
            }
            --count;
        }
    }
    for (const auto& perChannel : open) {
        for (int count : perChannel) {
            if (count != 0) {
                return false;  // a Note-On never received its Note-Off
            }
        }
    }
    return true;
}

}  // namespace phu::arpeggiator::test
