add_executable(genpng genpng.c)
if (M_LIBRARY)
  target_link_libraries(genpng ${M_LIBRARY})
endif (M_LIBRARY)

add_test(NAME ProFontGrid COMMAND genpng ${CMAKE_CURRENT_SOURCE_DIR}/profont/ProFont.ttf)

# Discover all .ttf files in dejavu/ttf, create test for each
file(GLOB DEJAVU_TTFS "${CMAKE_CURRENT_SOURCE_DIR}/dejavu/ttf/*.ttf")
foreach(TTF ${DEJAVU_TTFS})
  get_filename_component(TTFNAME ${TTF} NAME_WE)
  add_test(
    NAME "DejaVuGrid_${TTFNAME}"
    COMMAND genpng ${TTF}
  )
endforeach()

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
