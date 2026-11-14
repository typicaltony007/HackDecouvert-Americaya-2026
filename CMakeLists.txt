# ==============================================================================
#  SynthMath - JUCE VST3 synthesizer plugin
# ==============================================================================
cmake_minimum_required(VERSION 3.22)

project(SynthMath VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Group targets into folders in the Visual Studio solution explorer.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Static MSVC runtime: the plugin DLL won't depend on the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ------------------------------------------------------------------------------
#  Locate JUCE
#  By default this uses the local JUCE 9.0.5 checkout next to this project.
#  Override with:  cmake -DJUCE_DIR=C:/path/to/JUCE ...
#  If the directory doesn't exist, JUCE is downloaded from GitHub instead.
# ------------------------------------------------------------------------------
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../juce-9.0.5-windows/JUCE"
    CACHE PATH "Path to the JUCE framework (folder containing JUCE's CMakeLists.txt)")

if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(STATUS "Using local JUCE: ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    message(STATUS "JUCE not found at '${JUCE_DIR}' - fetching JUCE 9.0.5 from GitHub")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        9.0.5
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ------------------------------------------------------------------------------
#  Plugin target
# ------------------------------------------------------------------------------
option(SYNTH_COPY_PLUGIN_AFTER_BUILD
       "Copy the VST3 into the system VST3 folder after building (needs admin rights)" OFF)

juce_add_plugin(SynthMath
    COMPANY_NAME                "Hackathon Audio"
    PRODUCT_NAME                "SynthMath"
    VERSION                     ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE    Hack
    PLUGIN_CODE                 Shk1
    FORMATS                     VST3 Standalone       # Standalone is handy for quick testing
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE                  # computer keys A..J play notes
    VST3_CATEGORIES             Instrument Synth
    COPY_PLUGIN_AFTER_BUILD     ${SYNTH_COPY_PLUGIN_AFTER_BUILD}
    NEEDS_WEB_BROWSER           FALSE
    NEEDS_CURL                  FALSE)

target_sources(SynthMath
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/Parameters.cpp
        Source/Parameters.h

        Source/DSP/Oscillator.h
        Source/DSP/CustomWaveTable.cpp
        Source/DSP/CustomWaveTable.h
        Source/DSP/ModSource.h
        Source/DSP/SynthVoice.cpp
        Source/DSP/SynthVoice.h
        Source/DSP/EffectsChain.cpp
        Source/DSP/EffectsChain.h
        Source/DSP/SpectrumTap.h

        Source/UI/SynthLookAndFeel.cpp
        Source/UI/SynthLookAndFeel.h
        Source/UI/Controls.h
        Source/UI/ComputerKeyboard.h
        Source/UI/OscillatorPanel.cpp
        Source/UI/OscillatorPanel.h
        Source/UI/EffectsPanel.cpp
        Source/UI/EffectsPanel.h
        Source/UI/FxChainView.cpp
        Source/UI/FxChainView.h
        Source/UI/EqVisualiser.cpp
        Source/UI/EqVisualiser.h
        Source/UI/ModulationPanel.cpp
        Source/UI/ModulationPanel.h)

target_include_directories(SynthMath PRIVATE Source)

target_compile_definitions(SynthMath
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SynthMath
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(SynthMath PRIVATE /MP)   # parallel compilation
endif()

# Make the Standalone the default startup project when opening the .sln
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
             PROPERTY VS_STARTUP_PROJECT SynthMath_Standalone)
