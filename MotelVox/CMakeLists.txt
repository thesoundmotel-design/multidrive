cmake_minimum_required(VERSION 3.22)
project(MotelVox VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(MV_FORMATS VST3 AU Standalone)
else()
    set(MV_FORMATS VST3 Standalone)
endif()

juce_add_plugin(MotelVox
    COMPANY_NAME "The Sound Motel"
    PLUGIN_MANUFACTURER_CODE Smtl
    PLUGIN_CODE Mvox
    FORMATS ${MV_FORMATS}
    PRODUCT_NAME "Motel Vox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(MotelVoxAssets SOURCES Assets/logo.png)

target_sources(MotelVox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(MotelVox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MotelVox
    PRIVATE
        MotelVoxAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# DSP unit test (no JUCE needed):  cmake --build build --target dsp_test
add_executable(dsp_test EXCLUDE_FROM_ALL tests/dsp_test.cpp)
