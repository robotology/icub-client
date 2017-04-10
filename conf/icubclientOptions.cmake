# Copyright: (C) 2014 WYSIWYD Consortium
# Authors: Ugo Pattacini
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

if(MSVC)

   if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
      string(REGEX REPLACE "/W[0-4]" "/W2" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
   else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W2")
   endif()

   add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
   set(CMAKE_DEBUG_POSTFIX "d")

elseif(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

   set(WARNINGS_LIST -Wall)
   foreach(WARNING ${WARNINGS_LIST})
      set(WARNINGS_STRING "${WARNINGS_STRING} ${WARNING}")
   endforeach(WARNING)
   set(WARNINGS_STRING "${WARNINGS_STRING} -Wno-deprecated-declarations")

   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${WARNINGS_STRING}")

   include(CheckCXXCompilerFlag)
   CHECK_CXX_COMPILER_FLAG("-std=c++11" COMPILER_SUPPORTS_CXX11)
   CHECK_CXX_COMPILER_FLAG("-std=c++0x" COMPILER_SUPPORTS_CXX0X)
   if(COMPILER_SUPPORTS_CXX11)
       message(STATUS "Enabled C++11 support for compiler ${CMAKE_CXX_COMPILER}.")
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
   elseif(COMPILER_SUPPORTS_CXX0X)
       message(STATUS "Enabled C++0x support for compiler ${CMAKE_CXX_COMPILER}.")
       set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
   else()
       message(SEND_ERROR "The compiler ${CMAKE_CXX_COMPILER} has no C++11 support. Please use a different C++ compiler, and welcome to the modern world.")
   endif()

endif()

if(NOT CMAKE_CONFIGURATION_TYPES)
    if(NOT CMAKE_BUILD_TYPE)
       set(CMAKE_BUILD_TYPE "Release" CACHE STRING
           "Choose the build type, recommanded options are: Debug or Release" FORCE)
    endif()
    set(ICUBCLIENT_BUILD_TYPES "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${ICUBCLIENT_BUILD_TYPES})
endif()

