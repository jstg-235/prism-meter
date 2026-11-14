# JUCE is pinned to an exact release. Point PRISM_JUCE_DIR at a local checkout
# to build offline.
set(PRISM_JUCE_DIR "" CACHE PATH "Optional local JUCE checkout")

if(PRISM_JUCE_DIR)
    add_subdirectory(${PRISM_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# Product target: macOS, VST3 + AU. Standalone is kept for testing without a DAW.
# Other platforms build VST3 + Standalone for development only.
set(PRISM_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PRISM_FORMATS AU)
endif()

# AAX is planned for the end of the roadmap. It needs the Avid SDK (developer
# account) and PACE signing. Set PRISM_AAX_SDK to try it early.
set(PRISM_AAX_SDK "" CACHE PATH "Path to the Avid AAX SDK")
if(PRISM_AAX_SDK)
    juce_set_aax_sdk_path(${PRISM_AAX_SDK})
    list(APPEND PRISM_FORMATS AAX)
endif()

juce_add_plugin(PrismMeter
    COMPANY_NAME "New Constellation"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.newconstellation.prismmeter"
    PLUGIN_MANUFACTURER_CODE NwCn
    PLUGIN_CODE PrMt
    PRODUCT_NAME "Prism Meter"
    FORMATS ${PRISM_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Analyzer
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_None
)

target_sources(PrismMeter PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/MeterViews.cpp
    Source/ui/MeterModules.cpp
)

target_include_directories(PrismMeter PRIVATE Source)

target_compile_definitions(PrismMeter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(PrismMeter
    PRIVATE
        prism_core
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Optional developer tool that renders the editor to a PNG with a synthetic signal.
option(PRISM_BUILD_SNAPSHOT_TOOL "Build the PrismSnapshot screenshot tool" OFF)

if(PRISM_BUILD_SNAPSHOT_TOOL)
    juce_add_console_app(PrismSnapshot PRODUCT_NAME "PrismSnapshot")

    target_sources(PrismSnapshot PRIVATE
        ${CMAKE_SOURCE_DIR}/tools/Snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/MeterViews.cpp
        Source/ui/MeterModules.cpp
    )

    target_include_directories(PrismSnapshot PRIVATE Source)

    target_compile_definitions(PrismSnapshot PRIVATE
        JucePlugin_Name="Prism Meter"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_MODAL_LOOPS_PERMITTED=1
    )

    target_link_libraries(PrismSnapshot PRIVATE
        prism_core
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
    )
endif()
