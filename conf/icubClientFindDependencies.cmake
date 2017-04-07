# Copyright: (C) 2014 WYSIWYD Consortium
# Authors: Ugo Pattacini
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

macro(checkandset_dependency package)
    if(${package}_FOUND)
        set(ICUB_CLIENT_HAS_${package} TRUE CACHE BOOL "Package ${package} found" FORCE)
        set(ICUB_CLIENT_USE_${package} TRUE CACHE BOOL "Use package ${package}")
        message(STATUS "found ${package}")
    else()
        set(ICUB_CLIENT_HAS_${package} FALSE CACHE BOOL "" FORCE)
        set(ICUB_CLIENT_USE_${package} FALSE CACHE BOOL "Use package ${package}")
    endif()
    mark_as_advanced(ICUB_CLIENT_HAS_${package})
    
    if(NOT ${package}_FOUND AND ICUB_CLIENT_USE_${package})
        message("Warning: you requested to use the package ${package}, but it is unavailable (or was not found). This might lead to compile errors, we recommend you turn off the ICUB_CLIENT_USE_${package} flag.") 
    endif()
endmacro(checkandset_dependency)

find_package(OpenCV)
find_package(kinectWrapper QUIET)
find_package(Boost COMPONENTS chrono thread system QUIET)

message(STATUS "I have found the following libraries:")
checkandset_dependency(OpenCV)
checkandset_dependency(Boost)
checkandset_dependency(kinectWrapper)
