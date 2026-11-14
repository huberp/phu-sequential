# phu-sequential — Architecture — 03-sequencing-modules.md

## 3. Reusable core modules (`lib/sequencing`, namespace `phu::sequencing`)

### 3.1 M01 Constants — `lib/sequencing/Constants.h` (#5)

The single central place for every numeric value (§16.3). No other file may contain a literal for any of these values (tests may). Grouped in nested namespaces; all `inline constexpr`.

```cpp
#pragma once
#include <array>
#include <cstdint>

namespace phu::sequencing::constants {

namespace midi {
inline constexpr int channelCount            = 16;
inline constexpr int firstChannel            = 1;
inline constexpr int lastChannel             = 16;
inline constexpr int lowestPitch             = 0;
inline constexpr int highestPitch            = 127;
inline constexpr int pitchCount              = 128;
inline constexpr int lowestVelocity          = 0;   // §7.2
inline constexpr int highestVelocity         = 127; // §7.2
inline constexpr int noteOffVelocity         = 0;   // §9.1
inline constexpr int sustainPedalController  = 64;  // §3.1
inline constexpr int sustainPedalOnThreshold = 64;  // value >= 64 = on (§3.1)
} // namespace midi

namespace velocity { // §7.1; velocity limits live only in midi:: (§16.3 "exactly one place")
inline constexpr int minimumDelta          = -127;
inline constexpr int maximumDelta          = 127;
inline constexpr int minimumPercent        = 0;
inline constexpr int maximumPercent        = 200;
inline constexpr int percentDivisor        = 100;
inline constexpr int defaultByStepValue    = 100; // D-5
inline constexpr int defaultDeltaValue     = 0;   // D-5
inline constexpr int defaultPercentValue   = 100; // D-5
} // namespace velocity

namespace lanes {
inline constexpr int laneCount               = 8;    // §4
inline constexpr int minimumStepCount        = 1;    // §5.4
inline constexpr int maximumStepCount        = 64;   // §5.4 (also text-grid max entries)
inline constexpr int defaultStepCount        = 16;   // §20
inline constexpr int minimumFollowIndex      = 0;    // §4.2
inline constexpr int maximumFollowIndex      = 15;   // §4.2
inline constexpr int defaultFollowIndex      = 0;    // §20
inline constexpr int defaultFollowChannel    = 1;    // D-33
inline constexpr int defaultOutputChannel    = 1;    // §8, §20
inline constexpr int defaultChordInputChannel= 1;    // §20
inline constexpr int minimumGatePercent      = 1;    // §6
inline constexpr int maximumGatePercent      = 200;  // §6
inline constexpr int defaultGatePercent      = 100;  // §6, §20
inline constexpr int gatePercentDivisor      = 100;  // §6
inline constexpr int minimumTranspose        = -24;  // §8
inline constexpr int maximumTranspose        = 24;   // §8
inline constexpr int defaultTranspose        = 0;    // §20
inline constexpr int minimumSwingPercent     = 0;    // §5.1
inline constexpr int maximumSwingPercent     = 100;  // §5.1
inline constexpr int defaultSwingPercent     = 0;    // §20
inline constexpr int swingPercentToShiftDivisor = 200; // 100 % -> shift = 1/2 step (§5.1)
inline constexpr int maximumVoicesPerLane    = 32;   // §9.4
} // namespace lanes

namespace timing {
struct RationalFactor { int numerator; int denominator; };
inline constexpr std::int64_t ticksPerBeat      = 960;  // D-3
inline constexpr std::int64_t beatsPerWholeNote = 4;
inline constexpr std::int64_t ticksPerWholeNote = ticksPerBeat * beatsPerWholeNote;
inline constexpr RationalFactor straightFactor{1, 1};
inline constexpr RationalFactor tripletFactor{2, 3};   // §16.2
inline constexpr RationalFactor dottedFactor{3, 2};    // §16.2
// Base-timing list (§5.1, §16.2): order = parameter choice order.
inline constexpr std::array<int, 6> baseTimingDenominators{32, 16, 8, 4, 2, 1};
inline constexpr int defaultBaseTimingIndex = 1;       // 1/16 (§20)
// Text grid (§5.2)
inline constexpr int textGridNumerator = 1;
inline constexpr std::array<int, 7> textGridDenominators{1, 2, 4, 8, 16, 32, 64};
inline constexpr int textGridMaximumEntries = 64;
inline constexpr char textGridEntrySeparator = ',';
inline constexpr char textGridFractionSeparator = '/';
inline constexpr char textGridTripletSuffix = 't';
inline constexpr char textGridDottedSuffix = '.';
// Tolerances (D-16)
inline constexpr double beatEpsilon = 1.0e-6;
inline constexpr double playheadJitterToleranceSamples = 1.0;
inline constexpr double sampleRoundingEpsilon = 1.0e-6;
inline constexpr double secondsPerMinute = 60.0;
} // namespace timing

namespace capacity { // D-30, §17
inline constexpr int maximumSoundingVoices   = midi::channelCount * midi::pitchCount; // 2048
inline constexpr int voiceOwnerCount         = lanes::laneCount + 1;                  // lanes + pass-through
inline constexpr int inputEventsPerChunk     = 4096;   // core input per core call; adapter streams in chunks, no per-host-block ceiling (D-30, D-51)
inline constexpr int samplesPerChunk         = 4096;   // max samples per core call (bounds timeline output per call, D-51)
inline constexpr int outputEventsPerBlock    = 16384;  // core output per core call; drained by the adapter after each call
inline constexpr int noteOffReserveEvents    = maximumSoundingVoices; // 2048 core-output slots only Note-Offs may use (D-30)
inline constexpr int outputMidiReserveBytes  = 1048576; // processor-owned output scratch juce::MidiBuffer, reserved in prepareToPlay (D-45)
inline constexpr int midiEventBytes          = 9;      // 3-byte channel message + JUCE header sizeof(int32)+sizeof(uint16)
inline constexpr int noteOffReserveBytes     = maximumSoundingVoices * midiEventBytes;     // 18432: core Note-Offs only (D-45)
inline constexpr int rawReleaseReserveBytes  = 3 * midi::channelCount * midiEventBytes;    // 432: CC64<64, CC120, CC123 per channel
inline constexpr int deferredEventCapacity   = 8192;   // zero-sample deferral store entries (D-53)
inline constexpr int deferredArenaBytes      = 65536;  // zero-sample deferral store bytes for non-release payloads (SysEx, D-53)
} // namespace capacity

namespace colors { // §14, §20; RGB 0xRRGGBB, lane order 1..8 (docs/ui/README.md)
inline constexpr std::array<std::uint32_t, lanes::laneCount> defaultLanePalette{
    0x4FC3F7 /*blue*/, 0x81C784 /*green*/, 0xBA68C8 /*purple*/, 0xF06292 /*pink*/,
    0x4DB6AC /*teal*/, 0x7986CB /*indigo*/, 0xDCE775 /*lime*/,  0xA1887F /*brown*/};
} // namespace colors

namespace editor { // §14; D-36, D-37 (pixels)
inline constexpr int minimumStepDisplayWidth = 16;
inline constexpr int laneRowHeight           = 140;
inline constexpr int globalBarHeight         = 56;
inline constexpr int controlStripWidth       = 148;  // lane control strip (docs/ui sketches)
inline constexpr int defaultWidth            = 1280;
inline constexpr int defaultHeight           = 820;
inline constexpr int minimumWidth            = 1000;
inline constexpr int minimumHeight           = 420;
inline constexpr int maximumWidth            = 3840;
inline constexpr int maximumHeight           = 2400;
inline constexpr int statusRefreshHz         = 30;
} // namespace editor

namespace state { // §13
inline constexpr int currentFormatVersion = 1;
} // namespace state

} // namespace phu::sequencing::constants
```

