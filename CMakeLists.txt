CMAKE_MINIMUM_REQUIRED(VERSION 3.6.3 FATAL_ERROR)
IF(COMMAND CMAKE_POLICY)
    CMAKE_POLICY(SET CMP0003 NEW) # change linker path search behaviour
    CMAKE_POLICY(SET CMP0048 NEW) # set project version
    CMAKE_POLICY(SET CMP0065 OLD) # Use old linking behavior
    CMAKE_POLICY(SET CMP0057 NEW) # enable IN_LIST
    IF(${CMAKE_VERSION} VERSION_GREATER "3.13")
        CMAKE_POLICY(SET CMP0077 NEW) # allow overwriting options with normal variables
        CMAKE_POLICY(SET CMP0079 NEW) # Allow lookup of linking libraries in other directories
    ENDIF()
ENDIF(COMMAND CMAKE_POLICY)

project(EUDAQ VERSION 2.6.1)

include(CMakeDependentOption)
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    include(CTest)
endif()

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
include(CMakeCompatibility)
include(Platform)

set(EUDAQ_PREFIX "eudaq_")
set(EUDAQ_TARGET "core")
set(EUDAQ_CORE_LIBRARY "${EUDAQ_PREFIX}${EUDAQ_TARGET}")
# set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS)

include_directories(main/lib/core/include)
include_directories(extern/include)

set(EUDAQ_LIB_VERSION ${EUDAQ_VERSION_MAJOR}.${EUDAQ_VERSION_MINOR}.${EUDAQ_VERSION_PATCH})
set(EUDAQ_LIB_VERSION_BASE ${EUDAQ_VERSION_MAJOR}.${EUDAQ_VERSION_MINOR}.${EUDAQ_VERSION_PATCH})
find_package(Git QUIET)
if(GIT_FOUND AND (IS_DIRECTORY ${PROJECT_SOURCE_DIR}/.git))
  execute_process(COMMAND ${GIT_EXECUTABLE} --work-tree=${PROJECT_SOURCE_DIR} describe --tags HEAD OUTPUT_VARIABLE EUDAQ_GIT_VERSION ERROR_VARIABLE EUDAQ_GIT_ERROR_DESCRIBE)
  execute_process(COMMAND ${GIT_EXECUTABLE} --work-tree=${PROJECT_SOURCE_DIR} status --porcelain ${PROJECT_SOURCE_DIR}/main OUTPUT_VARIABLE EUDAQ_GIT_MAIN_STATUS ERROR_VARIABLE EUDAQ_GIT_ERROR_STATUS)
  if(EUDAQ_GIT_ERROR_DESCRIBE OR EUDAQ_GIT_ERROR_STATUS)
    message(WARNING "Could not determine EUDAQ version from git -- git execution resulted in error: ${EUDAQ_GIT_ERROR_DESCRIBE} : ${EUDAQ_GIT_ERROR_STATUS}")
  else()
    string(REGEX REPLACE "([v0-9.]+[-a-zA-Z0-9]+)-([0-9]+)-([A-Za-z0-9]+)" "\\1+\\2~\\3" EUDAQ_GIT_LIB_VERSION ${EUDAQ_GIT_VERSION})
    string(REGEX REPLACE "\n" "" EUDAQ_GIT_VERSION ${EUDAQ_GIT_VERSION}) # remove potential line breaks
    string(REGEX REPLACE "v([v0-9.]+)(.*)" "\\1" EUDAQ_GIT_VERSION_BASE ${EUDAQ_GIT_VERSION})
    if("${EUDAQ_GIT_VERSION_BASE}" VERSION_LESS "${EUDAQ_LIB_VERSION_BASE}")
      message(WARNING "Your do not seem to have fetched the latest tags in your git repository -- please consider running 'git fetch upstream'")
      message("EUDAQ_GIT_VERSION_BASE=${EUDAQ_GIT_VERSION_BASE} EUDAQ_LIB_VERSION_BASE=${EUDAQ_LIB_VERSION_BASE}")
    endif()
    if(NOT (EUDAQ_GIT_MAIN_STATUS STREQUAL ""))
      message("the directory ${PROJECT_SOURCE_DIR}/main is dirty (uncommitted changes present). ${EUDAQ_GIT_MAIN_STATUS}")
    endif()
    set(EUDAQ_LIB_VERSION ${EUDAQ_GIT_VERSION})
  endif()
