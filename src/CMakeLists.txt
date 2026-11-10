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

target_sources(phu-sequential PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
)

target_link_libraries(phu-sequential
    PRIVATE
        SequentialCoreLib
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
