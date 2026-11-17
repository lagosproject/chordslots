# Optional CLAP format support through the pinned, MIT-licensed upstream JUCE adapter.
# CMake's standard FETCHCONTENT_SOURCE_DIR_CLAP_JUCE_EXTENSIONS variable can
# point to a pre-fetched recursive checkout for offline builds.
if(NOT CHORDSLOTS_BUILD_CLAP)
    return()
endif()

include(FetchContent)
set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(clap_juce_extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG 3837f4a6ed2ab0ba46aba69ae5b7041c7c64ce74
    GIT_SUBMODULES_RECURSE TRUE)
FetchContent_MakeAvailable(clap_juce_extensions)