`static_assert`s in the same header: `ticksPerWholeNote % (64 * 3) == 0` and `(ticksPerWholeNote / 64) % 2 == 0` (all triplet/dotted lengths integral); `defaultLanePalette.size() == laneCount`.

### 3.2 M02 Musical time and note values — `MusicalTime.h`, `NoteValues.h` (#5)

```cpp
// MusicalTime.h
namespace phu::sequencing {
using Beats     = double;        // quarter-note beats (PPQ)
using TickCount = std::int64_t;  // constants::timing::ticksPerBeat per beat
[[nodiscard]] constexpr Beats ticksToBeats(TickCount ticks) noexcept;          // [RT]
[[nodiscard]] constexpr TickCount wholeBeatsToTicks(std::int64_t beats) noexcept; // [RT]
}

// NoteValues.h — data-driven; the only place mapping names <-> factors
namespace phu::sequencing {
enum class Subdivision : std::uint8_t { straight = 0, triplet = 1, dotted = 2 };
inline constexpr int subdivisionCount = 3;

struct SubdivisionDefinition {
    std::string_view name;            // "Straight" / "Triplet" / "Dotted" (parameter choice text)
    std::string_view persistenceKey;  // "straight" / "triplet" / "dotted" (state XML)
    constants::timing::RationalFactor factor;
    char textGridSuffix;              // '\0', 't', '.'
};
inline constexpr std::array<SubdivisionDefinition, subdivisionCount> subdivisionRegistry{{
    {"Straight", "straight", constants::timing::straightFactor, '\0'},
    {"Triplet",  "triplet",  constants::timing::tripletFactor,  constants::timing::textGridTripletSuffix},
    {"Dotted",   "dotted",   constants::timing::dottedFactor,   constants::timing::textGridDottedSuffix},
}};

struct NoteLength {                  // "1/denominator" modified by subdivision
    int denominator = 16;            // power of two, 1..64
    Subdivision subdivision = Subdivision::straight;
    friend constexpr bool operator==(const NoteLength&, const NoteLength&) noexcept = default;
};
// ticksPerWholeNote / denominator * factor.numerator / factor.denominator (exact integer)
[[nodiscard]] constexpr TickCount noteLengthTicks(NoteLength) noexcept;          // [RT]

struct BaseTimingDefinition { std::string_view name; int denominator; };      // name "1/16"
// Built from constants::timing::baseTimingDenominators; index = parameter choice index
[[nodiscard]] constexpr std::span<const BaseTimingDefinition> baseTimingRegistry() noexcept;
[[nodiscard]] constexpr NoteLength baseTimingNoteLength(int baseTimingIndex, Subdivision) noexcept; // index clamped
}
```

