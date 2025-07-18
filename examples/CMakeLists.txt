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

# ProFont (relatively small and simple font)
add_test(NAME ProFontGrid COMMAND foview ${CMAKE_CURRENT_SOURCE_DIR}/profont/ProFont.ttf)

# DejaUu ttf files
file(GLOB DEJAVU_TTFS "${CMAKE_CURRENT_SOURCE_DIR}/dejavu/ttf/*.ttf")
foreach(TTF ${DEJAVU_TTFS})
  get_filename_component(TTFNAME ${TTF} NAME_WE)
  add_test(
    NAME "${TTFNAME}"
    COMMAND foview ${TTF}
  )
endforeach()

# STIX ttf files
file(GLOB STIX_TTFS "${CMAKE_CURRENT_SOURCE_DIR}/stix/ttf/*.ttf")
foreach(TTF ${STIX_TTFS})
  get_filename_component(TTFNAME ${TTF} NAME_WE)
  add_test(
    NAME "${TTFNAME}"
    COMMAND foview ${TTF}
  )
endforeach()

# GNU Unifont ttf files (large character coverage)
file(GLOB UNIFONT_TTFS "${CMAKE_CURRENT_SOURCE_DIR}/unifont/*.ttf")
foreach(TTF ${UNIFONT_TTFS})
  get_filename_component(TTFNAME ${TTF} NAME_WE)
  add_test(
    NAME "${TTFNAME}"
    COMMAND foview ${TTF}
  )
endforeach()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
