get_filename_component(EUDAQ_USERNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(EUDAQ_MODULE ${EUDAQ_PREFIX}module_${EUDAQ_USERNAME})

ADD_LIBRARY(${EUDAQ_MODULE} SHARED AdeniumRaw2StdEventConverter.cpp)
TARGET_LINK_LIBRARIES(${EUDAQ_MODULE} PUBLIC ${EUDAQ_CORE_LIBRARY})

install(TARGETS
  ${EUDAQ_MODULE}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
