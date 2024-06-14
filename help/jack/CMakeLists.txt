#
# Copyright: 2024 Erik Sunden <eriksunden85@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

if(CPLAY_INSTALL_DEPENDENCIES)	
	option(CPLAY_INSTALL_PRECOMPILIED_JACK_DLL "Jack2 can be difficult. Here are some pre-compilied DLLs to install." ON)
    if(CPLAY_INSTALL_PRECOMPILIED_JACK_DLL)
		install(CODE [[
			message("Copying ${PROJECT_SOURCE_DIR}/help/jack DLLs to bin")
		]])
		install(DIRECTORY "${PROJECT_SOURCE_DIR}/help/jack/" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
	endif()
endif()
