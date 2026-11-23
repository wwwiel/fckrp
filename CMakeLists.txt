cmake_minimum_required(VERSION 3.22)

project(fckrp VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Link the C++ runtime statically on Windows so the plugin does not depend on
# the Visual C++ Redistributable being installed.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE is downloaded automatically. To use a local copy instead, configure with
#   -DFETCHCONTENT_SOURCE_DIR_JUCE=/path/to/JUCE
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(FCKRP_FORMATS VST3 AU)
else()
    set(FCKRP_FORMATS VST3)
endif()

juce_add_plugin(fckrp
    COMPANY_NAME "fckrp"
    PRODUCT_NAME "fckrp"
    BUNDLE_ID "com.fckrp.fckrp"
    PLUGIN_MANUFACTURER_CODE Fckr
    PLUGIN_CODE Fkrp
    FORMATS ${FCKRP_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics EQ Reverb)

target_sources(fckrp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Presets.cpp)

target_include_directories(fckrp PRIVATE Source)

target_compile_definitions(fckrp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# JUCE writes Contents/Resources/moduleinfo.json into the VST3 bundle after
# signing it, which breaks the ad-hoc signature on macOS. Re-sign afterwards.
if(APPLE)
    add_custom_command(TARGET fckrp_VST3 POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:fckrp_VST3>"
        VERBATIM)
endif()

target_link_libraries(fckrp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline tests: run the plugin's audio code on test signals without a DAW.
#   cmake -B build -DFCKRP_BUILD_TESTS=ON && cmake --build build --target fckrp_tests
option(FCKRP_BUILD_TESTS "Build the offline test program" OFF)
if(FCKRP_BUILD_TESTS)
    juce_add_console_app(fckrp_tests PRODUCT_NAME "fckrp_tests")
    target_sources(fckrp_tests PRIVATE
        Tests/RunTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Presets.cpp)
    target_include_directories(fckrp_tests PRIVATE Source)
    target_compile_definitions(fckrp_tests PRIVATE
        JucePlugin_Name="fckrp"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(fckrp_tests PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()

# Load check: opens the built plugin file like a DAW would.
#   cmake -B build -DFCKRP_BUILD_LOADCHECK=ON && cmake --build build --target fckrp_loadcheck
option(FCKRP_BUILD_LOADCHECK "Build the plugin load-check program" OFF)
if(FCKRP_BUILD_LOADCHECK)
    juce_add_console_app(fckrp_loadcheck PRODUCT_NAME "fckrp_loadcheck")
    target_sources(fckrp_loadcheck PRIVATE Tests/LoadCheck.cpp)
    target_compile_definitions(fckrp_loadcheck PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_PLUGINHOST_AU=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(fckrp_loadcheck PRIVATE
        juce::juce_audio_processors
        juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()

if(FCKRP_BUILD_TESTS)
    juce_add_console_app(fckrp_fuzz PRODUCT_NAME "fckrp_fuzz")
    target_sources(fckrp_fuzz PRIVATE Tests/Fuzz.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/Presets.cpp)
    target_include_directories(fckrp_fuzz PRIVATE Source)
    target_compile_definitions(fckrp_fuzz PRIVATE JucePlugin_Name="fckrp" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(fckrp_fuzz PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
