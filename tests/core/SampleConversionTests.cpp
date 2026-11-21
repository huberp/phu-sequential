#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "sequencing/SampleConversion.h"

// M04 Sample conversion: the pure beats<->samples boundary (§10, §16.2, §17).
using namespace phu::sequencing;

TEST_CASE("SampleConversion: samplesPerBeat at several BPM/sample-rate combinations",
          "[M04][sample-conversion][REQ-10-conversion]") {
    CHECK(samplesPerBeat(120.0, 44100.0) == Catch::Approx(22050.0));
    CHECK(samplesPerBeat(60.0, 48000.0) == Catch::Approx(48000.0));
    CHECK(samplesPerBeat(140.0, 48000.0) == Catch::Approx(48000.0 * 60.0 / 140.0));
    CHECK(samplesPerBeat(96.0, 96000.0) == Catch::Approx(96000.0 * 60.0 / 96.0));
}

TEST_CASE("SampleConversion: samplesPerBeat returns 0 for non-positive bpm or sample rate",
          "[M04][sample-conversion]") {
    CHECK(samplesPerBeat(0.0, 48000.0) == 0.0);
    CHECK(samplesPerBeat(-10.0, 48000.0) == 0.0);
    CHECK(samplesPerBeat(120.0, 0.0) == 0.0);
    CHECK(samplesPerBeat(120.0, -1.0) == 0.0);
}

TEST_CASE("SampleConversion: beatsToSamples and samplesToBeats round-trip", "[M04][sample-conversion]") {
    const double spb = samplesPerBeat(120.0, 44100.0);
    CHECK(beatsToSamples(1.0, spb) == Catch::Approx(spb));
    CHECK(beatsToSamples(2.5, spb) == Catch::Approx(spb * 2.5));
    CHECK(samplesToBeats(beatsToSamples(3.0, spb), spb) == Catch::Approx(3.0));
}

TEST_CASE("SampleConversion: samplesToBeats returns 0 when samplesPerBeat is 0", "[M04][sample-conversion]") {
    CHECK(samplesToBeats(1000.0, 0.0) == 0.0);
}

TEST_CASE("SampleConversion: beatToSampleOffset floors and clamps to the block", "[M04][sample-conversion][D-15]") {
    const double spb = 1000.0;
    CHECK(beatToSampleOffset(0.5, 0.0, spb, 1024) == 500);
    CHECK(beatToSampleOffset(2.0, 0.0, spb, 512) == 511);     // clamped to numSamples - 1
    CHECK(beatToSampleOffset(-1.0, 0.0, spb, 512) == 0);      // clamped to 0
    CHECK(beatToSampleOffset(1.5, 1.0, spb, 1024) == 500);    // relative to blockStartBeat
}

TEST_CASE("SampleConversion: beatToSampleOffset returns 0 when numSamples <= 0", "[M04][sample-conversion]") {
    CHECK(beatToSampleOffset(0.5, 0.0, 1000.0, 0) == 0);
    CHECK(beatToSampleOffset(0.5, 0.0, 1000.0, -5) == 0);
}

TEST_CASE("SampleConversion: loopWrapSampleOffset finds the loop end inside a block (D-59)", "[M04][sample-conversion][D-59]") {
    constexpr double spb = 24000.0;
    // Loop [0, 16) beats, block starts 3072 samples before the loop end.
    CHECK(loopWrapSampleOffset(16.0 - 3072.0 / spb, 0.0, 16.0, spb, 4096) == 3072);
    // Fractional loop end -> first sample at or after it; tiny ppq noise does not move it.
    CHECK(loopWrapSampleOffset(16.0 - 100.5 / spb, 0.0, 16.0, spb, 4096) == 101);
    CHECK(loopWrapSampleOffset(16.0 - (100.0 + 1e-9) / spb, 0.0, 16.0, spb, 4096) == 100);
    // Loop end at block start, at/after the block end, or block already past the loop end: no cut.
    CHECK(loopWrapSampleOffset(16.0, 0.0, 16.0, spb, 4096) == -1);
    CHECK(loopWrapSampleOffset(16.0 - 4096.0 / spb, 0.0, 16.0, spb, 4096) == -1);
    CHECK(loopWrapSampleOffset(17.0, 0.0, 16.0, spb, 4096) == -1);
    // Empty loop region or no clock.
    CHECK(loopWrapSampleOffset(15.9, 16.0, 16.0, spb, 4096) == -1);
    CHECK(loopWrapSampleOffset(15.9, 0.0, 16.0, 0.0, 4096) == -1);
}
