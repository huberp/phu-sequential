#pragma once

#include "BlockClock.h"
#include "sequencing/MusicalTime.h"
#include <cstdint>

namespace phu::arpeggiator {

struct TransportInfo {
    bool isPlaying = false;
    bool hasMusicalPosition = false;
    sequencing::Beats playheadBeat = 0;
    double tempoBeatsPerMinute = 0;
    // Host loop (cycle) region; used by the adapter to cut a host block at the wrap (D-59), ignored by the core.
    bool isLooping = false;
    sequencing::Beats loopStartBeat = 0;
    sequencing::Beats loopEndBeat = 0;
};

enum class TransportTransition : std::uint8_t {
    idle,
    started,
    stopped,
    continuous,
    discontinuity
};

struct TransportUpdate {
    TransportTransition transition;
    BlockClock clock;
};

class TransportTracker {
public:
    void reset() noexcept;
    [[nodiscard]] TransportUpdate update(const TransportInfo& info, double sampleRate, int numSamples,
                                         bool advanceExpectation = true) noexcept;

private:
    bool wasPlaying_ = false;
    bool hasHistory_ = false;
    sequencing::Beats expectedNextBeat_ = 0;
};

}
