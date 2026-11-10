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
inline constexpr int lowestVelocity          = 0;
inline constexpr int highestVelocity         = 127;
inline constexpr int noteOffVelocity         = 0;
inline constexpr int sustainPedalController  = 64;
inline constexpr int sustainPedalOnThreshold = 64;
} // namespace midi

namespace velocity {
inline constexpr int minimumDelta          = -127;
inline constexpr int maximumDelta          = 127;
inline constexpr int minimumPercent        = 0;
inline constexpr int maximumPercent        = 200;
inline constexpr int percentDivisor        = 100;
inline constexpr int defaultByStepValue    = 100;
inline constexpr int defaultDeltaValue     = 0;
inline constexpr int defaultPercentValue   = 100;
} // namespace velocity

namespace lanes {
inline constexpr int laneCount               = 8;
inline constexpr int minimumStepCount        = 1;
inline constexpr int maximumStepCount        = 64;
inline constexpr int defaultStepCount        = 16;
inline constexpr int minimumFollowIndex      = 0;
inline constexpr int maximumFollowIndex      = 15;
inline constexpr int defaultFollowIndex      = 0;
inline constexpr int defaultFollowChannel    = 1;
inline constexpr int defaultOutputChannel    = 1;
inline constexpr int defaultChordInputChannel= 1;
inline constexpr int minimumGatePercent      = 1;
inline constexpr int maximumGatePercent      = 200;
inline constexpr int defaultGatePercent      = 100;
inline constexpr int gatePercentDivisor      = 100;
inline constexpr int minimumTranspose        = -24;
inline constexpr int maximumTranspose        = 24;
inline constexpr int defaultTranspose        = 0;
inline constexpr int minimumSwingPercent     = 0;
inline constexpr int maximumSwingPercent     = 100;
inline constexpr int defaultSwingPercent     = 0;
inline constexpr int swingPercentToShiftDivisor = 200;
inline constexpr int maximumVoicesPerLane    = 32;
} // namespace lanes

namespace timing {
struct RationalFactor { int numerator; int denominator; };
inline constexpr std::int64_t ticksPerBeat      = 960;
inline constexpr std::int64_t beatsPerWholeNote = 4;
inline constexpr std::int64_t ticksPerWholeNote = ticksPerBeat * beatsPerWholeNote;
inline constexpr RationalFactor straightFactor{1, 1};
inline constexpr RationalFactor tripletFactor{2, 3};
inline constexpr RationalFactor dottedFactor{3, 2};
inline constexpr std::array<int, 6> baseTimingDenominators{32, 16, 8, 4, 2, 1};
inline constexpr int defaultBaseTimingIndex = 1;
inline constexpr int textGridNumerator = 1;
inline constexpr std::array<int, 7> textGridDenominators{1, 2, 4, 8, 16, 32, 64};
inline constexpr int textGridMaximumEntries = 64;
inline constexpr char textGridEntrySeparator = ',';
inline constexpr char textGridFractionSeparator = '/';
inline constexpr char textGridTripletSuffix = 't';
inline constexpr char textGridDottedSuffix = '.';
inline constexpr double beatEpsilon = 1.0e-6;
inline constexpr double playheadJitterToleranceSamples = 1.0;
inline constexpr double sampleRoundingEpsilon = 1.0e-6;
inline constexpr double secondsPerMinute = 60.0;
} // namespace timing

namespace capacity {
inline constexpr int maximumSoundingVoices   = midi::channelCount * midi::pitchCount;
inline constexpr int voiceOwnerCount         = lanes::laneCount + 1;
inline constexpr int inputEventsPerChunk     = 4096;
inline constexpr int samplesPerChunk         = 4096;
inline constexpr int outputEventsPerBlock    = 16384;
inline constexpr int noteOffReserveEvents    = maximumSoundingVoices;
inline constexpr int outputMidiReserveBytes  = 1048576;
inline constexpr int midiEventBytes          = 9;
inline constexpr int noteOffReserveBytes     = maximumSoundingVoices * midiEventBytes;
inline constexpr int rawReleaseReserveBytes  = 3 * midi::channelCount * midiEventBytes;
inline constexpr int deferredEventCapacity   = 8192;
inline constexpr int deferredArenaBytes      = 65536;
} // namespace capacity

namespace colors {
inline constexpr std::array<std::uint32_t, lanes::laneCount> defaultLanePalette{
    0x4FC3F7 /*blue*/, 0x81C784 /*green*/, 0xBA68C8 /*purple*/, 0xF06292 /*pink*/,
    0x4DB6AC /*teal*/, 0x7986CB /*indigo*/, 0xDCE775 /*lime*/,  0xA1887F /*brown*/};
} // namespace colors

namespace editor {
inline constexpr int minimumStepDisplayWidth = 16;
inline constexpr int laneRowHeight           = 140;
inline constexpr int globalBarHeight         = 56;
inline constexpr int controlStripWidth       = 148;
inline constexpr int defaultWidth            = 1280;
inline constexpr int defaultHeight           = 820;
inline constexpr int minimumWidth            = 1000;
inline constexpr int minimumHeight           = 420;
inline constexpr int maximumWidth            = 3840;
inline constexpr int maximumHeight           = 2400;
inline constexpr int statusRefreshHz         = 30;
} // namespace editor

namespace state {
inline constexpr int currentFormatVersion = 1;
} // namespace state

} // namespace phu::sequencing::constants

// Static assertions for constant validity
static_assert(phu::sequencing::constants::timing::ticksPerWholeNote % (64 * 3) == 0,
              "All triplet lengths must be integral ticks");
static_assert((phu::sequencing::constants::timing::ticksPerWholeNote / 64) % 2 == 0,
              "All dotted lengths must be integral ticks");
static_assert(phu::sequencing::constants::colors::defaultLanePalette.size() == phu::sequencing::constants::lanes::laneCount,
              "Lane palette size must match lane count");
