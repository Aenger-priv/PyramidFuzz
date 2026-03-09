cmake_minimum_required(VERSION 3.22)

project(PyramidFuzz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

juce_add_plugin(PyramidFuzz
    COMPANY_NAME "Anders"
    BUNDLE_ID com.anders.pyramidfuzz
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE AnDr
    PLUGIN_CODE PhFz
    FORMATS AU Standalone
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Pyramid Fuzz needs microphone access to capture guitar or line input in the standalone app."
    PRODUCT_NAME "Pyramid Fuzz"
)

juce_generate_juce_header(PyramidFuzz)

target_sources(PyramidFuzz
    PRIVATE
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/dsp/DspUtilities.h
        src/dsp/FuzzEngine.cpp
        src/dsp/FuzzEngine.h
        src/dsp/HighControlFilter.h
        src/dsp/InputVoicingStage.h
        src/dsp/OversamplingEngine.h
        src/dsp/OutputStage.h
        src/dsp/PreGainStage.h
        src/dsp/ClippingStage.h
        src/dsp/ToneStack.h
)

target_compile_definitions(PyramidFuzz
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(PyramidFuzz
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    src/PluginEditor.cpp
    src/PluginEditor.h
    src/PluginProcessor.cpp
    src/PluginProcessor.h
    src/dsp/DspUtilities.h
    src/dsp/FuzzEngine.cpp
    src/dsp/FuzzEngine.h
    src/dsp/HighControlFilter.h
    src/dsp/InputVoicingStage.h
    src/dsp/OversamplingEngine.h
    src/dsp/OutputStage.h
    src/dsp/PreGainStage.h
    src/dsp/ClippingStage.h
    src/dsp/ToneStack.h
)
