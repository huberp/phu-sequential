#include "BlockEngine.h"
#include "HostTransport.h"
#include "arpeggiator/BlockTypes.h"
#include "sequencing/OutputEventBuffer.h"
#include "sequencing/SampleConversion.h"

#include <algorithm>
#include <array>

namespace phu::plugin {

namespace {
namespace capacity = sequencing::constants::capacity;
}  // namespace

BlockEngine::BlockEngine() noexcept : core_(), outputOpen_() {
}

void BlockEngine::prepare() noexcept {
    core_.reset();
    outputOpen_.reset();
    deferredInput_.clear();
    outputScratch_.clear();
    outputScratch_.ensureSize(capacity::outputMidiReserveBytes);
    knownHostCapacityBytes_ = 0;
}

void BlockEngine::process(juce::MidiBuffer& midi, int numSamples, const arpeggiator::TransportInfo& transportInfo,
                          double sampleRate, bool bypassed, const arpeggiator::SequencerConfiguration& config) noexcept {
    // Step 0 (D-53): a zero-sample host block never touches the core or the output writer; every
    // host event is deferred (in input order) for delivery at the next non-empty block.
    if (numSamples == 0) {
        for (const auto metadata : midi) {
            deferredInput_.insert(metadata);
        }
        midi.clear();
        return;
    }

    OutputMidiWriter writer(outputScratch_, outputOpen_);
    writer.beginBlock();

    // Pass 1 (raw, D-31): deferred events first (offset 0), then the host buffer, in input order.
    // Non-note events are never filtered by Thru/Consume/bypass and never enter a core queue.
    deferredInput_.forEach([&writer](const std::uint8_t* data, int size) {
        const juce::MidiMessageMetadata metadata(data, size, 0);
        if (routeOf(metadata) != HostEventRoute::coreOnly) {
            writer.appendRaw(metadata, 0);
        }
    });
    for (const auto metadata : midi) {
        if (routeOf(metadata) != HostEventRoute::coreOnly) {
            writer.appendRaw(metadata, metadata.samplePosition);
        }
    }

    // Pass 2 (chunks, D-51): deliver only core-routed events (note, CC64) to the core.
    const double samplesPerBeat = (transportInfo.hasMusicalPosition && transportInfo.tempoBeatsPerMinute > 0.0)
                                      ? sampleRate * 60.0 / transportInfo.tempoBeatsPerMinute
                                      : 0.0;
    // D-59: a looping host may jump back to the loop start inside this block; cut a chunk there so
    // the core sees the jump at the exact sample instead of at the next host block.
    const int wrapSample = (transportInfo.isPlaying && transportInfo.isLooping && samplesPerBeat > 0.0)
        ? sequencing::loopWrapSampleOffset(transportInfo.playheadBeat, transportInfo.loopStartBeat,
                                           transportInfo.loopEndBeat, samplesPerBeat, numSamples)
        : -1;
    ChunkedInputReader reader(deferredInput_, midi, numSamples, wrapSample);

    ChunkedInputReader::ChunkDescriptor descriptor{};
    while (reader.next(std::span<sequencing::MidiEvent, capacity::inputEventsPerChunk>(inputChunk_), descriptor)) {
        arpeggiator::BlockInput input;
        input.events = std::span<const sequencing::MidiEvent>(inputChunk_.data(),
                                                               static_cast<std::size_t>(descriptor.eventCount));
        input.transport = transportInfo;
        if (transportInfo.hasMusicalPosition && samplesPerBeat > 0.0) {
            if (wrapSample >= 0 && descriptor.startSample >= wrapSample) {
                input.transport.playheadBeat = transportInfo.loopStartBeat +
                    static_cast<double>(descriptor.startSample - wrapSample) / samplesPerBeat;
            } else {
                input.transport.playheadBeat += static_cast<double>(descriptor.startSample) / samplesPerBeat;
            }
        }
        input.sampleRate = sampleRate;
        input.numSamples = descriptor.numSamples;
        input.bypassed = bypassed;
        input.moreInputAtSameOffset = descriptor.moreInputAtSameOffset;

        coreOutput_.clear();
        core_.processBlock(input, config, coreOutput_);
        writer.appendCoreEvents(coreOutput_.events(), descriptor.startSample);
    }

    // Step 5 (copy to host, D-45/D-54). `outputScratch_.data.size()` is the real number of bytes
    // written this block (public juce::Array API); `needed` can never exceed
    // outputMidiReserveBytes because OutputMidiWriter enforces that budget itself.
    const auto needed = static_cast<std::size_t>(outputScratch_.data.size());
    midi.clear();
    if (needed > knownHostCapacityBytes_) {
        // juce::Array<uint8> (the storage behind juce::MidiBuffer) does not expose its real
        // allocated capacity through any public API in this JUCE version, so growth is tracked
        // against our own best-known lower bound rather than the literal `data.getNumAllocated()`
        // from the architecture text (see also the note in MidiConversion.cpp). ensureSize() is a
        // no-op if the host buffer already has enough real capacity.
        midi.ensureSize(needed);
        ++hostOutputGrowthCount_;
        knownHostCapacityBytes_ = needed;
    }
    midi.addEvents(outputScratch_, 0, -1, 0);

    droppedOutputEventsTotal_ += writer.droppedEventCount() + deferredInput_.droppedEventCount();
    deferredInput_.clear();
}

std::uint32_t BlockEngine::droppedOutputEventsTotal() const noexcept {
    return droppedOutputEventsTotal_;
}

std::uint32_t BlockEngine::hostOutputGrowthCount() const noexcept {
    return hostOutputGrowthCount_;
}

arpeggiator::SequencerCore& BlockEngine::core() noexcept {
    return core_;
}

}  // namespace phu::plugin
