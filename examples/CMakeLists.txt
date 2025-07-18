# Compression options - default ON for both
option(ENABLE_MINIZ_COMPRESSION "Enable miniz (Flate) compression support" ON)
option(ENABLE_TOOJPEG_COMPRESSION "Enable toojpeg (JPEG) compression support" ON)

# Set up includes and sources based on enabled compression options
set(FOVIEW_SOURCES foview.cpp)
set(FOVIEW_INCLUDES "")

if (ENABLE_MINIZ_COMPRESSION)
  list(APPEND FOVIEW_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/miniz")
  list(APPEND FOVIEW_SOURCES miniz/miniz.c)
  add_definitions(-DENABLE_MINIZ_COMPRESSION)
endif()

if (ENABLE_TOOJPEG_COMPRESSION)
  list(APPEND FOVIEW_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/toojpeg")
  list(APPEND FOVIEW_SOURCES toojpeg/toojpeg.cpp)
  add_definitions(-DENABLE_TOOJPEG_COMPRESSION)
endif()

include_directories(${FOVIEW_INCLUDES})
add_executable(foview ${FOVIEW_SOURCES})

if (M_LIBRARY)
  target_link_libraries(foview ${M_LIBRARY})
endif (M_LIBRARY)

add_subdirectory(fonts)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
