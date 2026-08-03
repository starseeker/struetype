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

add_executable(struetype-hardening hardening.c)
if(M_LIBRARY)
  target_link_libraries(struetype-hardening ${M_LIBRARY})
endif()
struetype_harden_target(struetype-hardening)
add_test(NAME struetype-hardening
  COMMAND struetype-hardening
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/fonts/profont/ProFont.ttf)

add_executable(struetype-allocation-failure allocation_failure.c)
if(M_LIBRARY)
  target_link_libraries(struetype-allocation-failure ${M_LIBRARY})
endif()
struetype_harden_target(struetype-allocation-failure)
add_test(NAME struetype-allocation-failure
  COMMAND struetype-allocation-failure
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/fonts/profont/ProFont.ttf)

# Local Variables:
# tab-width: 8
# mode: cmake
# indent-tabs-mode: t
# End:
# ex: shiftwidth=2 tabstop=8
