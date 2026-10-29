add_library(chordslots_engine STATIC
  src/scales.cpp
  src/chord.cpp
  src/progression.cpp
)

target_include_directories(chordslots_engine PUBLIC include)

if(MSVC)
  target_compile_options(chordslots_engine PRIVATE /W4)
else()
  target_compile_options(chordslots_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