### 3.3 M03 Step timing — `StepTiming.h/.cpp` (#5)

Owns the grid model shared by base timing and text grid (§5.1, §5.2, §5.4), swing (§5.1) and gate (§6).

```cpp
namespace phu::sequencing {

struct GridLayout {
    int stepCount = 0;   // 0 = no grid (text grid without valid content, D-9) -> lane silent
    std::array<TickCount, constants::lanes::maximumStepCount>     stepLengthTicks{};
    std::array<TickCount, constants::lanes::maximumStepCount + 1> stepStartTicks{}; // [stepCount] = loop length
    bool swingApplies = false;  // true only for base timing (§5.1 "swing applies only to base timing")

    [[nodiscard]] bool isEmpty() const noexcept { return stepCount == 0; }
    [[nodiscard]] TickCount loopLengthTicks() const noexcept { return stepStartTicks[stepCount]; }
    [[nodiscard]] Beats loopLengthBeats() const noexcept;
    // Equality compares stepCount, swingApplies, and lengths of the first stepCount steps only.
    friend bool operator==(const GridLayout&, const GridLayout&) noexcept;
};

// Base timing (§5.1, §5.4): stepCount clamped to [1, 64]; swingApplies = true.
[[nodiscard]] GridLayout makeUniformGridLayout(NoteLength, int stepCount) noexcept;           // [RT]
// Text grid (§5.2): entries.size() <= 64; empty span -> empty layout; swingApplies = false.
[[nodiscard]] GridLayout makeGridLayout(std::span<const NoteLength> entries) noexcept;        // [RT]

// Swing (§5.1): ADVANCE (D-4: the step starts EARLIER) of steps with odd 0-based index (even 1-based);
// never step 0 of a loop. advance = stepLength * swingPercent / 200; 0 if !swingApplies or index even.
// swingPercent clamped [0,100]. Effective step start = grid start - advance.
[[nodiscard]] Beats swingAdvanceBeats(TickCount stepLengthTicks, int stepIndexInLoop,
                                      int swingPercent, bool swingApplies) noexcept;          // [RT]
// Gate (§6): stepLength * gatePercent / 100, gatePercent clamped [1,200]. Always > 0.
[[nodiscard]] Beats gatedLengthBeats(TickCount stepLengthTicks, int gatePercent) noexcept;    // [RT]
// Index of the step containing tick position p in [0, loopLength): largest k with start[k] <= p.
[[nodiscard]] int stepIndexAtTick(const GridLayout&, TickCount positionInLoop) noexcept;      // [RT], binary search
}
```

Rules: swing is a function of the step index **within the loop** (counting restarts at each loop, §5.1); with an odd step count the loop's last step (even 0-based index) is unswung and step 0 of the next loop is never shifted. Swing never changes `stepStartTicks` (grid positions unchanged; also not in display, §14).

Backward swing (D-4): the effective start of a swung step *k* (odd 0-based) is `e_k = g_k − len·swing/200`, which lies inside step *k−1* (base timing has uniform step lengths, so `e_k ≥ g_k − len/2 > g_{k−1}`; text grids never swing). Hence effective starts are strictly increasing within a loop, and because step 0 is never shifted, **swing never moves the loop start** (a swung last step of an even-count loop starts inside the second-to-last step, never before the loop start). Examples at 1/16 (len = 0.25 beat), swing 50 %: advance = 0.0625 beat; steps 0..3 of a loop at phase 0 start at 0, 0.1875, 0.5, 0.6875; the next loop's step 0 starts at 1.0 (unshifted). With swing 100 %: step 1 starts at 0.125 (middle of step 0). Gate is measured from the effective start (note length unchanged, §5.1).

### 3.4 M04 Sample conversion — `SampleConversion.h` (#5)

The single beats→samples boundary (§10, §16.2, §17). Pure, header-only.

