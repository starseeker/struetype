include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/miniz"
  "${CMAKE_CURRENT_SOURCE_DIR}/toojpeg"
  )
add_executable(foview foview.cpp miniz/miniz.c toojpeg/toojpeg.cpp)
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
