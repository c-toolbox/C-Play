#
# Copyright: 2024 Erik Sunden <eriksunden85@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

if(CPLAY_INSTALL_DEPENDENCIES)
	install(CODE "set(PROJECT_SOURCE_DIR \"${PROJECT_SOURCE_DIR}\")")
	install(CODE [[
		if(EXISTS "${PROJECT_SOURCE_DIR}/CPlay_Pack.nsi")
			file(INSTALL
				DESTINATION "${CMAKE_INSTALL_PREFIX}"
				FILES "${PROJECT_SOURCE_DIR}/CPlay_Pack.nsi"
			)
		else()
			message("${PROJECT_SOURCE_DIR}/CPlay_Pack.nsi was not found")
		endif()
		if(EXISTS "${PROJECT_SOURCE_DIR}/LICENSE.txt")
			file(INSTALL
				DESTINATION "${CMAKE_INSTALL_PREFIX}"
				FILES "${PROJECT_SOURCE_DIR}/LICENSE.txt"
			)
		else()
			message("${PROJECT_SOURCE_DIR}/LICENSE.txt was not found")
		endif()
	]])
	install(CODE [[
		message("Copying ${PROJECT_SOURCE_DIR}/help/sgct files to bin")
	]])
	install(DIRECTORY "${PROJECT_SOURCE_DIR}/help/sgct/" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
endif()
