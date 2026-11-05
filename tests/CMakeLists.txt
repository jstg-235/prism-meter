FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(prism_core_tests
    BallisticsTests.cpp
    KWeightingTests.cpp
    LoudnessTests.cpp
    SpectrumTests.cpp
    StereoTests.cpp
    TruePeakTests.cpp
)

target_link_libraries(prism_core_tests PRIVATE prism_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(prism_core_tests)
