# Copyright: (C) 2014 WYSIWYD Consortium
# Authors: Ugo Pattacini
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

cmake_minimum_required(VERSION 2.8.7)
if(COMMAND cmake_policy)
    cmake_policy(SET CMP0003 NEW)
endif()
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
project(icub-client)

find_package(YARP REQUIRED)
find_package(ICUB REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${YARP_MODULE_PATH}
                              ${ICUB_MODULE_PATH}
                              ${PROJECT_SOURCE_DIR}/conf)

message(STATUS "YARP is version: ${YARP_VERSION}")
message(STATUS "iCub is version: ${ICUB_VERSION}")

add_definitions(${YARP_DEFINES})
include(YarpInstallationHelpers)
yarp_configure_external_installation(icub_client)

include(iCubOptions)
include(iCubHelpers)

set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)
make_directory(${LIBRARY_OUTPUT_PATH})
mark_as_advanced(LIBRARY_OUTPUT_PATH)
message(STATUS "Libraries go to ${LIBRARY_OUTPUT_PATH}")

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)
make_directory(${EXECUTABLE_OUTPUT_PATH})
mark_as_advanced(EXECUTABLE_OUTPUT_PATH)
message(STATUS "Executables go to ${EXECUTABLE_OUTPUT_PATH}")

set(IDL_GENERATED_CODE_DIR ${PROJECT_SOURCE_DIR}/src/idl_generated_code)
mark_as_advanced(IDL_GENERATED_CODE_DIR)
message(STATUS "IDL generated files go to ${IDL_GENERATED_CODE_DIR}")

include(icubClientFindDependencies)
include(icubClientOptions)

#### add the "uninstall_icub-client" target
configure_file("${CMAKE_SOURCE_DIR}/cmake_uninstall.cmake.in"
               "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
               @ONLY)
add_custom_target(uninstall_icubClient "${CMAKE_COMMAND}" -P
                  "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake")
####

#### install cmake scripts
message(STATUS "Installing cmake scripts")
set(ICUB_CLIENT_CMAKE_SCRIPTS icubClientOptions.cmake)

foreach(conf_i ${ICUB_CLIENT_CMAKE_SCRIPTS})
    configure_file(${CMAKE_SOURCE_DIR}/conf/${conf_i}
                   ${CMAKE_BINARY_DIR}/conf/${conf_i} COPYONLY IMMEDIATE)
    install(FILES conf/${conf_i} DESTINATION share/icub-client/cmake/ COMPONENT Development)
endforeach()
####

add_subdirectory(src)
add_subdirectory(app)

