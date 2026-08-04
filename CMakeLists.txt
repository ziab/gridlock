cmake_minimum_required(VERSION 3.22)

project(MidiGridAnalyzer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fetch JUCE 7 framework
include(FetchContent)
FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
)
FetchContent_MakeAvailable(juce)

# Embed WAV sound assets
juce_add_binary_data(MidiGridAnalyzerAssets
    HEADER_NAME "BinaryData.h"
    NAMESPACE "BinaryData"
    SOURCES
        assets/sounds/wood_clave.wav
        assets/sounds/Drum_Stick_Click_01.wav
)

# Define JUCE Audio Plugin & Standalone target
juce_add_plugin(MidiGridAnalyzer
    COMPANY_NAME "Gridlock"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Grdl
    PLUGIN_CODE Mga1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "MIDI Grid Analyzer"
)

# Source files
target_sources(MidiGridAnalyzer PRIVATE
    Source/HitEvent.h
    Source/RingBuffer.h
    Source/ClickGenerator.h
    Source/ClickGenerator.cpp
    Source/RemoteControlServer.h
    Source/RemoteControlServer.cpp
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/GridComponent.h
    Source/GridComponent.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)

# Link JUCE Modules and Embedded Binary Data
target_link_libraries(MidiGridAnalyzer PRIVATE
    MidiGridAnalyzerAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_gui_basics
    juce::juce_graphics
    juce::juce_audio_formats
)

target_compile_definitions(MidiGridAnalyzer PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)
