cmake_minimum_required(VERSION 3.22)
project(ChordSlots VERSION 0.4.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

option(CHORDSLOTS_BUILD_PLUGIN "Build the JUCE plugin (VST3 / LV2 / AU / Standalone)" ON)
option(CHORDSLOTS_BUILD_CLAP "Build the CLAP plugin using clap-juce-extensions" OFF)
set(CHORDSLOTS_CLAP_EXTENSIONS_SOURCE_DIR "" CACHE PATH
    "Optional pre-fetched clap-juce-extensions checkout (including recursive submodules)")
option(CHORDSLOTS_COPY_AFTER_BUILD "Install the plugin into the user plugin folders after building" OFF)
option(CHORDSLOTS_LTO "Link-time optimisation (slower builds, slightly smaller/faster binaries)" OFF)

# ------------------------------------------------------------------------------
# Core: header-only, STL-only. Everything musical lives here and is testable
# without a plugin host.
add_library(chordslots_core INTERFACE)
target_include_directories(chordslots_core INTERFACE core/include)
target_compile_features(chordslots_core INTERFACE cxx_std_17)

add_executable(chordslots-cli tools/cli.cpp)
target_link_libraries(chordslots-cli PRIVATE chordslots_core)

enable_testing()
add_executable(chordslots_core_tests tests/core_tests.cpp)
target_link_libraries(chordslots_core_tests PRIVATE chordslots_core)
add_test(NAME core COMMAND chordslots_core_tests)

add_executable(chordslots_performance_tests tests/performance_tests.cpp)
target_link_libraries(chordslots_performance_tests PRIVATE chordslots_core)
add_test(NAME performance COMMAND chordslots_performance_tests)

add_executable(chordslots_trigger_tests tests/trigger_tests.cpp)
target_link_libraries(chordslots_trigger_tests PRIVATE chordslots_core)
add_test(NAME trigger COMMAND chordslots_trigger_tests)

# Compare identical seed sets in the fast guard. The 1,000-roll reference is
# kept separately for phase-to-phase evaluation.
add_test(NAME eval COMMAND chordslots-cli eval 60 --compare "${CMAKE_CURRENT_SOURCE_DIR}/tests/eval_smoke_baseline.json")

if(NOT CHORDSLOTS_BUILD_PLUGIN)
    return()
endif()

# ------------------------------------------------------------------------------
# Linux without root: scripts/fetch-linux-headers.sh unpacks the X11/ALSA dev
# headers into third_party/sysroot. Use it if it is there.
set(CS_SYSROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/sysroot")
if(UNIX AND NOT APPLE AND EXISTS "${CS_SYSROOT}/usr/include")
    message(STATUS "ChordSlots: using local dev headers from ${CS_SYSROOT}")
    include_directories(SYSTEM "${CS_SYSROOT}/usr/include")
    set(ENV{PKG_CONFIG_PATH} "${CS_SYSROOT}/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

# ------------------------------------------------------------------------------
# JUCE (AGPLv3). A checkout in third_party/JUCE wins; otherwise it is fetched.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# CLAP is intentionally opt-in. Load its adapter after JUCE so its CMake
# integration can inspect the JUCE plugin configuration.
if(CHORDSLOTS_BUILD_CLAP)
    include(cmake/ClapSupport.cmake)
endif()

set(CS_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CS_FORMATS AU)
else()
    list(APPEND CS_FORMATS LV2)
endif()

juce_add_plugin(ChordSlots
    PRODUCT_NAME "ChordSlots"
    COMPANY_NAME "ChordSlots"
    BUNDLE_ID "org.chordslots.ChordSlots"
    PLUGIN_MANUFACTURER_CODE Chsl
    PLUGIN_CODE Chs1
    FORMATS ${CS_FORMATS}
    # An instrument that also emits MIDI: it makes sound by itself (built-in
    # preview synth) and can drive any other instrument.
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Generator
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    LV2URI "https://chordslots.org/plugins/chordslots"
    COPY_PLUGIN_AFTER_BUILD ${CHORDSLOTS_COPY_AFTER_BUILD})

set(CS_PLUGIN_SOURCES
    plugin/PluginProcessor.cpp
    plugin/PluginEditor.cpp
    plugin/PreviewSynth.cpp
    plugin/ui/LookAndFeel.cpp
    plugin/ui/TimelineView.cpp
    plugin/ui/LanesView.cpp
    plugin/ui/MidiDragTile.cpp
    plugin/ui/ChordPickerOverlay.cpp)

set(CS_PIANO_ASSETS plugin/assets/acoustic-piano/ATTRIBUTION.txt)
foreach(root RANGE 21 87 6)
    list(APPEND CS_PIANO_ASSETS "plugin/assets/acoustic-piano/piano_${root}.flac")
endforeach()
juce_add_binary_data(ChordSlotsPianoAssets NAMESPACE ChordSlotsPianoAssets
    HEADER_NAME ChordSlotsPianoAssets.h SOURCES ${CS_PIANO_ASSETS})
set_target_properties(ChordSlotsPianoAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

function(cs_configure_plugin_sources target)
    target_sources(${target} PRIVATE ${CS_PLUGIN_SOURCES})
    target_include_directories(${target} PRIVATE plugin)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(${target}
        PRIVATE chordslots_core juce::juce_audio_utils ChordSlotsPianoAssets
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(CHORDSLOTS_LTO)
        target_link_libraries(${target} PUBLIC juce::juce_recommended_lto_flags)
    endif()
endfunction()

cs_configure_plugin_sources(ChordSlots)

if(NOT ANDROID AND NOT IOS)
    target_sources(ChordSlots_Standalone PRIVATE plugin/StandaloneApp.cpp)
    target_compile_definitions(ChordSlots_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
endif()

if(CHORDSLOTS_BUILD_CLAP)
    clap_juce_extensions_plugin(TARGET ChordSlots
        CLAP_ID "org.chordslots.ChordSlots"
        CLAP_FEATURES instrument)

    # Host-side smoke loads the built CLAP artifact and exercises its MIDI ports.
    add_executable(chordslots_clap_midi_smoke tools/clap_midi_smoke.cpp)
    target_include_directories(chordslots_clap_midi_smoke PRIVATE
        "${clap_juce_extensions_SOURCE_DIR}/clap-libs/clap/include")
    if(CMAKE_DL_LIBS)
        target_link_libraries(chordslots_clap_midi_smoke PRIVATE ${CMAKE_DL_LIBS})
    endif()
    add_dependencies(chordslots_clap_midi_smoke ChordSlots_CLAP)
    add_test(NAME clap_midi_smoke
        COMMAND chordslots_clap_midi_smoke "$<TARGET_FILE:ChordSlots_CLAP>")
endif()

# Logic's AU MIDI FX slot cannot use the MusicDevice AU's MIDI output routing.
# Build a distinct AU identity while retaining the optional CLAP target above.
if(APPLE)
    juce_add_plugin(ChordSlotsMidiFX
        PRODUCT_NAME "ChordSlots MIDI FX"
        PLUGIN_NAME "ChordSlots MIDI FX"
        COMPANY_NAME "ChordSlots"
        BUNDLE_ID "org.chordslots.ChordSlotsMidiFX"
        PLUGIN_MANUFACTURER_CODE Chsl
        PLUGIN_CODE ChsM
        FORMATS AU
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT TRUE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        AU_MAIN_TYPE kAudioUnitType_MIDIProcessor
        COPY_PLUGIN_AFTER_BUILD ${CHORDSLOTS_COPY_AFTER_BUILD})

    target_compile_definitions(ChordSlotsMidiFX PUBLIC CHORDSLOTS_MIDI_FX=1)
    cs_configure_plugin_sources(ChordSlotsMidiFX)

    add_executable(chordslots_midi_fx_tests tests/midi_fx_tests.cpp)
    target_include_directories(chordslots_midi_fx_tests PRIVATE $<TARGET_PROPERTY:ChordSlotsMidiFX,INCLUDE_DIRECTORIES>)
    target_compile_definitions(chordslots_midi_fx_tests PRIVATE $<TARGET_PROPERTY:ChordSlotsMidiFX,COMPILE_DEFINITIONS>)
    target_link_libraries(chordslots_midi_fx_tests PRIVATE ChordSlotsMidiFX chordslots_core)
    add_test(NAME midi_fx COMMAND chordslots_midi_fx_tests)
endif()

# ------------------------------------------------------------------------------
# Headless processor tests: fake host transport -> processBlock -> check MIDI/audio.
add_executable(chordslots_plugin_tests tests/plugin_tests.cpp)
# Reuse the plugin's own JUCE build (same headers, same flags) instead of compiling JUCE twice.
target_include_directories(chordslots_plugin_tests PRIVATE $<TARGET_PROPERTY:ChordSlots,INCLUDE_DIRECTORIES>)
target_compile_definitions(chordslots_plugin_tests PRIVATE $<TARGET_PROPERTY:ChordSlots,COMPILE_DEFINITIONS>)
target_link_libraries(chordslots_plugin_tests PRIVATE ChordSlots chordslots_core)
add_test(NAME plugin COMMAND chordslots_plugin_tests)
