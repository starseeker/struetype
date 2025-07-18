# ProFont (relatively small and simple font)
add_test(NAME ProFont COMMAND foview ${CMAKE_CURRENT_SOURCE_DIR}/profont/ProFont.ttf)

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
