cmake_minimum_required(VERSION 3.20)
project(chordslots VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(CHORDSLOTS_BUILD_TESTS "Compilar las pruebas del motor" ON)

add_subdirectory(engine)

if(CHORDSLOTS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(engine/tests)
endif()
