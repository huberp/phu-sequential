#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <string_view>

// The only place spelling parameter IDs (M20, sec. 12). Lane IDs use user-visible lane numbers 1..8.
namespace phu::plugin::parameter_ids {

inline constexpr std::string_view thru = "thru";
inline constexpr std::string_view consume = "consume";
inline constexpr std::string_view chordInputChannel = "chordInputChannel";
inline constexpr std::string_view bypass = "bypass";

enum class LaneParameter : std::uint8_t {
    mute, latch, inputMode, followIndex, followChannel, outputChannel, transpose,
    gate, timingSource, baseTiming, subdivision, swing, stepCount, velocityMode
};

inline constexpr int laneParameterCount = 14;

inline constexpr std::array<std::string_view, laneParameterCount> laneParameterSuffixes{
    "mute", "latch", "inputMode", "followIndex", "followChannel", "outputChannel", "transpose", "gate",
    "timingSource", "baseTiming", "subdivision", "swing", "stepCount", "velocityMode"};

inline constexpr std::array<std::string_view, laneParameterCount> laneParameterDisplayNames{
    "Mute", "Latch", "Input Mode", "Follow Index", "Follow Channel", "Output Channel", "Transpose", "Gate",
    "Timing Source", "Base Timing", "Subdivision", "Swing", "Step Count", "Velocity Mode"};

// Version hint required by AU/VST3 wrappers; bump only for newly added parameters.
inline constexpr int parameterVersionHint = 1;

[[nodiscard]] juce::String laneParameterId(int laneIndex0, LaneParameter);    // "lane1_followIndex"
[[nodiscard]] juce::String laneParameterName(int laneIndex0, LaneParameter);  // "Lane 1 Follow Index"

} // namespace phu::plugin::parameter_ids
