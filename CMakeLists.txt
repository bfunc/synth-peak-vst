cmake_minimum_required(VERSION 3.22)

project(PeakControl VERSION 0.1.0 LANGUAGES C CXX)
set(PEAK_VERSION_LABEL "0.1.0")  # the next release; raise it right after each release

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()
# Static MSVC runtime, so the app runs without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ---- core: plain C++, no JUCE, tested on its own ----
# Parameter table, CC / NRPN coding, patch SysEx and model, sync / timing logic, library, connection checklist,
# and the UI layout spec + modulation map (data only).
add_library(peak_core STATIC
    src/core/ParamTable.cpp
    src/core/Midi.cpp
    src/core/Sysex.cpp
    src/core/SendQueue.cpp
    src/core/Patch.cpp
    src/core/Timing.cpp
    src/core/Link.cpp
    src/core/Library.cpp
    src/core/Sync.cpp
    src/core/SlotWrite.cpp
    src/ui/Layout.cpp
    src/ui/ModMap.cpp)
target_include_directories(peak_core PUBLIC src)
set_target_properties(peak_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

enable_testing()
add_executable(peak_tests tests/CoreTests.cpp)
target_link_libraries(peak_tests PRIVATE peak_core)
target_compile_definitions(peak_tests PRIVATE PEAK_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/docs/dumps")
add_test(NAME core COMMAND peak_tests)

# PEAK_CORE_ONLY=ON skips JUCE (quick core test builds).
option(PEAK_CORE_ONLY "Build only the core library and its tests" OFF)
if(PEAK_CORE_ONLY)
    return()
endif()

# ---- the plugin ----
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PeakControl
    COMPANY_NAME "bFunc"
    COMPANY_CODE Bfnc
    PLUGIN_MANUFACTURER_CODE Bfnc
    PLUGIN_CODE PkCt
    PRODUCT_NAME "Peak Control"
    BUNDLE_ID cc.bfunc.peakcontrol
    FORMATS Standalone VST3
    VST3_CATEGORIES Instrument Synth
    # A generator in FL Studio: its MIDI goes out through the wrapper's MIDI output port.
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE)

set(PEAK_PLUGIN_SOURCES
    src/link/PeakLink.cpp
    src/plugin/ParamBridge.cpp
    src/plugin/PatchSession.cpp
    src/plugin/LibraryService.cpp
    src/plugin/PeakProcessor.cpp
    src/ui/PeakLook.cpp
    src/ui/Widgets.cpp
    src/ui/Graphs.cpp
    src/ui/PanelKit.cpp
    src/ui/MainPage.cpp
    src/ui/FxMatrixPages.cpp
    src/ui/LibrarySetupPages.cpp
    src/ui/PeakEditor.cpp)
target_sources(PeakControl PRIVATE ${PEAK_PLUGIN_SOURCES})

# UI fonts (SIL Open Font License, see assets/fonts/OFL-*.txt), compiled in as BinaryData.
juce_add_binary_data(peak_fonts SOURCES
    assets/fonts/BarlowSemiCondensed-SemiBold.ttf
    assets/fonts/BarlowSemiCondensed-Medium.ttf
    assets/fonts/Barlow-Regular.ttf
    assets/fonts/IBMPlexMono-Regular.ttf)
set_target_properties(peak_fonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(PeakControl PRIVATE src)
target_compile_definitions(PeakControl PUBLIC
    PEAK_VERSION_LABEL="${PEAK_VERSION_LABEL}"
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0)

target_link_libraries(PeakControl
    PRIVATE
        peak_core
        peak_fonts
        juce::juce_audio_utils
        juce::juce_audio_devices
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# A local tool that draws every editor page to a PNG (not in CI): cmake -DPEAK_SNAPSHOT=ON
option(PEAK_SNAPSHOT "Build the page picture tool" OFF)
if(PEAK_SNAPSHOT)
    juce_add_console_app(peak_snapshot PRODUCT_NAME "peak_snapshot")
    target_sources(peak_snapshot PRIVATE tests/Snapshot.cpp)
    target_include_directories(peak_snapshot PRIVATE src)
    target_compile_definitions(peak_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Peak Control"
        PEAK_VERSION_LABEL="${PEAK_VERSION_LABEL}" PEAK_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/docs/dumps")
    target_link_libraries(peak_snapshot PRIVATE PeakControl juce::juce_audio_utils juce::juce_audio_devices)
endif()
