# Copyright: (C) 2015 WYSIWYD Consortium
# Authors: Grégoire Pointeau
# CopyPolicy: Released under the terms of the GNU GPL v2.0.

set(appname opcPopulator)
file(GLOB ini      ${CMAKE_CURRENT_SOURCE_DIR}/conf/*.*)
file(GLOB template ${CMAKE_CURRENT_SOURCE_DIR}/scripts/*.template)

yarp_install(FILES ${ini}      DESTINATION ${ICUBCLIENT_CONTEXTS_INSTALL_DIR}/${appname})
yarp_install(FILES ${template} DESTINATION ${ICUBCLIENT_APPLICATIONS_TEMPLATES_INSTALL_DIR})
