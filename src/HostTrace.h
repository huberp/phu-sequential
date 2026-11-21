#pragma once

#include "arpeggiator/TransportTracker.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

// M28 (D-61): debug host trace. Records what the host hands to the plugin per block (size, sample rate,
// bypass, transport incl. loop points, MIDI in) and what the plugin returns (MIDI out), so real host
// sessions can be inspected and replayed deterministically in tests. Wired into the plugin only when
// built with PHU_ENABLE_HOST_TRACE=ON.
namespace phu::plugin {

struct HostTraceRecord {
    enum class Kind : std::uint8_t { block, input, output };
    Kind kind = Kind::block;
    std::uint8_t size = 0;                    // MIDI bytes stored (events longer than 3 bytes are truncated)
    std::array<std::uint8_t, 3> bytes{};
    bool bypassed = false;
    std::int32_t value = 0;                   // block: numSamples; input/output: sample offset
    double sampleRate = 0;
    arpeggiator::TransportInfo transport{};
};

// Single-producer (audio thread) / single-consumer (message thread) ring. A block is published only
// when complete (endBlock); a block that does not fit is dropped as a whole and counted.
class HostTraceRecorder {
public:
    explicit HostTraceRecorder(std::size_t capacityRecords);  // [MT] allocates

    void beginBlock(int numSamples, double sampleRate, bool bypassed,
                    const arpeggiator::TransportInfo& transport) noexcept;            // [RT]
    void addMidi(HostTraceRecord::Kind kind, const juce::MidiBuffer& midi) noexcept;  // [RT]
    void endBlock() noexcept;                                                         // [RT]

    // [MT] Moves all published records to `out` (appended); returns the number moved.
    std::size_t drain(std::vector<HostTraceRecord>& out);
    [[nodiscard]] std::uint64_t droppedBlocks() const noexcept;

private:
    void push(const HostTraceRecord& record) noexcept;

    std::vector<HostTraceRecord> ring_;
    std::atomic<std::uint64_t> published_{0};  // written by the producer
    std::atomic<std::uint64_t> consumed_{0};   // written by the consumer
    std::uint64_t pending_ = 0;                // producer-local write position of the open block
    bool overflow_ = false;
    std::atomic<std::uint64_t> droppedBlocks_{0};
};

// Text format v1 (one record per line, locale independent, doubles round-trip exactly):
//   B <numSamples> <sampleRate> <bypassed> <isPlaying> <hasPpq> <ppq> <bpm> <isLooping> <loopStart> <loopEnd>
//   I <offset> <hex bytes>      host MIDI input of the preceding B
//   O <offset> <hex bytes>      plugin MIDI output of the preceding B
//   S <base64>                  plugin state (getStateInformation) valid from the next B on
//   # ...                       comment
[[nodiscard]] std::string hostTraceHeader();                              // [MT]
void appendHostTraceLine(const HostTraceRecord& record, std::string& out);  // [MT]
void appendHostTraceStateLine(const juce::MemoryBlock& state, std::string& out);  // [MT]

struct HostTraceMidi {
    std::int32_t offset = 0;
    std::vector<std::uint8_t> bytes;
    bool operator==(const HostTraceMidi&) const = default;
};

struct HostTraceBlock {
    int numSamples = 0;
    double sampleRate = 0;
    bool bypassed = false;
    arpeggiator::TransportInfo transport{};
    std::vector<HostTraceMidi> input;
    std::vector<HostTraceMidi> output;
    std::optional<juce::MemoryBlock> stateBefore;  // state to apply before this block
};

struct HostTraceParseResult {
    std::vector<HostTraceBlock> blocks;
    int errorLine = 0;  // 1-based line of the first malformed line, 0 if none
};

[[nodiscard]] HostTraceParseResult parseHostTrace(const std::string& text);  // [MT]

// [MT] Drains a recorder into a trace file: header at construction, then on every flush() the new
// records, a "# dropped" note when blocks were lost and an S line whenever the plugin state changed.
class HostTraceFileWriter : private juce::Timer {
public:
    using StateProvider = std::function<void(juce::MemoryBlock&)>;

    HostTraceFileWriter(HostTraceRecorder& recorder, const juce::File& file, StateProvider stateProvider);
    ~HostTraceFileWriter() override;

    void start(int intervalMilliseconds);  // periodic flush on the message thread
    void flush();
    [[nodiscard]] const juce::File& file() const noexcept { return file_; }

private:
    void timerCallback() override;

    HostTraceRecorder& recorder_;
    juce::File file_;
    StateProvider stateProvider_;
    juce::MemoryBlock lastState_;
    bool hasState_ = false;
    std::uint64_t reportedDrops_ = 0;
    std::vector<HostTraceRecord> scratch_;
};

// <user documents>/phu-sequential/host-traces/trace-<yyyymmdd-hhmmss>.txt (a non-existent name).
[[nodiscard]] juce::File defaultHostTraceFile();  // [MT]

}  // namespace phu::plugin
