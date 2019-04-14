# Copyright: (C) 2014 WYSIWYD Consortium
# Authors: Matej Hoffmann
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

set(ICUBCLIENT_BUILD_RFH TRUE CACHE BOOL "Enable compilation of referenceFrameHandler")

if(ICUB_USE_IPOPT AND ICUBCLIENT_BUILD_RFH)
    add_subdirectory(referenceFrameHandler)
endif()

add_subdirectory(guiUpdater)
add_subdirectory(opcPopulator)
add_subdirectory(actionRecogDataDumper)
add_subdirectory(touchDetector)

