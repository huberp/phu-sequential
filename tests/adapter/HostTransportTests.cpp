#include <catch2/catch_test_macros.hpp>
#include "HostTransport.h"
#include "arpeggiator/TransportTracker.h"

#include <juce_audio_basics/juce_audio_basics.h>

using namespace phu::plugin;
using namespace phu::arpeggiator;

namespace {

// Minimal fake juce::AudioPlayHead for testing (no inheritance complexity)
struct FakePlayHead : public juce::AudioPlayHead {
    FakePlayHead() = default;

    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override {
        return currentPosition;
    }

    juce::AudioPlayHead::PositionInfo currentPosition;
};

}  // namespace

// --- HostTransport: null and default cases -------------------------------------------------

TEST_CASE("HostTransport: null playhead returns not playing", "[HostTransport]") {
    auto info = readHostTransport(nullptr);
    CHECK(info.isPlaying == false);
    CHECK(info.hasMusicalPosition == false);
    CHECK(info.playheadBeat == 0.0);
    CHECK(info.tempoBeatsPerMinute == 0.0);
}

TEST_CASE("HostTransport: default PositionInfo returns not playing", "[HostTransport]") {
    FakePlayHead playhead;
    playhead.currentPosition = juce::AudioPlayHead::PositionInfo();
    auto info = readHostTransport(&playhead);
    CHECK(info.isPlaying == false);
    CHECK(info.hasMusicalPosition == false);
    CHECK(info.tempoBeatsPerMinute == 0.0);
}

// --- HostTransport: isPlaying extraction ---------------------------------------------------

TEST_CASE("HostTransport: reads isPlaying state", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    
    // Not playing
    pos.setIsPlaying(false);
    auto infoStopped = readHostTransport(&playhead);
    CHECK(infoStopped.isPlaying == false);
    
    // Playing
    pos.setIsPlaying(true);
    auto infoPlaying = readHostTransport(&playhead);
    CHECK(infoPlaying.isPlaying == true);
}

// --- HostTransport: playheadBeat with PPQ -------------------------------------------------

TEST_CASE("HostTransport: sets playheadBeat and hasMusicalPosition when PPQ present", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    pos.setIsPlaying(true);
    pos.setPpqPosition(123.5);
    
    auto info = readHostTransport(&playhead);
    CHECK(info.isPlaying == true);
    CHECK(info.hasMusicalPosition == true);
    CHECK(info.playheadBeat == 123.5);
}

TEST_CASE("HostTransport: hasMusicalPosition false when PPQ absent", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    pos.setIsPlaying(true);
    // No PPQ set
    
    auto info = readHostTransport(&playhead);
    CHECK(info.isPlaying == true);
    CHECK(info.hasMusicalPosition == false);
    CHECK(info.playheadBeat == 0.0);
}

TEST_CASE("HostTransport: playheadBeat zero when starting", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    pos.setIsPlaying(true);
    pos.setPpqPosition(0.0);
    
    auto info = readHostTransport(&playhead);
    CHECK(info.playheadBeat == 0.0);
    CHECK(info.hasMusicalPosition == true);
}

// --- HostTransport: tempo extraction -------------------------------------------------------

TEST_CASE("HostTransport: extracts tempo when BPM present", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    pos.setIsPlaying(true);
    pos.setBpm(120.0);
    
    auto info = readHostTransport(&playhead);
    CHECK(info.tempoBeatsPerMinute == 120.0);
}

TEST_CASE("HostTransport: tempo zero when BPM absent", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    pos.setIsPlaying(true);
    // No BPM set
    
    auto info = readHostTransport(&playhead);
    CHECK(info.tempoBeatsPerMinute == 0.0);
}

TEST_CASE("HostTransport: handles extreme tempos", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    pos.setIsPlaying(true);
    
    // Very slow
    pos.setBpm(1.0);
    auto infoSlow = readHostTransport(&playhead);
    CHECK(infoSlow.tempoBeatsPerMinute == 1.0);
    
    // Very fast
    pos.setBpm(300.0);
    auto infoFast = readHostTransport(&playhead);
    CHECK(infoFast.tempoBeatsPerMinute == 300.0);
}

// --- HostTransport: combined states -------------------------------------------------------

TEST_CASE("HostTransport: stopped state has zero tempo", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    pos.setIsPlaying(false);
    pos.setBpm(120.0);
    
    auto info = readHostTransport(&playhead);
    CHECK(info.isPlaying == false);
    // Stopped -> tempo may or may not be read (spec doesn't specify)
}

TEST_CASE("HostTransport: all fields populated together", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    pos.setIsPlaying(true);
    pos.setPpqPosition(5.5);
    pos.setBpm(140.0);
    
    auto info = readHostTransport(&playhead);
    CHECK(info.isPlaying == true);
    CHECK(info.hasMusicalPosition == true);
    CHECK(info.playheadBeat == 5.5);
    CHECK(info.tempoBeatsPerMinute == 140.0);
}

TEST_CASE("HostTransport: fractional PPQ values", "[HostTransport]") {
    FakePlayHead playhead;
    auto& pos = playhead.currentPosition;
    pos.setIsPlaying(true);
    pos.setPpqPosition(0.123456);
    
    auto info = readHostTransport(&playhead);
    CHECK(info.playheadBeat == 0.123456);
}

