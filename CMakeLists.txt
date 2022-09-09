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

add_subdirectory(CVE-2020-6617)
add_subdirectory(CVE-2020-6618)
add_subdirectory(CVE-2020-6619)
add_subdirectory(CVE-2020-6620)
add_subdirectory(CVE-2020-6621)
add_subdirectory(CVE-2020-6622)
add_subdirectory(CVE-2020-6623)
add_subdirectory(CVE-2022-25514)
add_subdirectory(CVE-2022-25515)
add_subdirectory(CVE-2022-25516)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
