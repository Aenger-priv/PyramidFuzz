cmake_minimum_required(VERSION 3.22)

project(PyramidFuzz VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

juce_add_binary_data(PyramidFuzzAssets
    SOURCES
        assets/generated/knob.png
        assets/generated/pyramid-fuzz-panel.png
)

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
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/generated/pyramid-fuzz-icon.png"
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
        src/dsp/NonlinearPath.h
        src/dsp/InputVoicingStage.h
        src/dsp/InputCircuit.h
        src/dsp/SustainStage.h
        src/dsp/OversamplingEngine.h
        src/dsp/OutputStage.h
        src/dsp/PreGainStage.h
        src/dsp/FeedbackClipper.h
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
        PyramidFuzzAssets
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
    src/dsp/NonlinearPath.h
    src/dsp/InputVoicingStage.h
    src/dsp/InputCircuit.h
    src/dsp/SustainStage.h
    src/dsp/OversamplingEngine.h
    src/dsp/OutputStage.h
    src/dsp/PreGainStage.h
    src/dsp/FeedbackClipper.h
    src/dsp/ClippingStage.h
    src/dsp/ToneStack.h
)

include(CTest)
if(BUILD_TESTING)
    juce_add_console_app(PyramidFuzzDspTests PRODUCT_NAME "Pyramid Fuzz DSP Tests")
    juce_generate_juce_header(PyramidFuzzDspTests)
    target_sources(PyramidFuzzDspTests PRIVATE tests/DspTests.cpp src/dsp/FuzzEngine.cpp)
    target_include_directories(PyramidFuzzDspTests PRIVATE src)
    target_compile_definitions(PyramidFuzzDspTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PyramidFuzzDspTests PRIVATE juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME PyramidFuzzDspTests COMMAND PyramidFuzzDspTests)

    add_executable(PyramidFuzzProcessorTests tests/ProcessorTests.cpp)
    target_include_directories(PyramidFuzzProcessorTests PRIVATE src "$<TARGET_PROPERTY:PyramidFuzz,JUCE_GENERATED_SOURCES_DIRECTORY>")
    target_compile_definitions(PyramidFuzzProcessorTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PyramidFuzzProcessorTests PRIVATE PyramidFuzz PyramidFuzzAssets juce::juce_audio_utils juce::juce_dsp)
    add_test(NAME PyramidFuzzProcessorTests COMMAND PyramidFuzzProcessorTests)

    # Offline measurements use the production stages without changing plugin DSP.
    juce_add_console_app(PyramidFuzzMeasurements PRODUCT_NAME "Pyramid Fuzz Measurements")
    juce_generate_juce_header(PyramidFuzzMeasurements)
    target_sources(PyramidFuzzMeasurements PRIVATE tools/Measurements.cpp src/dsp/FuzzEngine.cpp)
    target_include_directories(PyramidFuzzMeasurements PRIVATE src)
    target_compile_definitions(PyramidFuzzMeasurements PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PyramidFuzzMeasurements PRIVATE juce::juce_dsp juce::juce_audio_formats)

    juce_add_console_app(PyramidFuzzQualityMeasurements PRODUCT_NAME "Pyramid Fuzz Quality Measurements")
    juce_generate_juce_header(PyramidFuzzQualityMeasurements)
    target_sources(PyramidFuzzQualityMeasurements PRIVATE tools/QualityMeasurements.cpp src/dsp/FuzzEngine.cpp)
    target_include_directories(PyramidFuzzQualityMeasurements PRIVATE src)
    target_compile_definitions(PyramidFuzzQualityMeasurements PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(PyramidFuzzQualityMeasurements PRIVATE juce::juce_dsp juce::juce_audio_formats)

    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME PyramidFuzzToneReferenceTests
            COMMAND "${Python3_EXECUTABLE}" -m unittest discover -s "${CMAKE_CURRENT_SOURCE_DIR}/tests" -p "test_*.py")
        set_tests_properties(PyramidFuzzToneReferenceTests PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME PyramidFuzzToneResponseTests
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_tone_capture.py"
                    --executable "$<TARGET_FILE:PyramidFuzzMeasurements>")
        set_tests_properties(PyramidFuzzToneResponseTests PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME PyramidFuzzClippingResponseTests
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_clipping_capture.py"
                    --executable "$<TARGET_FILE:PyramidFuzzMeasurements>")
        set_tests_properties(PyramidFuzzClippingResponseTests PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
        add_test(NAME PyramidFuzzInputResponseTests
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/validate_input_capture.py"
                    --executable "$<TARGET_FILE:PyramidFuzzMeasurements>")
        set_tests_properties(PyramidFuzzInputResponseTests PROPERTIES ENVIRONMENT "PYTHONDONTWRITEBYTECODE=1")
    endif()
endif()
