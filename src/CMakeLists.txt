# Semicolon-separated JUCE formats; set by the "formats-*" presets in CMakePresets.json.
set(PHU_PLUGIN_FORMATS "VST3" CACHE STRING "JUCE plugin formats to build (e.g. \"VST3;AU\")")

juce_add_plugin(phu-sequential
    PLUGIN_MANUFACTURER_CODE Phus
    PLUGIN_CODE Pseq
    FORMATS ${PHU_PLUGIN_FORMATS}
    PRODUCT_NAME "phu-sequential"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
)

set(PHU_PLUGIN_SOURCES
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    HostTransport.cpp
    HostTransport.h
    MidiConversion.cpp
    MidiConversion.h
    BlockEngine.cpp
    BlockEngine.h
    ParameterIds.h
    ParameterIds.cpp
    ParameterLayout.h
    ParameterLayout.cpp
    ParameterReader.h
    ParameterReader.cpp
    PatternStore.h
    PatternStore.cpp
    StateMigration.h
    StateMigration.cpp
    LaneStateSerializer.h
    LaneEditHistory.h
    LaneEditHistory.cpp
    StateSerializer.h
    StateSerializer.cpp
    PresetManager.h
    PresetManager.cpp
    editor/EditorControls.h
    editor/EditorLayout.h
    editor/EditorLookAndFeel.h
    editor/GlobalBar.h
    editor/GlobalBar.cpp
    editor/LaneControlStrip.h
    editor/LaneControlStrip.cpp
    editor/LaneRow.h
    editor/LaneRow.cpp
    editor/StepDisplay.h
    editor/StepDisplay.cpp
    editor/TextGridField.h
    editor/TextGridField.cpp
)
target_sources(phu-sequential PRIVATE ${PHU_PLUGIN_SOURCES})

target_include_directories(phu-sequential PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(phu-sequential
    PRIVATE
        SequentialCoreLib
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Offscreen editor renderer used by CI ("take-screenshot"); not part of the plugin.
option(PHU_BUILD_SCREENSHOT_TOOL "Build the editor screenshot tool (phu-screenshot)" OFF)
if(PHU_BUILD_SCREENSHOT_TOOL)
    juce_add_console_app(phu-screenshot PRODUCT_NAME "phu-screenshot")
    target_sources(phu-screenshot PRIVATE
        ${CMAKE_SOURCE_DIR}/tools/screenshot/ScreenshotMain.cpp
        ${PHU_PLUGIN_SOURCES})
    target_include_directories(phu-screenshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(phu-screenshot PRIVATE
        JucePlugin_Name="phu-sequential"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(phu-screenshot
        PRIVATE
            SequentialCoreLib
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags)
endif()
