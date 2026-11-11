#pragma once
#include <optional>

namespace phu::sequencing {

class HeldNoteTracker;
struct SourceSelector;
struct HeldNote;

using SourceNote = HeldNote;

[[nodiscard]] std::optional<SourceNote> selectChannelSource(const HeldNoteTracker&, const SourceSelector&) noexcept;

}  // namespace phu::sequencing