```cpp
namespace phu::sequencing {
using SampleOffset = std::int32_t;

// sampleRate * 60 / bpm; returns 0 if bpm <= 0 or sampleRate <= 0 (caller treats as "no clock").
[[nodiscard]] constexpr double samplesPerBeat(double tempoBeatsPerMinute, double sampleRate) noexcept; // [RT]
[[nodiscard]] constexpr double beatsToSamples(Beats duration, double samplesPerBeat) noexcept;         // [RT]
[[nodiscard]] constexpr Beats  samplesToBeats(double samples, double samplesPerBeat) noexcept;         // [RT]
// D-15: floor((beat - blockStartBeat) * samplesPerBeat + sampleRoundingEpsilon), clamped to [0, numSamples-1]
// (returns 0 when numSamples <= 0).
[[nodiscard]] SampleOffset beatToSampleOffset(Beats beat, Beats blockStartBeat,
                                              double samplesPerBeat, int numSamples) noexcept;         // [RT]
}
```

### 3.5 M05 Text-grid parser — `TextGridParser.h/.cpp` (#6)

Runs on the message thread (may allocate for error lists); the *result* type `TextGrid` is fixed-capacity so it can travel to the audio thread.

```cpp
namespace phu::sequencing {

struct TextGrid {                                    // a VALID grid only
    int entryCount = 0;                              // 0..64; 0 = "no grid" (D-9)
    std::array<NoteLength, constants::timing::textGridMaximumEntries> entries{};
    [[nodiscard]] std::span<const NoteLength> activeEntries() const noexcept;
    friend bool operator==(const TextGrid&, const TextGrid&) noexcept; // first entryCount entries
};

enum class TextGridErrorReason : std::uint8_t {
    emptyText,           // whole text empty / whitespace only
    emptyEntry,          // ",," or trailing ","
    malformedEntry,      // not "<num>/<den>[suffix]"
    invalidNumerator,    // numerator token != "1"
    invalidDenominator,  // not one of 1,2,4,8,16,32,64
    unknownSuffix,       // suffix other than 't' or '.'
    conflictingSuffixes, // both / repeated suffixes, e.g. "1/8t." or "1/8tt"
    tooManyEntries       // entry index >= 64
};
struct TextGridError {
    int entryIndex;                 // 0-based entry index (0 for emptyText)
    std::size_t textBegin;          // byte range in the original text, for red marking (§5.3)
    std::size_t textEnd;            // exclusive
    TextGridErrorReason reason;
};
struct TextGridParseResult {
    std::optional<TextGrid> grid;          // set iff errors.empty()
    std::vector<TextGridError> errors;     // every invalid entry, ascending entryIndex
    [[nodiscard]] bool isValid() const noexcept { return grid.has_value(); }
};

[[nodiscard]] TextGridParseResult parseTextGrid(std::string_view text);   // [MT]
[[nodiscard]] std::string formatTextGrid(const TextGrid&);                // [MT] canonical "1/8t, 1/8., 1/4"
}
```

Grammar (D-8): `text := entry (',' entry)*`; `entry := ws* "1" "/" den suffix? ws*`; `den ∈ {"1","2","4","8","16","32","64"}` (exact tokens: no sign, no leading zeros, no inner whitespace); `suffix := 't' | '.'` (lowercase `t` only); `ws` = ASCII space or tab. A grid with any error is rejected as a whole (§5.3). `formatTextGrid(parse(x).grid)` re-parses to the same grid.

### 3.6 M06 MIDI events and output buffer — `MidiEvent.h` (#7), `OutputEventBuffer.h/.cpp` (#9)

