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

# Parameter table, CC / NRPN coding, patch SysEx, send queue: plain C++, no JUCE, tested on its own.
add_library(peak_core STATIC
    src/core/ParamTable.cpp
    src/core/Midi.cpp
    src/core/Sysex.cpp
    src/core/SendQueue.cpp)
target_include_directories(peak_core PUBLIC src)
set_target_properties(peak_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

enable_testing()
add_executable(peak_tests tests/CoreTests.cpp)
target_link_libraries(peak_tests PRIVATE peak_core)
target_compile_definitions(peak_tests PRIVATE PEAK_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/docs/dumps")
add_test(NAME core COMMAND peak_tests)

# The JUCE plugin (VST3 + standalone) comes in PLAN.md step 6, after the mockup is approved.
