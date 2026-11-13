#include "TransportTracker.h"
#include "sequencing/Constants.h"

namespace phu::arpeggiator {

void TransportTracker::reset() noexcept {
    wasPlaying_ = false;
    hasHistory_ = false;
    expectedNextBeat_ = 0;
}

TransportUpdate TransportTracker::update(const TransportInfo& info, double sampleRate, int numSamples,
                                        bool advanceExpectation) noexcept {
    using namespace sequencing::constants;
    
    bool isPlaying = info.isPlaying && info.hasMusicalPosition && info.tempoBeatsPerMinute > 0;
    
    if (!isPlaying) {
        if (wasPlaying_) {
            wasPlaying_ = false;
            return TransportUpdate{TransportTransition::stopped, {}};
        }
        return TransportUpdate{TransportTransition::idle, {}};
    }
    
    double samplesPerBeat = sampleRate * timing::secondsPerMinute / info.tempoBeatsPerMinute;
    double beatDurationOfBlock = numSamples / samplesPerBeat;
    
    if (!hasHistory_) {
        hasHistory_ = true;
        wasPlaying_ = true;
        expectedNextBeat_ = info.playheadBeat + beatDurationOfBlock;
        return TransportUpdate{
            TransportTransition::started,
            BlockClock{
                info.playheadBeat,
                info.playheadBeat,
                info.playheadBeat + beatDurationOfBlock,
                samplesPerBeat,
                numSamples
            }
        };
    }
    
    wasPlaying_ = true;
    
    sequencing::Beats beatTolerance = beatDurationOfBlock;
    sequencing::Beats deviation = info.playheadBeat - expectedNextBeat_;
    
    if (deviation < -sequencing::constants::timing::beatEpsilon || deviation > beatTolerance) {
        sequencing::Beats windowEnd = info.playheadBeat + beatDurationOfBlock;
        if (advanceExpectation) {
            expectedNextBeat_ = windowEnd;
        }
        return TransportUpdate{
            TransportTransition::discontinuity,
            BlockClock{
                info.playheadBeat,
                info.playheadBeat,
                windowEnd,
                samplesPerBeat,
                numSamples
            }
        };
    }
    
    sequencing::Beats windowStart = expectedNextBeat_;
    sequencing::Beats windowEnd = expectedNextBeat_ + beatDurationOfBlock;
    if (advanceExpectation) {
        expectedNextBeat_ = windowEnd;
    }
    
    return TransportUpdate{
        TransportTransition::continuous,
        BlockClock{
            info.playheadBeat,
            windowStart,
            windowEnd,
            samplesPerBeat,
            numSamples
        }
    };
}

}