```cpp
namespace phu::sequencing {

enum class MidiEventKind : std::uint8_t {
    noteOn,          // data1 = pitch, data2 = velocity (0 allowed on INPUT; normalized by consumers, §3.1)
    noteOff,         // data1 = pitch, data2 = release velocity
    controlChange    // data1 = controller, data2 = value (the arpeggiator core only receives CC64, D-31)
};
// Non-note events (other CCs, pitch bend, aftertouch, program change, SysEx, …) never enter the core:
// the adapter copies them from the host input straight to the output (D-31).

struct MidiEvent {
    SampleOffset sampleOffset = 0;
    MidiEventKind kind = MidiEventKind::noteOn;
    std::uint8_t channel = 1;        // 1..16
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;

    [[nodiscard]] constexpr bool isNoteOnWithVelocity() const noexcept;  // noteOn && data2 > 0
    [[nodiscard]] constexpr bool isNoteRelease() const noexcept;         // noteOff || (noteOn && data2 == 0)
    friend constexpr bool operator==(const MidiEvent&, const MidiEvent&) noexcept = default;
};
[[nodiscard]] constexpr MidiEvent makeNoteOn(SampleOffset, int channel, int pitch, int velocity) noexcept;
[[nodiscard]] constexpr MidiEvent makeNoteOff(SampleOffset, int channel, int pitch) noexcept; // velocity 0 (§9.1)
// Note-On with velocity 0 -> noteOff kind (release velocity 0); other events unchanged (D-26).
[[nodiscard]] constexpr MidiEvent normalizeNoteEvent(const MidiEvent&) noexcept;

class OutputEventBuffer {                         // D-30
public:
    explicit OutputEventBuffer(std::size_t capacity = constants::capacity::outputEventsPerBlock); // [MT] allocates once
    void clear() noexcept;                                                    // [RT]
    // Fixed partition (D-30): the last noteOffReserveEvents (2048) slots are usable by Note-Offs only.
    // Note-Offs: always accepted while size() < capacity(). Proof: a Note-On is admitted only if afterwards
    // ≥ 2048 slots stay free; sounding voices ≤ maximumSoundingVoices = 2048 (lane voices ≤ 8 × 32 = 256 plus
    // pass-through voices, one registry slot per (channel, pitch)), and every later Note-Off ends one of them,
    // so the Note-Offs of all voices sounding at any time fit. Returns false only on an asserted logic error.
    bool pushNoteOff(const MidiEvent&) noexcept;                              // [RT]
    // Note-Ons (lane, pass-through, bypassed raw notes): accepted only if size()+1 <= capacity() - 2048;
    // otherwise refused and counted, and the caller does not start the voice. Per core call the bound of §5.2
    // (13 376 events) is below the 14 336 general slots, so a refusal cannot occur (D-30).
    [[nodiscard]] bool tryPushNoteOn(const MidiEvent&) noexcept;              // [RT]
    [[nodiscard]] std::span<const MidiEvent> events() const noexcept;         // non-decreasing sampleOffset
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::uint32_t droppedEventCount() const noexcept;           // since construction
private:
    std::vector<MidiEvent> storage_;  // sized in constructor, never resized
    std::size_t used_ = 0;
    std::uint32_t dropped_ = 0;
};
}
```

Invariant (relied on by M17): output events are appended in processing order, which is chronological, so `events()` is sorted by `sampleOffset` (non-decreasing); same-offset order is meaningful (§5.3 of this doc) and must be preserved by the adapter.

### 3.7 M07 Held-note tracker — `HeldNoteTracker.h/.cpp` (#7)

Live held-note state for all 16 channels (§3.1, §3.2, §4.3, §10 "sources always reflect the live held-note state"). Tracks every channel, so changing the chord input channel or a follow channel takes effect on already-held keys.

```cpp
namespace phu::sequencing {

struct HeldNote { std::uint8_t pitch; std::uint8_t velocity; 
                  friend constexpr bool operator==(const HeldNote&, const HeldNote&) noexcept = default; };

class HeldNoteTracker {
public:
    void reset() noexcept;                                                        // [RT] nothing held, pedals up
    // Applies Note-On (v>0), Note-Off / Note-On v=0, CC64. Other events: ignored, returns false.
    // Returns true iff the held set or the most-recent note of the event's channel changed.
    bool apply(const MidiEvent& event) noexcept;                                  // [RT] O(1)
    [[nodiscard]] bool isHeld(int channel, int pitch) const noexcept;             // key down OR sustained
    [[nodiscard]] int heldCount(int channel) const noexcept;
    // Sorted ascending by pitch, index 0 = lowest (§3.4); nullopt if index >= heldCount.
    [[nodiscard]] std::optional<HeldNote> heldNoteAtIndex(int channel, int sortedIndex) const noexcept; // O(128)
    // Most recent Note-On on the channel, only while that pitch is still held (no note stack, §4.3).
    [[nodiscard]] std::optional<HeldNote> mostRecentNote(int channel) const noexcept;
    [[nodiscard]] bool isSustainPedalDown(int channel) const noexcept;
private:
    struct ChannelState {
        std::bitset<constants::midi::pitchCount> keyDown;
        std::bitset<constants::midi::pitchCount> sustained;   // released while pedal down
        std::array<std::uint8_t, constants::midi::pitchCount> velocity{};
        int mostRecentPitch = -1;
        bool pedalDown = false;
    };
    std::array<ChannelState, constants::midi::channelCount> channels_{};
};
}
```

Rules: Note-On(p,v>0): `keyDown[p]=1`, `velocity[p]=v` (replaces, one entry per pitch), `mostRecentPitch=p`. Release(p): `keyDown[p]=0`; if pedal down `sustained[p]=1`. CC64 ≥ 64: pedal down. CC64 < 64: pedal up, `sustained` cleared (notes not key-held leave). Held = `keyDown | sustained`. Out-of-range channel/pitch in events: ignored (§17 validation).

### 3.8 M08 Source selection (input modes as strategies) — `sources/*` (#7)

"Index determination" (§16.1, §16.4) is the per-input-mode strategy that turns held notes into a lane's source note. One unit per mode; one registry.

