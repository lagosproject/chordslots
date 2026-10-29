# Catch2 v3: se usa la instalada en el sistema si existe; si no, se descarga.
find_package(Catch2 3 QUIET)

if(NOT Catch2_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.5.2
  )
  FetchContent_MakeAvailable(Catch2)
  list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
endif()

add_executable(engine_tests
  test_scales.cpp
  test_chord.cpp
  test_progression.cpp
)

target_link_libraries(engine_tests PRIVATE chordslots_engine Catch2::Catch2WithMain)

include(CTest)
include(Catch)
catch_discover_tests(engine_tests)
