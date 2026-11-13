add_library(SequentialCoreLib STATIC)

target_include_directories(SequentialCoreLib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(SequentialCoreLib PRIVATE
    sequencing/Constants.h
    sequencing/MusicalTime.h
    sequencing/NoteValues.h
    sequencing/StepTiming.h
    sequencing/StepTiming.cpp
    sequencing/SampleConversion.h
    sequencing/TextGridParser.h
    sequencing/TextGridParser.cpp
    sequencing/MidiEvent.h
    sequencing/OutputEventBuffer.h
    sequencing/OutputEventBuffer.cpp
    sequencing/HeldNoteTracker.h
    sequencing/HeldNoteTracker.cpp
    sequencing/sources/SourceSelection.h
    sequencing/sources/SourceSelection.cpp
    sequencing/sources/ChordFollowSource.h
    sequencing/sources/ChordFollowSource.cpp
    sequencing/sources/ChannelSource.h
    sequencing/sources/ChannelSource.cpp
    sequencing/sources/InputModeRegistry.h
    sequencing/velocity/VelocityMode.h
    sequencing/velocity/IncomingVelocity.h
    sequencing/velocity/ByStepVelocity.h
    sequencing/velocity/DeltaByStepVelocity.h
    sequencing/velocity/RelativeVelocity.h
    sequencing/velocity/VelocityModeRegistry.h
    sequencing/velocity/VelocityModeRegistry.cpp
    sequencing/StepData.h
    sequencing/StepData.cpp
    sequencing/VoiceRegistry.h
    sequencing/VoiceRegistry.cpp
    arpeggiator/LaneConfiguration.h
    arpeggiator/LaneConfiguration.cpp
    arpeggiator/BlockClock.h
    arpeggiator/BlockClock.cpp
    arpeggiator/LanePosition.h
    arpeggiator/LanePosition.cpp
    arpeggiator/LaneEngine.h
    arpeggiator/LaneEngine.cpp
    arpeggiator/DisplayScale.h
    arpeggiator/DisplayScale.cpp
)

set_target_properties(SequentialCoreLib PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)
