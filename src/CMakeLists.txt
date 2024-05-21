#
# Copyright: 2021-2024 Erik Sunden <eriksunden85@gmail.com>
#
# SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

set(QML_IMPORT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/qml" CACHE STRING "Qt Creator 4.1 extra qml import paths" FORCE)

set(TARGET_NAME C-Play)

add_executable(${TARGET_NAME})
set_compile_options(${TARGET_NAME})

target_sources(${TARGET_NAME}
    PRIVATE
    application.cpp
    application.h
    haction.cpp
    haction.h
    httpserverthread.cpp
    httpserverthread.h
    main.cpp    
    mpvobject.cpp
    mpvobject.h
    qthelper.h
    playercontroller.cpp
    playercontroller.h
    playlist/playlistitem.cpp
    playlist/playlistitem.h
    playlist/playlistmodel.cpp
    playlist/playlistmodel.h
	renderthread.cpp
    renderthread.h
    track.cpp
    track.h
    tracksmodel.cpp
    tracksmodel.h
    worker.cpp
    worker.h
    qml.qrc
    ../data/images/images.qrc
    ../data/images/icon.rc
)

kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/audiosettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/gridsettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/imagesettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/locationsettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/mousesettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/playbacksettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/playlistsettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/userinterfacesettings.kcfgc)

target_include_directories(${TARGET_NAME} PRIVATE playlist)

target_compile_definitions(${TARGET_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

if(WIN32)
        set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()


option(SGCT_FREETYPE_SUPPORT "Build SGCT with Freetype2" OFF)
option(SGCT_DEP_INCLUDE_FREETYPE "Include FreeType library" OFF)
option(SGCT_BUILD_TESTS "Build SGCT tests" OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sgct)

set(TARGET_LIBRARIES
    Qt5::Core
    Qt5::DBus
    Qt5::Qml
    Qt5::Quick
    Qt5::QuickControls2

    KF5::ConfigCore
    KF5::ConfigGui
    KF5::CoreAddons
    KF5::FileMetaData
    KF5::I18n
    KF5::IconThemes
    KF5::KIOCore
    KF5::KIOFileWidgets
    KF5::KIOWidgets
    KF5::KIONTLM
    KF5::Kirigami2
    KF5::XmlGui

    Libmpv::Libmpv
    sgct
    glm
)

target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIBRARIES})

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)

install(TARGETS ${TARGET_NAME} DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

if(CPLAY_INSTALL_DEPENDENCIES)
    get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    get_filename_component(_qt_dir "${_qt_bin_dir}" DIRECTORY)
    get_filename_component(_mpv_bin_dir "${Libmpv_LIBRARIES}" DIRECTORY)

    install(CODE "set(PROJECT_SOURCE_DIR \"${PROJECT_SOURCE_DIR}\")")
    install(CODE "set(QT_BIN_DIR \"${_qt_bin_dir}\")")

    option(CPLAY_INSTALL_PLUGINS_FROM_CRAFT "Copy complete plugins folder for Qt/KDE" ON)   
    if(CPLAY_INSTALL_PLUGINS_FROM_CRAFT)
        install(CODE [[
            message("Copying Qt/KDE plugins folder")
        ]])
        install(DIRECTORY "${_qt_dir}/plugins" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
    endif()

    option(CPLAY_INSTALL_QML_FROM_CRAFT "Copy complete qml folder for Qt/KDE" ON)
    if(CPLAY_INSTALL_QML_FROM_CRAFT)
        install(CODE [[
            message("Copying Qt/KDE qml folder")
        ]])
        install(DIRECTORY "${_qt_dir}/qml" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")
    endif()

    option(CPLAY_INSTALL_PREDEFINED_DLL_LIST_FROM_CRAFT "Shared dlls and delayed load is hard to detect. Copy predefined list" ON)
    if(CPLAY_INSTALL_PREDEFINED_DLL_LIST_FROM_CRAFT)
        install(CODE [[
            message("Copying predefined DLL:s (${PROJECT_SOURCE_DIR}/data/craft_dlls.txt) from Craft folder")
            file(READ "${PROJECT_SOURCE_DIR}/data/craft_dlls.txt" DEPENDENCY_DLLS)
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

    option(CPLAY_INSTALL_GET_RUNTIME_DEPENDENCIES "When running INSTALL, look for dependencies with GET_RUNTIME_DEPENDENCIES and copy them" ON)
    if(CPLAY_INSTALL_GET_RUNTIME_DEPENDENCIES)
        set(MY_DEPENDENCY_PATHS ${_qt_bin_dir} ${_mpv_bin_dir})
        # Transfer the values into the install script
        install(CODE "set(MY_DEPENDENCY_PATHS \"${MY_DEPENDENCY_PATHS}\")")
        install(CODE "set(target \"$<TARGET_FILE:${TARGET_NAME}>\")")
        # Install dependencies
        install(CODE [[
            message("Looking for direct dependencies for C-Play.exe (In Qt and MPV build directories)")
            file(GET_RUNTIME_DEPENDENCIES
                EXECUTABLES ${target}
                PRE_EXCLUDE_REGEXES api-ms msvcp vcruntime kernel32
                POST_EXCLUDE_REGEXES C:[/\\]Windows[/\\]
                RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPENDENCIES
                UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPENDENCIES
                DIRECTORIES ${MY_DEPENDENCY_PATHS}
            )
            foreach(DEPENDENCY ${UNRESOLVED_DEPENDENCIES})
                message(WARNING "Unable to resolve dependency ${DEPENDENCY}")
            endforeach()
            message("Copying dependencies")
            foreach(DEPENDENCY ${RESOLVED_DEPENDENCIES})
                message("Copying ${DEPENDENCY}")
                file(INSTALL
                    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                    FILES "${DEPENDENCY}"
                )
            endforeach()
        ]])
    endif()
endif()