#pragma once

#include "arpeggiator/TransportTracker.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace phu::plugin {

[[nodiscard]] arpeggiator::TransportInfo readHostTransport(juce::AudioPlayHead*) noexcept;

}  // namespace phu::plugin
