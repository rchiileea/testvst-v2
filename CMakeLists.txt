cmake_minimum_required(VERSION 3.22)
project(VOX_RE_STORE_V2 VERSION 2.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VOX_RE_STORE
    COMPANY_NAME "Independent Audio Tools"
    BUNDLE_ID "audio.independent.voxrestore"
    PLUGIN_MANUFACTURER_CODE InAu
    PLUGIN_CODE VxR2
    FORMATS VST3 Standalone
    PRODUCT_NAME "VOX_RESTORE"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(VOX_RE_STORE)

target_sources(VOX_RE_STORE PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)
target_compile_features(VOX_RE_STORE PRIVATE cxx_std_17)
target_compile_definitions(VOX_RE_STORE PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
)
target_link_libraries(VOX_RE_STORE PRIVATE
    juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags
           juce::juce_recommended_lto_flags
           juce::juce_recommended_warning_flags
)
