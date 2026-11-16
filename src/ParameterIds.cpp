#include "ParameterIds.h"

namespace phu::plugin::parameter_ids {

juce::String laneParameterId(int laneIndex0, LaneParameter parameter)
{
    const auto index = static_cast<std::size_t>(parameter);
    return "lane" + juce::String(laneIndex0 + 1) + "_" + juce::String(laneParameterSuffixes[index].data());
}

juce::String laneParameterName(int laneIndex0, LaneParameter parameter)
{
    const auto index = static_cast<std::size_t>(parameter);
    return "Lane " + juce::String(laneIndex0 + 1) + " " + juce::String(laneParameterDisplayNames[index].data());
}

} // namespace phu::plugin::parameter_ids
