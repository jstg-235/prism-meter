cmake_minimum_required(VERSION 3.22)

project(PrismMeter VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(PRISM_BUILD_PLUGIN "Build the JUCE plugin (downloads JUCE)" ON)
option(PRISM_BUILD_TESTS "Build the unit and compliance tests (downloads GoogleTest)" ON)

include(FetchContent)

# The DSP core has no JUCE dependency, so it builds and tests fast everywhere.
add_subdirectory(core)

if(PRISM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(PRISM_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
