cmake_minimum_required(VERSION 3.22)
project(FoundryPlugin VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Foundry
    COMPANY_NAME               "YourName"
    PLUGIN_MANUFACTURER_CODE   Ychr
    PLUGIN_CODE                Fndr
    FORMATS                    VST3 Standalone
    PRODUCT_NAME               "Foundry"
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           TRUE
    NEEDS_MIDI_OUTPUT          FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE)

# All source files sit next to this CMakeLists.txt (flat layout).
target_sources(Foundry PRIVATE
    PitchDetector.h
    HarmonizerEngine.h
    Dsp.h
    FoundryChain.h
    PluginProcessor.h
    PluginProcessor.cpp
    PluginEditor.h
    PluginEditor.cpp)

target_compile_definitions(Foundry PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Foundry
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
