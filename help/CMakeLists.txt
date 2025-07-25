#
# Copyright: 2024-2025 Erik Sunden <eriksunden85@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

if(CPLAY_INSTALL_DEPENDENCIES)
	find_package(QT NAMES Qt6)
	if(${QT_VERSION_MAJOR} GREATER_EQUAL 6 AND ${QT_VERSION_MINOR} GREATER_EQUAL 8)
		set(CPLAY_INSTALL_PREDEFINED_DLL_NAME "qt68_ffmpeg5_dlls" CACHE STRING "qt68_ffmpeg5_dlls.txt chosen as predefined dll list")
	else()
		set(CPLAY_INSTALL_PREDEFINED_DLL_NAME "qt67_ffmpeg5_dlls" CACHE STRING "qt67_ffmpeg5_dlls.txt chosen as predefined dll list")
	endif()
	get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
	set_property(CACHE CPLAY_INSTALL_PREDEFINED_DLL_NAME PROPERTY STRINGS qt68_ffmpeg5_dlls qt68_ffmpeg7_dlls qt67_ffmpeg5_dlls qt67_ffmpeg6_dlls qt67_ffmpeg7_dlls)
	
	get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
	install(CODE "set(QT_BIN_DIR \"${_qt_bin_dir}\")")
	
	get_filename_component(_mpv_bin_dir "${Libmpv_LIBRARIES}" DIRECTORY)
	install(CODE "set(MPV_BIN_DIR \"${_mpv_bin_dir}\")")
	
	install(CODE "set(PROJECT_SOURCE_DIR \"${PROJECT_SOURCE_DIR}\")")
	install(CODE "set(CPLAY_INSTALL_PREDEFINED_DLL_NAME \"${CPLAY_INSTALL_PREDEFINED_DLL_NAME}\")")
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
	
	option(CPLAY_INSTALL_PREDEFINED_DLL_LIST "Shared dlls and delayed load is hard to detect. Copy predefined list" OFF)
    if(CPLAY_INSTALL_PREDEFINED_DLL_LIST)
        install(CODE [[
            message("Copying predefined DLL:s (${PROJECT_SOURCE_DIR}/help/dll_lists/${CPLAY_INSTALL_PREDEFINED_DLL_NAME}.txt) to install folder")
			file(READ "${PROJECT_SOURCE_DIR}/help/dll_lists/${CPLAY_INSTALL_PREDEFINED_DLL_NAME}.txt" DEPENDENCY_DLLS)
            string(REGEX REPLACE ";" "\\\\;" DEPENDENCY_DLLS "${DEPENDENCY_DLLS}")
            string(REGEX REPLACE "\n" ";" DEPENDENCY_DLLS "${DEPENDENCY_DLLS}")
            foreach(DLL_FILE ${DEPENDENCY_DLLS})
				if(EXISTS "${MPV_BIN_DIR}/${DLL_FILE}")
					message("Copying ${MPV_BIN_DIR}/${DLL_FILE}")
                    file(INSTALL
                        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                        FILES "${MPV_BIN_DIR}/${DLL_FILE}"
                    )
				elseif(EXISTS "${QT_BIN_DIR}/${DLL_FILE}")
					message("Copying ${QT_BIN_DIR}/${DLL_FILE}")
                    file(INSTALL
                        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                        FILES "${QT_BIN_DIR}/${DLL_FILE}"
                    )
                else()
                    message("${DLL_FILE} was not found")
                endif()
            endforeach()
        ]])
    endif()
endif()
