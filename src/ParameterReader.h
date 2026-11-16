#pragma once

#include "ParameterIds.h"
#include "arpeggiator/ParameterSnapshot.h"
#include "sequencing/Constants.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace phu::plugin {

// Caches raw value pointers once; read() is lock-free (sec. 17, D-21).
class ParameterReader {
public:
    explicit ParameterReader(juce::AudioProcessorValueTreeState&);  // [MT]

    [[nodiscard]] arpeggiator::ParameterSnapshot read() const noexcept;  // [RT]

private:
    std::atomic<float>* thru_ = nullptr;
    std::atomic<float>* consume_ = nullptr;
    std::atomic<float>* chordInputChannel_ = nullptr;
    std::array<std::array<std::atomic<float>*, parameter_ids::laneParameterCount>,
               sequencing::constants::lanes::laneCount> lanes_{};
};

} // namespace phu::plugin
