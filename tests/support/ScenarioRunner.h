#pragma once

#include "arpeggiator/SequencerCore.h"
#include "arpeggiator/BlockTypes.h"
#include "arpeggiator/SequencerConfiguration.h"
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/MidiEvent.h"
#include "sequencing/Constants.h"

#include <cstdint>
#include <vector>

namespace phu::arpeggiator::test {

// One output event as captured by ScenarioRunner, with its sample offset rewritten to be
// absolute (relative to the first chunk ever fed to the runner) instead of chunk-relative.
// This lets scenarios that split a host block into several core calls, or that compare runs
// using different chunk sizes, compare positions on one timeline (REQ-10-11, REQ-10-13).
struct CapturedEvent {
    std::int64_t absoluteSample = 0;
    sequencing::MidiEventKind kind = sequencing::MidiEventKind::noteOn;
    int channel = 0;
    int pitch = 0;
    int velocity = 0;
};

// Drives a SequencerCore across one or more chunks and keeps a cumulative, absolute-sample log
// of every output event, plus simple Note-On/Note-Off balance bookkeeping (REQ-9.3-9).
class ScenarioRunner {
public:
    explicit ScenarioRunner(SequencerConfiguration config) noexcept;

    [[nodiscard]] SequencerCore& core() noexcept { return core_; }
    void setConfig(const SequencerConfiguration& config) noexcept { config_ = config; }
    [[nodiscard]] const SequencerConfiguration& config() const noexcept { return config_; }

    // Feeds one chunk (one SequencerCore::processBlock call). `input.events`' sampleOffset
    // values are chunk-relative, as the real contract requires; the runner advances its own
    // sample cursor by `input.numSamples` after the call.
    BlockResult runChunk(const BlockInput& input);

    [[nodiscard]] const std::vector<CapturedEvent>& captured() const noexcept { return captured_; }
    [[nodiscard]] std::int64_t sampleCursor() const noexcept { return sampleCursor_; }

    // Every captured Note-On for a (channel, pitch) must be matched by a later-or-same-sample
    // Note-Off before another Note-On for the same key, and none may remain open at the end
    // (the guaranteed Note-Off property, sec. 9.3). Returns false on any unmatched Note-Off or
    // any key still open.
    [[nodiscard]] bool allNotesClosed() const noexcept;

private:
    SequencerCore core_;
    SequencerConfiguration config_;
    sequencing::OutputEventBuffer output_;
    std::vector<CapturedEvent> captured_;
    std::int64_t sampleCursor_ = 0;
};

}  // namespace phu::arpeggiator::test
