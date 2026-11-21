#include "HostTrace.h"

#include <algorithm>
#include <limits>
#include <locale>
#include <sstream>

namespace phu::plugin {

HostTraceRecorder::HostTraceRecorder(std::size_t capacityRecords) : ring_(std::max<std::size_t>(capacityRecords, 1)) {
}

void HostTraceRecorder::push(const HostTraceRecord& record) noexcept {
    if (overflow_ || pending_ - consumed_.load(std::memory_order_acquire) >= ring_.size()) {
        overflow_ = true;
        return;
    }
    ring_[static_cast<std::size_t>(pending_ % ring_.size())] = record;
    ++pending_;
}

void HostTraceRecorder::beginBlock(int numSamples, double sampleRate, bool bypassed,
                                   const arpeggiator::TransportInfo& transport) noexcept {
    pending_ = published_.load(std::memory_order_relaxed);
    overflow_ = false;
    HostTraceRecord record;
    record.kind = HostTraceRecord::Kind::block;
    record.value = numSamples;
    record.sampleRate = sampleRate;
    record.bypassed = bypassed;
    record.transport = transport;
    push(record);
}

void HostTraceRecorder::addMidi(HostTraceRecord::Kind kind, const juce::MidiBuffer& midi) noexcept {
    for (const auto metadata : midi) {
        HostTraceRecord record;
        record.kind = kind;
        record.value = metadata.samplePosition;
        record.size = static_cast<std::uint8_t>(std::clamp(metadata.numBytes, 0, 3));
        std::copy_n(metadata.data, record.size, record.bytes.begin());
        push(record);
    }
}

void HostTraceRecorder::endBlock() noexcept {
    if (overflow_) {
        droppedBlocks_.fetch_add(1, std::memory_order_relaxed);
        pending_ = published_.load(std::memory_order_relaxed);
        return;
    }
    published_.store(pending_, std::memory_order_release);
}

std::size_t HostTraceRecorder::drain(std::vector<HostTraceRecord>& out) {
    const std::uint64_t end = published_.load(std::memory_order_acquire);
    const std::uint64_t begin = consumed_.load(std::memory_order_relaxed);
    for (std::uint64_t i = begin; i < end; ++i) {
        out.push_back(ring_[static_cast<std::size_t>(i % ring_.size())]);
    }
    consumed_.store(end, std::memory_order_release);
    return static_cast<std::size_t>(end - begin);
}

std::uint64_t HostTraceRecorder::droppedBlocks() const noexcept {
    return droppedBlocks_.load(std::memory_order_relaxed);
}

namespace {

std::ostringstream makeStream() {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream.precision(std::numeric_limits<double>::max_digits10);
    return stream;
}

constexpr char hexDigits[] = "0123456789abcdef";

int hexValue(char c) noexcept {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool parseHex(const std::string& text, std::vector<std::uint8_t>& bytes) {
    if (text.empty() || text.size() % 2 != 0) {
        return false;
    }
    for (std::size_t i = 0; i < text.size(); i += 2) {
        const int high = hexValue(text[i]);
        const int low = hexValue(text[i + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        bytes.push_back(static_cast<std::uint8_t>(high * 16 + low));
    }
    return true;
}

}  // namespace

std::string hostTraceHeader() {
    return "# phu-sequential host trace v1\n"
           "# B numSamples sampleRate bypassed isPlaying hasPpq ppq bpm isLooping loopStart loopEnd\n"
           "# I/O offset hexBytes (host input / plugin output of the preceding B); S base64 state\n";
}

void appendHostTraceLine(const HostTraceRecord& record, std::string& out) {
    auto stream = makeStream();
    if (record.kind == HostTraceRecord::Kind::block) {
        const auto& t = record.transport;
        stream << "B " << record.value << ' ' << record.sampleRate << ' ' << int{record.bypassed} << ' '
               << int{t.isPlaying} << ' ' << int{t.hasMusicalPosition} << ' ' << t.playheadBeat << ' '
               << t.tempoBeatsPerMinute << ' ' << int{t.isLooping} << ' ' << t.loopStartBeat << ' ' << t.loopEndBeat;
    } else {
        stream << (record.kind == HostTraceRecord::Kind::input ? "I " : "O ") << record.value << ' ';
        for (std::uint8_t i = 0; i < record.size; ++i) {
            stream << hexDigits[record.bytes[i] >> 4] << hexDigits[record.bytes[i] & 0x0f];
        }
    }
    stream << '\n';
    out += stream.str();
}

void appendHostTraceStateLine(const juce::MemoryBlock& state, std::string& out) {
    out += "S ";
    out += state.toBase64Encoding().toStdString();
    out += '\n';
}

HostTraceParseResult parseHostTrace(const std::string& text) {
    HostTraceParseResult result;
    std::optional<juce::MemoryBlock> pendingState;
    std::istringstream lines(text);
    std::string line;
    int lineNumber = 0;
    while (std::getline(lines, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        std::istringstream fields(line);
        fields.imbue(std::locale::classic());
        std::string tag;
        fields >> tag;
        bool ok = true;
        if (tag == "B") {
            HostTraceBlock block;
            int bypassed = 0;
            int playing = 0;
            int hasPpq = 0;
            int looping = 0;
            auto& t = block.transport;
            fields >> block.numSamples >> block.sampleRate >> bypassed >> playing >> hasPpq >> t.playheadBeat >>
                t.tempoBeatsPerMinute >> looping >> t.loopStartBeat >> t.loopEndBeat;
            ok = !fields.fail();
            block.bypassed = bypassed != 0;
            t.isPlaying = playing != 0;
            t.hasMusicalPosition = hasPpq != 0;
            t.isLooping = looping != 0;
            block.stateBefore = std::move(pendingState);
            pendingState.reset();
            if (ok) {
                result.blocks.push_back(std::move(block));
            }
        } else if (tag == "I" || tag == "O") {
            HostTraceMidi midi;
            std::string hex;
            fields >> midi.offset >> hex;
            ok = !fields.fail() && !result.blocks.empty() && parseHex(hex, midi.bytes);
            if (ok) {
                auto& target = tag == "I" ? result.blocks.back().input : result.blocks.back().output;
                target.push_back(std::move(midi));
            }
        } else if (tag == "S") {
            std::string encoded;
            fields >> encoded;
            juce::MemoryBlock state;
            ok = state.fromBase64Encoding(juce::String(encoded));
            if (ok) {
                pendingState = std::move(state);
            }
        } else {
            ok = false;
        }
        if (!ok) {
            result.errorLine = lineNumber;
            return result;
        }
    }
    return result;
}

HostTraceFileWriter::HostTraceFileWriter(HostTraceRecorder& recorder, const juce::File& file, StateProvider stateProvider)
    : recorder_(recorder), file_(file), stateProvider_(std::move(stateProvider)) {
    (void)file_.getParentDirectory().createDirectory();
    (void)file_.replaceWithText(juce::String(hostTraceHeader()), false, false, "\n");
}

HostTraceFileWriter::~HostTraceFileWriter() {
    stopTimer();
    flush();
}

void HostTraceFileWriter::start(int intervalMilliseconds) {
    startTimer(intervalMilliseconds);
}

void HostTraceFileWriter::timerCallback() {
    flush();
}

void HostTraceFileWriter::flush() {
    std::string text;
    if (stateProvider_) {
        juce::MemoryBlock state;
        stateProvider_(state);
        if (!hasState_ || state != lastState_) {
            appendHostTraceStateLine(state, text);
            lastState_ = std::move(state);
            hasState_ = true;
        }
    }
    scratch_.clear();
    (void)recorder_.drain(scratch_);
    for (const auto& record : scratch_) {
        appendHostTraceLine(record, text);
    }
    const auto drops = recorder_.droppedBlocks();
    if (drops != reportedDrops_) {
        text += "# dropped blocks (ring full): " + std::to_string(drops - reportedDrops_) + "\n";
        reportedDrops_ = drops;
    }
    if (!text.empty()) {
        juce::FileOutputStream stream(file_);
        if (stream.openedOk()) {
            (void)stream.write(text.data(), text.size());
            stream.flush();
        }
    }
}

juce::File defaultHostTraceFile() {
    const auto directory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                               .getChildFile("phu-sequential")
                               .getChildFile("host-traces");
    const auto name = "trace-" + juce::Time::getCurrentTime().formatted("%Y%m%d-%H%M%S");
    return directory.getChildFile(name + ".txt").getNonexistentSibling(false);
}

}  // namespace phu::plugin
