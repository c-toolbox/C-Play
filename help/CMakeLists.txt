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
	
	option(CPLAY_INSTALL_PREDEFINED_DLL_LIST_FROM_CRAFT "Shared dlls and delayed load is hard to detect. Copy predefined list" ON)
    if(CPLAY_INSTALL_PREDEFINED_DLL_LIST_FROM_CRAFT)
        install(CODE [[
            message("Copying predefined DLL:s (${PROJECT_SOURCE_DIR}/help/craft_dlls.txt) from Craft folder")
            file(READ "${PROJECT_SOURCE_DIR}/help/craft_dlls.txt" DEPENDENCY_DLLS)
            string(REGEX REPLACE ";" "\\\\;" DEPENDENCY_DLLS "${DEPENDENCY_DLLS}")
            string(REGEX REPLACE "\n" ";" DEPENDENCY_DLLS "${DEPENDENCY_DLLS}")
            foreach(DLL_FILE ${DEPENDENCY_DLLS})
                message("Copying ${QT_BIN_DIR}/${DLL_FILE}")
                if(EXISTS "${QT_BIN_DIR}/${DLL_FILE}")
                    file(INSTALL
                        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                        FILES "${QT_BIN_DIR}/${DLL_FILE}"
                    )
                else()
                    message("${QT_BIN_DIR}/${DLL_FILE} was not found")
                endif()
            endforeach()
        ]])
    endif() 
	
	option(CPLAY_INSTALL_PRECOMPILIED_JACK_DLL "Jack2 can be difficult. Here are some pre-compilied DLLs to install." ON)
    if(CPLAY_INSTALL_PRECOMPILIED_JACK_DLL)
		install(CODE [[
			message("Copying ${PROJECT_SOURCE_DIR}/help/jack DLLs to bin")
		]])
		install(DIRECTORY "${PROJECT_SOURCE_DIR}/help/jack/" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
	endif()
endif()