```cpp
// sources/SourceSelection.h
namespace phu::sequencing {
enum class InputMode : std::uint8_t { chordFollow = 0, channel = 1 };   // order == registry order
inline constexpr int inputModeCount = 2;

struct SourceSelector {                 // committed lane selection + global chord channel
    InputMode inputMode = InputMode::chordFollow;
    int followIndex = constants::lanes::defaultFollowIndex;          // chordFollow
    int followChannel = constants::lanes::defaultFollowChannel;      // channel
    int chordInputChannel = constants::lanes::defaultChordInputChannel; // global (§3.2)
    friend constexpr bool operator==(const SourceSelector&, const SourceSelector&) noexcept = default;
};
using SourceNote = HeldNote;            // pitch + velocity of the followed input note

using SelectSourceFunction = std::optional<SourceNote> (*)(const HeldNoteTracker&, const SourceSelector&) noexcept;
struct InputModeDefinition {
    std::string_view name;              // parameter choice text: "Chord Follow", "Channel"
    std::string_view persistenceKey;    // "chordFollow", "channel"
    SelectSourceFunction select;
};

[[nodiscard]] std::optional<SourceNote> selectSource(const HeldNoteTracker&, const SourceSelector&) noexcept; // [RT] registry lookup

enum class SourceChange : std::uint8_t { unchanged, started, pitchChanged, released };
// none->none: unchanged; none->note: started; note->none: released (§4.4, §9.2);
// note->note with different pitch: pitchChanged (re-sorting or new note, §4.2/§4.4); same pitch: unchanged.
[[nodiscard]] constexpr SourceChange classifySourceChange(std::optional<SourceNote> previous,
                                                          std::optional<SourceNote> current) noexcept; // [RT]
}
// sources/ChordFollowSource.h — heldNoteAtIndex(chordInputChannel, followIndex) (§4.2)
[[nodiscard]] std::optional<SourceNote> selectChordFollowSource(const HeldNoteTracker&, const SourceSelector&) noexcept;
// sources/ChannelSource.h — mostRecentNote(followChannel) (§4.3)
[[nodiscard]] std::optional<SourceNote> selectChannelSource(const HeldNoteTracker&, const SourceSelector&) noexcept;
// sources/InputModeRegistry.h — THE registration place (§16.4)
inline constexpr std::array<InputModeDefinition, inputModeCount> inputModeRegistry{{
    {"Chord Follow", "chordFollow", &selectChordFollowSource},
    {"Channel",      "channel",     &selectChannelSource},
}};
```

Adding a mode = new unit file + one registry line + one enum value; parameter choices, state keys and UI lists are derived from the registry (no `switch` over modes anywhere).

### 3.9 M09 Velocity modes (strategies) — `velocity/*` (#8)

```cpp
// velocity/VelocityMode.h
namespace phu::sequencing {
enum class VelocityMode : std::uint8_t { incoming = 0, byStep = 1, deltaByStep = 2, relative = 3 }; // == registry order
inline constexpr int velocityModeCount = 4;

struct StepValueRange { int minimum; int maximum; int defaultValue; };
// Returns the UNCLAMPED velocity; clamping/zero handling is common code (§7.2).
using ComputeVelocityFunction = int (*)(int stepValue, int inputVelocity) noexcept;
struct VelocityModeDefinition {
    std::string_view name;            // "Incoming", "By Step", "Delta by Step", "Relative" (choice text)
    std::string_view persistenceKey;  // "incoming", "byStep", "deltaByStep", "relative"
    bool usesStepValues;              // false for incoming (§7.1; drag bars disabled, §14)
    StepValueRange stepValueRange;    // drives clamping, defaults, UI drag range
    ComputeVelocityFunction compute;
};
}
// velocity/IncomingVelocity.h      compute = inputVelocity                         range {0,0,0}, usesStepValues=false
// velocity/ByStepVelocity.h        compute = stepValue                             range {0,127,100}
// velocity/DeltaByStepVelocity.h   compute = inputVelocity + stepValue             range {-127,127,0}
// velocity/RelativeVelocity.h      compute = (inputVelocity*stepValue + 50) / 100  range {0,200,100}  (D-6)
// velocity/VelocityModeRegistry.h
namespace phu::sequencing {
extern const std::array<VelocityModeDefinition, velocityModeCount> velocityModeRegistry; // or inline constexpr
[[nodiscard]] const VelocityModeDefinition& velocityModeDefinition(VelocityMode) noexcept;     // [RT]
[[nodiscard]] int clampStepValue(VelocityMode, int value) noexcept;                             // [RT]
// clamp(compute(...), 0, 127); 0 -> nullopt ("not sent", §7.2); else 1..127.
[[nodiscard]] std::optional<std::uint8_t> computeOutputVelocity(VelocityMode, int stepValue, int inputVelocity) noexcept; // [RT]
}
```

