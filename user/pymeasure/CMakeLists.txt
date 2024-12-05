get_filename_component(EUDAQ_USERNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(EUDAQ_MODULE module_${EUDAQ_USERNAME})

option(USER_PYMEASURE_BUILD "build user/pymeasure" ON)

if(NOT USER_PYMEASURE_BUILD)
    message(STATUS "user/pymeasure is NOT to be built (USER_PYMEASURE_BUILD=OFF)")
  return()
endif()

message(STATUS "user/pymeasure is to be built (USER_PYMEASURE_BUILD=ON)")

#add_subdirectory(module)
