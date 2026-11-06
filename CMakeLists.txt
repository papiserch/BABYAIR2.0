cmake_minimum_required(VERSION 3.22)

# Windows: static MSVC runtime so the VST3 does not need the VC++ redistributable
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(BABY_AIR VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

option(BABY_AIR_BUILD_PLUGIN "Build the JUCE plugin (VST3 / AU / Standalone)" ON)
option(BABY_AIR_BUILD_TESTS  "Build the JUCE-free DSP test suite" ON)
set(JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (leave empty to download JUCE 8 with FetchContent)")

# --- DSP validation tests (no JUCE needed) -----------------------------------------------------
if(BABY_AIR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Source/Tests)
endif()

# --- Plugin ------------------------------------------------------------------------------------
if(BABY_AIR_BUILD_PLUGIN)
    if(JUCE_PATH)
        add_subdirectory(${JUCE_PATH} juce)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW ON)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(PLUGIN_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND PLUGIN_FORMATS AU)
    endif()

    juce_add_plugin(BabyAir
        COMPANY_NAME "Baby Demon"
        PLUGIN_MANUFACTURER_CODE Bdmn
        PLUGIN_CODE Bair
        PRODUCT_NAME "BABY AIR"
        FORMATS ${PLUGIN_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Fx
        AU_MAIN_TYPE kAudioUnitType_Effect
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(BabyAir PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Parameters.cpp
        Source/DSP/OversamplingEngine.cpp
        Source/DSP/ModeProcessor.cpp
        Source/DSP/ExciterEngine.cpp)

    target_include_directories(BabyAir PRIVATE Source)

    target_compile_definitions(BabyAir PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(BabyAir
        PRIVATE juce::juce_audio_utils
        PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

    # The DSP relies on IEEE semantics (std::isfinite sanitising): never use fast-math.
    if(NOT MSVC)
        target_compile_options(BabyAir PRIVATE -fno-fast-math)
    endif()
endif()