All ranges reference `constants::velocity` / `constants::midi` (no literals). Mode enum out of range → treated as `incoming` via clamped index (§17).

### 3.10 M10 Step data — `StepData.h/.cpp` (#8)

One array per lane, shared by both timing sources (§5.2), fixed capacity 64, values stored per velocity mode (§7).

```cpp
namespace phu::sequencing {
struct StepData {
    bool enabled = true;                                           // §5.4/§20: new steps are on
    std::array<std::int16_t, velocityModeCount> valueByMode{};     // indexed by VelocityMode; incoming slot unused (0)
    friend constexpr bool operator==(const StepData&, const StepData&) noexcept = default;
};
[[nodiscard]] constexpr StepData defaultStepData() noexcept;       // enabled, each mode's defaultValue

class StepPattern {
public:
    StepPattern() noexcept;                                         // 64 x defaultStepData()
    [[nodiscard]] const StepData& step(int index) const noexcept;   // index clamped [0,63]
    void setEnabled(int index, bool enabled) noexcept;              // out-of-range index ignored
    void setValue(int index, VelocityMode, int value) noexcept;     // clampStepValue; incoming ignored
    // Positional transfer (§5.2, §5.4): data at index i stays at i; steps >= newCount reset to defaults
    // when newCount < oldCount ("discarded, not restored"); new steps already hold defaults (invariant).
    void applyStepCountChange(int oldCount, int newCount) noexcept;
    friend bool operator==(const StepPattern&, const StepPattern&) noexcept = default;
private:
    std::array<StepData, constants::lanes::maximumStepCount> steps_;
};
}
```

Invariant **I-1**: every slot at index ≥ the lane's effective step count holds `defaultStepData()` — **as seen by the audio thread**, and in `PatternDocument` once it has acknowledged all reported discards. Hence "increase" needs no action, and "decrease" resets the tail. The core guarantees I-1 at read time (it never reads slots ≥ its committed step count and masks discarded-but-unacknowledged slots, D-32 revisioned hand-off, §4.3); the document applies reported discards and loaders enforce it on load.

### 3.11 M11 Voice registry — `VoiceRegistry.h/.cpp` (#9)

Central ownership of every sounding output note, across all lanes and pass-through (§9.1, §9.3, §9.4, §3.3). At most one sounding voice per (output channel, pitch) — guaranteed by the retrigger policy — so storage is a fixed 16×128 slot table.

```cpp
namespace phu::sequencing {
using VoiceId = std::uint32_t;                 // unique, monotonically increasing; 0 = invalid; wraps skipping 0
inline constexpr VoiceId invalidVoiceId = 0;
using OwnerId = std::uint8_t;                  // arpeggiator: lanes 0..7, pass-through = laneCount (8)
inline constexpr Beats noScheduledEnd = std::numeric_limits<Beats>::infinity();

struct VoiceStartRequest {
    OwnerId owner;
    int channel;                               // 1..16 (output channel)
    int pitch;                                 // 0..127 (already transposed and range-checked by caller)
    int velocity;                              // 1..127
    Beats scheduledEndBeat = noScheduledEnd;   // lanes: absolute beat of Note-Off; pass-through: none
};
struct VoiceHandle {                           // returned by startVoice; identifies one voice instance
    VoiceId id = invalidVoiceId;
    std::uint16_t slot = 0;                    // (channel-1)*128 + pitch
    [[nodiscard]] constexpr bool isValid() const noexcept { return id != invalidVoiceId; }
};
struct ScheduledVoiceEnd { Beats beat; VoiceHandle voice; };

class VoiceRegistry {
public:
    VoiceRegistry() noexcept;                                            // no voices, all owners unlimited
    void reset() noexcept;                                               // [RT] forget all WITHOUT emitting (used only after a flush)
    void setVoiceLimit(OwnerId, int maximumVoices) noexcept;             // 0 = unlimited; lanes: 32 (§9.4)
    // Emits, in this order (D-29): (1) Note-Off of the voice currently sounding on (channel,pitch), ownership
    // transferred (§9.1); (2) if owner still at its limit: Note-Off of the owner's OLDEST voice (§9.4);
    // (3) Note-On. Returns an invalid handle (and emits nothing) if the output buffer refuses the Note-On.
    [[nodiscard]] VoiceHandle startVoice(const VoiceStartRequest&, SampleOffset, OutputEventBuffer&) noexcept; // [RT]
    // Registers a note already sounding downstream (bypass exit, D-27); emits nothing; same eviction rules.
    [[nodiscard]] VoiceHandle adoptVoice(const VoiceStartRequest&) noexcept;                                   // [RT]
    // Emits Note-Off (velocity 0) iff the voice is still sounding and owned; otherwise suppressed (§9.1). 
    bool endVoice(VoiceHandle, SampleOffset, OutputEventBuffer&) noexcept;                                     // [RT]
    int releaseOwner(OwnerId, SampleOffset, OutputEventBuffer&) noexcept;    // all voices of owner, oldest first
    int releaseAll(SampleOffset, OutputEventBuffer&) noexcept;               // all voices, oldest first
    // Earliest finite scheduledEndBeat; ties -> earlier start first. O(1): top of endHeap_ (see below).
    [[nodiscard]] std::optional<ScheduledVoiceEnd> nextScheduledEnd() const noexcept;                     // [RT] O(1)
    [[nodiscard]] bool isSounding(VoiceHandle) const noexcept;
    [[nodiscard]] int soundingCount() const noexcept;
    [[nodiscard]] int soundingCount(OwnerId) const noexcept;
    [[nodiscard]] std::optional<OwnerId> ownerOf(int channel, int pitch) const noexcept;
private:
    struct Slot { VoiceId id = invalidVoiceId; OwnerId owner = 0; Beats endBeat = noScheduledEnd;
                  std::uint16_t previousInOwner, nextInOwner, previousGlobal, nextGlobal; };
    std::array<Slot, constants::capacity::maximumSoundingVoices> slots_;   // index = (channel-1)*128 + pitch
    // intrusive lists (start order) per owner and global; per-owner counts and limits
    std::array<std::uint16_t, constants::capacity::voiceOwnerCount> ownerHead_, ownerTail_;
    std::array<int, constants::capacity::voiceOwnerCount> ownerCount_{}, ownerLimit_{};
    std::uint16_t globalHead_, globalTail_;
    // Indexed binary min-heap of the slots that carry a finite end, key (endBeat, startSequence).
    // heapPosition_[slot] = index in endHeap_ or npos; capacity = maximumSoundingVoices (one entry per slot,
    // no stale entries, never overflows).
    std::array<std::uint16_t, constants::capacity::maximumSoundingVoices> endHeap_, heapPosition_;
    std::array<std::uint64_t, constants::capacity::maximumSoundingVoices> startSequence_;
    int endHeapSize_ = 0;
    std::uint64_t nextStartSequence_ = 0;
    VoiceId nextVoiceId_ = 1;
    int soundingCount_ = 0;
};
}
```

