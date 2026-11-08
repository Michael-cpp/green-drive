cmake_minimum_required(VERSION 3.22)
project(GreenDrive VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.5
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(GreenDrive
    COMPANY_NAME "GreenDrive"
    PLUGIN_MANUFACTURER_CODE Grdv
    PLUGIN_CODE Grdr
    FORMATS VST3 Standalone
    PRODUCT_NAME "Green Drive"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(GreenDrive PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PedalTexture.cpp)

target_compile_definitions(GreenDrive PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GreenDrive
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# DSP checks with no JUCE dependency.
add_executable(ModelTest Tests/ModelTest.cpp)
target_include_directories(ModelTest PRIVATE Source)
