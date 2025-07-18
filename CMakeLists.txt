# Minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set CMake project name
project(STT)

include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  )

if (NOT MSVC)
  add_definitions(-g)
endif (NOT MSVC)

find_library(M_LIBRARY m)

include(CTest)
add_subdirectory(tests)
add_subdirectory(examples)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