`VoiceHandle` makes `endVoice`/`isSounding` O(1): the slot index is `(channel-1)*128 + pitch`; a handle is stale (→ suppressed) iff `slots_[slot].id != handle.id`. Owner ids ≥ `voiceOwnerCount`, channels/pitches out of range are rejected (§17).

Scheduled ends (replaces an O(N·2048) slot scan): every voice with a finite end is in the indexed min-heap `endHeap_`; `startVoice`/`adoptVoice` insert (sift-up), every way a voice stops sounding (end, retrigger, eviction, `releaseOwner`, `releaseAll`, `reset`) removes it by `heapPosition_` (swap with last, sift). Complexity: `nextScheduledEnd()` O(1); insert/remove O(log 2048) ≤ 11 levels; `releaseAll`/`reset` O(sounding). Since at most one voice per slot sounds, the heap never holds more than 2048 entries. The merged loop of §5.2 is therefore O((inputs + timeline items) · log 2048) per core call.

### 3.12 M18 Triple buffer — `TripleBuffer.h` (#12)

Wait-free single-producer/single-consumer "latest value" hand-off (§17). Used for pattern data (message → audio) and playback status (audio → message). Header-only template; `T` must be trivially copyable or at least nothrow-copy-assignable without allocation.

```cpp
namespace phu::sequencing {
template <typename T>
class TripleBuffer {
public:
    TripleBuffer() = default;                         // three value-initialized T (allocated with the owner)
    explicit TripleBuffer(const T& initial);          // all three slots = initial
    // Producer side (exactly one thread at a time; PatternStore serializes producers with its mutex).
    [[nodiscard]] T& writeBuffer() noexcept;          // fill in place, then publish()
    void publish() noexcept;                          // swaps back<->middle, sets "fresh" bit (release)
    void write(const T& value) noexcept { writeBuffer() = value; publish(); }
    // Consumer side (exactly one thread). Returns true if a newer value was taken.
    bool update() noexcept;                           // swaps front<->middle if fresh (acq_rel)
    [[nodiscard]] const T& read() const noexcept;     // current front, stable until next update()
private:
    std::array<T, 3> slots_{};
    std::atomic<std::uint8_t> middleAndFresh_{1};     // bits 0-1 middle index, bit 2 fresh
    std::uint8_t backIndex_ = 0, frontIndex_ = 2;
    static_assert(std::atomic<std::uint8_t>::is_always_lock_free);
};
}
```

All operations are [RT] (`noexcept`, O(1), no allocation, a single atomic exchange).

---

