#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace phu::plugin {

// 115 spec parameters (sec. 12) plus host bypass. Ranges, defaults and choice lists come from constants/registries. [MT]
[[nodiscard]] juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace phu::plugin
