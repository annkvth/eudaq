list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_LIST_DIR}/cmake)
get_filename_component(EUDAQ_USERNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(EUDAQ_MODULE ${EUDAQ_PREFIX}module_${EUDAQ_USERNAME})

option(USER_CMSPHASE2_BUILD "build cms phase2" OFF)

if(NOT USER_CMSPHASE2_BUILD)
  message(STATUS "user/cms-phase2 is NOT to be built (USER_CMSPHASE2_BUILD=OFF)")
  return()
else()
  message(STATUS "user/cms-phase2 is to be built (USER_CMSPHASE2_BUILD=ON)")
endif()


add_subdirectory(module)
