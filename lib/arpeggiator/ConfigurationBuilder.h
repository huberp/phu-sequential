#pragma once

#include "arpeggiator/ParameterSnapshot.h"
#include "arpeggiator/SequencerConfiguration.h"

namespace phu::arpeggiator {

// Maps indices to enums via the registries and clamps every field to its constants range (sec. 17). [RT]
[[nodiscard]] SequencerConfiguration buildConfiguration(const ParameterSnapshot&) noexcept;

} // namespace phu::arpeggiator