endif()
message(STATUS "Determined EUDAQ version ${EUDAQ_LIB_VERSION}")

# Define the project with the simple version
string(REGEX MATCH "([0-9.]+)+" SIMPLE_VERSION "${EUDAQ_LIB_VERSION}")
project(eudaq VERSION ${SIMPLE_VERSION} LANGUAGES CXX)

configure_file("${PROJECT_SOURCE_DIR}/cmake/Config.cmake.hh" "${CMAKE_CURRENT_BINARY_DIR}/include/eudaq/Config.hh" @ONLY)
include_directories("${CMAKE_CURRENT_BINARY_DIR}/include")
file(GLOB INC_FILES "${CMAKE_CURRENT_BINARY_DIR}/include/eudaq/*.hh")
install(FILES ${INC_FILES} DESTINATION include/eudaq)

# set the path to which we will install later: default project home, can be changed using
set(EUDAQ_INSTALL_PREFIX "${PROJECT_SOURCE_DIR}" CACHE PATH "Prefix prepended to install directories")
set(CMAKE_INSTALL_PREFIX "${EUDAQ_INSTALL_PREFIX}" CACHE INTERNAL "Prefix prepended to install directories" FORCE)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_INSTALL_PREFIX}/extern)

# Set the correct build type and allow command line options:
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'RelWithDebInfo' as none was specified.")
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Choose the type of build." FORCE)
endif()

# find necessary threading libraries (using C++11 threading)
find_package(Threads REQUIRED)
set(EUDAQ_THREADS_LIB ${CMAKE_THREAD_LIBS_INIT})

# see http://www.cmake.org/Wiki/CMake_RPATH_handling
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
  set(EUDAQ_INSTALL_RPATH "@loader_path/../lib;@loader_path/../extern/lib")
else()
  set(EUDAQ_INSTALL_RPATH "\$ORIGIN/../lib:\$ORIGIN/../extern/lib")
endif()
set(CMAKE_SKIP_BUILD_RPATH  FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
set(CMAKE_INSTALL_RPATH ${EUDAQ_INSTALL_RPATH})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)




add_subdirectory(main)
add_subdirectory(extra)
add_subdirectory(doc)
add_subdirectory(gui)
add_subdirectory(monitors)
add_subdirectory(user)

# Uninstall Target #
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
endif()

include(PrintSystemInformation)


if(DEFINED EUDAQ_CORE_LIBRARY)
  list(APPEND CONF_EUDAQ_LIBRARIES eudaq_${EUDAQ_CORE_LIBRARY})
endif()

if(DEFINED EUDAQ_LCIO_LIBRARY)
  list(APPEND CONF_EUDAQ_LIBRARIES eudaq_${EUDAQ_LCIO_LIBRARY})
endif()

if(DEFINED EUDAQ_ROOT_MONITOR_LIBRARY)
  list(APPEND CONF_EUDAQ_LIBRARIES eudaq_${EUDAQ_ROOT_MONITOR_LIBRARY})
endif()

if(DEFINED EUDAQ_TTREE_LIBRARY)
  list(APPEND CONF_EUDAQ_LIBRARIES eudaq_${EUDAQ_TTREE_LIBRARY})
endif()

set(CONF_INCLUDE_DIRS "\${EUDAQ_INSTALL_DIR}/include \${EUDAQ_INSTALL_DIR}/extern/include")
set(EUDAQ_INCLUDE_DIRS "include")
set(EUDAQ_LIBRARIES_DIRS "lib")

include(CMakePackageConfigHelpers)
configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/eudaqConfig.cmake.in
                              ${CMAKE_CURRENT_BINARY_DIR}/eudaqConfig.cmake
                              INSTALL_DESTINATION cmake
                              PATH_VARS EUDAQ_INCLUDE_DIRS EUDAQ_LIBRARIES_DIRS
                              )

write_basic_package_version_file(eudaqConfigVersion.cmake
                                 VERSION ${PROJECT_VERSION}
                                 COMPATIBILITY AnyNewerVersion)

# Install the export set for use with the install-tree
install(EXPORT eudaqTargets
  NAMESPACE eudaq::
  DESTINATION cmake
  COMPONENT dev)

install(FILES
  "${PROJECT_BINARY_DIR}/eudaqConfig.cmake"
  "${PROJECT_BINARY_DIR}/eudaqConfigVersion.cmake"
  DESTINATION cmake
  COMPONENT dev)

message(STATUS "Check your configuration/flags with: cmake -L")
