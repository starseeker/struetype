add_executable(genpng genpng.c)
if (M_LIBRARY)
  target_link_libraries(genpng ${M_LIBRARY})
endif (M_LIBRARY)

add_test(NAME ProFontGrid COMMAND genpng ${CMAKE_CURRENT_SOURCE_DIR}/profont/ProFont.ttf)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
