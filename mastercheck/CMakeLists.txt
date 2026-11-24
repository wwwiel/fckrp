cmake_minimum_required(VERSION 3.22)

# fckrp check: analyser for the end of the master chain (analysis only, audio passes unchanged).
project(fckrp_check VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(CHECK_FORMATS VST3 AU)
else()
    set(CHECK_FORMATS VST3)
endif()

juce_add_plugin(fckrp_check
    COMPANY_NAME "fckrp"
    PRODUCT_NAME "fckrp check"
    BUNDLE_ID "com.fckrp.check"
    PLUGIN_MANUFACTURER_CODE Fckr
    PLUGIN_CODE Fkck
    FORMATS ${CHECK_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Analyzer)

set(CHECK_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Advisor.cpp)
set(CHECK_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

target_sources(fckrp_check PRIVATE ${CHECK_SOURCES})
target_include_directories(fckrp_check PRIVATE Source)
target_compile_definitions(fckrp_check PUBLIC ${CHECK_DEFS}
    JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

if(APPLE)
    add_custom_command(TARGET fckrp_check_VST3 POST_BUILD
        COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:fckrp_check_VST3>"
        VERBATIM)
endif()

target_link_libraries(fckrp_check
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(CHECK_BUILD_TESTS "Build the offline test program" OFF)
if(CHECK_BUILD_TESTS)
    juce_add_console_app(mastercheck_tests PRODUCT_NAME "mastercheck_tests")
    target_sources(mastercheck_tests PRIVATE Tests/RunTests.cpp ${CHECK_SOURCES})
    target_include_directories(mastercheck_tests PRIVATE Source)
    target_compile_definitions(mastercheck_tests PRIVATE JucePlugin_Name="fckrp check" ${CHECK_DEFS})
    target_link_libraries(mastercheck_tests PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

option(CHECK_BUILD_LOADCHECK "Build the plugin load-check program" OFF)
if(CHECK_BUILD_LOADCHECK)
    juce_add_console_app(mastercheck_loadcheck PRODUCT_NAME "mastercheck_loadcheck")
    target_sources(mastercheck_loadcheck PRIVATE Tests/LoadCheck.cpp)
    target_compile_definitions(mastercheck_loadcheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=0 ${CHECK_DEFS})
    target_link_libraries(mastercheck_loadcheck PRIVATE juce::juce_audio_processors juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
