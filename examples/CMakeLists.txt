include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}/miniz"
  "${CMAKE_CURRENT_SOURCE_DIR}/toojpeg"
  )

# Option to enable PDF image compression
option(PDFIMG_ENABLE_COMPRESSION "Enable PDF image stream compression using miniz" OFF)

add_executable(foview foview.cpp miniz/miniz.c toojpeg/toojpeg.cpp)
if (M_LIBRARY)
  target_link_libraries(foview ${M_LIBRARY})
endif (M_LIBRARY)

# Add a compressed version of genpng if compression is enabled
if(PDFIMG_ENABLE_COMPRESSION)
  add_executable(genpng_compressed genpng.c miniz/miniz.c)
  target_compile_definitions(genpng_compressed PRIVATE PDFIMG_ENABLE_COMPRESSION)
  if (M_LIBRARY)
    target_link_libraries(genpng_compressed ${M_LIBRARY})
  endif (M_LIBRARY)
endif()

# ProFont (relatively small and simple font)
add_test(NAME ProFontGrid COMMAND foview ${CMAKE_CURRENT_SOURCE_DIR}/fonts/profont/ProFont.ttf)

if(0)
# DejaUu ttf files
file(GLOB DEJAVU_TTFS "${CMAKE_CURRENT_SOURCE_DIR}/fonts/dejavu/ttf/*.ttf")
foreach(TTF ${DEJAVU_TTFS})
  get_filename_component(TTFNAME ${TTF} NAME_WE)
  add_test(
    NAME "${TTFNAME}"
    COMMAND foview ${TTF}
  )
endforeach()
endif(0)

# STIX ttf files
file(GLOB STIX_TTFS "${CMAKE_CURRENT_SOURCE_DIR}/fonts/stix/ttf/*.ttf")
foreach(TTF ${STIX_TTFS})
  get_filename_component(TTFNAME ${TTF} NAME_WE)
  add_test(
    NAME "${TTFNAME}"
    COMMAND foview ${TTF}
  )
endforeach()

if(0)
# GNU Unifont ttf files (large character coverage)
file(GLOB UNIFONT_TTFS "${CMAKE_CURRENT_SOURCE_DIR}/fonts/unifont/*.ttf")
foreach(TTF ${UNIFONT_TTFS})
  get_filename_component(TTFNAME ${TTF} NAME_WE)
  add_test(
    NAME "${TTFNAME}"
    COMMAND foview ${TTF}
  )
endforeach()
endif(0)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
