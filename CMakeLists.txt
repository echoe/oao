cmake_minimum_required(VERSION 3.22)
project(OAO VERSION 2.0.3 LANGUAGES C CXX)

# --- Use local JUCE if available, otherwise fetch ---
set(JUCE_LOCAL_PATH "$ENV{HOME}/JUCE" CACHE PATH "Path to local JUCE installation")

if(EXISTS "${JUCE_LOCAL_PATH}/CMakeLists.txt")
    message(STATUS "Using local JUCE at ${JUCE_LOCAL_PATH}")
    add_subdirectory(${JUCE_LOCAL_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    message(STATUS "Local JUCE not found at ${JUCE_LOCAL_PATH}, fetching...")
    include(FetchContent)
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.13
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(juce)
endif()
# Target 1: Synth
juce_add_plugin(OAO
    COMPANY_NAME "echoe"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "Ecoe"
    PLUGIN_PLUGIN_CODE "OAOS"
    FORMATS VST3 Standalone
    PRODUCT_NAME "OAO")

target_sources(OAO PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/FMVoice.cpp
)

target_compile_definitions(OAO PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_compile_features(OAO PRIVATE cxx_std_17)

target_link_libraries(OAO PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_audio_utils
)

juce_generate_juce_header(OAO)

# Target 2: Effect
juce_add_plugin(OAOFX
    COMPANY_NAME "echoe"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE "Ecoe"
    PLUGIN_PLUGIN_CODE "OAOX"
    FORMATS VST3
    PRODUCT_NAME "OAOFX")

target_sources(OAO_FX PRIVATE
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
    Source/FMVoice.cpp
)

target_compile_definitions(OAOFX PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    OAO_FX_ONLY=1
)

target_compile_features(OAOFX PRIVATE cxx_std_17)

target_link_libraries(OAOFX PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_audio_utils
)

juce_generate_juce_header(OAOFX)
