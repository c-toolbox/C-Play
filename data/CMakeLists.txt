#
# Copyright: 2024 Erik Sunden <eriksunden85@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

install(FILES org.ctoolbox.cplay.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES org.ctoolbox.cplay.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

if(CPLAY_INSTALL_DEPENDENCIES)
    option(CPLAY_INSTALL_ITEMS_TO_DATA "Copy specific data items to C-Play" ON)
    if(CPLAY_INSTALL_ITEMS_TO_DATA)
		get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
		get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
		get_filename_component(_qt_dir "${_qt_bin_dir}" DIRECTORY)
		install(CODE "set(PROJECT_SOURCE_DIR \"${PROJECT_SOURCE_DIR}\")")
		install(CODE "set(QT_BIN_DIR \"${_qt_bin_dir}\")")
		install(CODE [[
            message("Copying Color Schemes Icons")
        ]])
        install(DIRECTORY "${PROJECT_SOURCE_DIR}/data/color-schemes" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data")
		install(CODE [[
            message("Copying Breeze Icons")
        ]])
        install(DIRECTORY "${_qt_bin_dir}/data/icons" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data")
        install(CODE [[
            message("Copying MPV Configs")
        ]])
        install(DIRECTORY "${PROJECT_SOURCE_DIR}/data/mpv-conf" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data")
        install(CODE [[
			message("Copying Data Items")
			file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data/images" FILES "${PROJECT_SOURCE_DIR}/data/images/C_play_transparent.ico")
			file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data/images" FILES "${PROJECT_SOURCE_DIR}/data/images/C_play_transparent.png")
			file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data/images" FILES "${PROJECT_SOURCE_DIR}/data/images/icon.rc")
            if(EXISTS "${PROJECT_SOURCE_DIR}/data/http-server-conf.json")
                file(INSTALL
                    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data"
                    FILES "${PROJECT_SOURCE_DIR}/data/http-server-conf.json"
                )
            else()
                message("${PROJECT_SOURCE_DIR}/data/http-server-conf.json was not found")
            endif()
            if(EXISTS "${QT_BIN_DIR}/data/icontheme.rcc")
                file(INSTALL
                    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data"
                    FILES "${QT_BIN_DIR}/data/icontheme.rcc"
                )
            else()
                message("${QT_BIN_DIR}/data/icontheme.rcc was not found")
            endif()
        ]])
		install(DIRECTORY DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/data/log")
    endif() 
endif()
