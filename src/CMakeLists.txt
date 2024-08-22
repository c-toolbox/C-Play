#
# Copyright: 2021-2024 Erik Sunden <eriksunden85@gmail.com>
#
# SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

set(TARGET_NAME C-Play)
set(CPLAY_VERSION_MAJOR 2)
set(CPLAY_VERSION_MINOR 1)
set(CPLAY_VERSION_PATCH 0)

add_executable(${TARGET_NAME})
set_compile_options(${TARGET_NAME})
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(SOURCE_FILES
    application.cpp
    application.h
    haction.cpp
    haction.h
    httpserverthread.cpp
    httpserverthread.h
    layers/baselayer.cpp
    layers/baselayer.h
    layers/imagelayer.cpp
    layers/imagelayer.h
    layers/mpvlayer.cpp
    layers/mpvlayer.h
	layerqtitem.cpp
	layerqtitem.h
    layerrenderer.cpp
    layerrenderer.h
    layersmodel.cpp
    layersmodel.h
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
)

set(KCFCG_FILE_NAMES
    audiosettings
    gridsettings
    imagesettings
    locationsettings
    mousesettings
    playbacksettings
    playlistsettings
    userinterfacesettings
)
set(KCFCG_MOC_FILES "")
set(KCFCG_FILES "")
foreach(KCFCG_NAME ${KCFCG_FILE_NAMES})
	set(KCFCG_FILE settings/${KCFCG_NAME}.kcfgc)
    kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC ${KCFCG_FILE})
	list(APPEND KCFCG_FILES ${KCFCG_FILE})
	list(APPEND KCFCG_MOC_FILES ${CMAKE_CURRENT_BINARY_DIR}/${KCFCG_NAME}.moc)
endforeach()
source_group(Kcfcg FILES ${KCFCG_FILES})
source_group(${AUTO_SOURCE_NAME} FILES ${KCFCG_MOC_FILES})

if(BUILD_CPLAY_WITH_NDI)
    list(APPEND SOURCE_FILES ndi/ndilayer.cpp)
    list(APPEND SOURCE_FILES ndi/ndilayer.h)
    set(OFX_NDI_FILES
        ndi/ofxNDI/ofxNDIdynloader.cpp
        ndi/ofxNDI/ofxNDIdynloader.h
        ndi/ofxNDI/ofxNDIplatforms.h
        ndi/ofxNDI/ofxNDIreceive.cpp
        ndi/ofxNDI/ofxNDIreceive.h
        ndi/ofxNDI/ofxNDIutils.cpp
        ndi/ofxNDI/ofxNDIutils.h
    )
    source_group(ofxNDI FILES ${OFX_NDI_FILES})
    list(APPEND SOURCE_FILES ${OFX_NDI_FILES})
endif()

if(BUILD_CPLAY_WITH_QT6)
    target_sources(${TARGET_NAME} 
        PRIVATE 
        ${SOURCE_FILES}
        ../data/images/images.qrc
        ../data/images/icon.rc
    )

    qt_policy(SET QTP0001 NEW)

    set(QML_SRC_FILES
        qml/qt6/Components/ImageAdjustmentSlider.qml
        qml/qt6/Components/LabelWithTooltip.qml
        qml/qt6/Components/SelectActionPopup.qml
        qml/qt6/Components/SettingsBasePage.qml
        qml/qt6/Components/SettingsHeader.qml
        qml/qt6/Menus/AudioMenu.qml
        qml/qt6/Menus/FileMenu.qml
        qml/qt6/Menus/HelpMenu.qml
        qml/qt6/Menus/PlaybackMenu.qml
        qml/qt6/Menus/SettingsMenu.qml
        qml/qt6/Settings/AudioSettings.qml
        qml/qt6/Settings/GridSettings.qml
        qml/qt6/Settings/ImageSettings.qml
        qml/qt6/Settings/LocationSettings.qml
        qml/qt6/Settings/MouseSettings.qml
        qml/qt6/Settings/Navigation.qml
        qml/qt6/Settings/PlaybackSettings.qml
        qml/qt6/Settings/PlaylistSettings.qml
        qml/qt6/Settings/UserInterfaceSettings.qml
        qml/qt6/Settings/SettingsEditor.qml
        qml/qt6/Actions.qml
        qml/qt6/Footer.qml
        qml/qt6/HProgressBar.qml
        qml/qt6/Header.qml
        qml/qt6/MpvVideo.qml
        qml/qt6/Osd.qml
        qml/qt6/PlayList.qml
        qml/qt6/PlayListItemCompact.qml
        qml/qt6/VolumeSlider.qml
        qml/qt6/Main.qml
        qml/qt6/PlaySections.qml
        qml/qt6/PlaySectionsItemCompact.qml
        qml/qt6/VisibilitySlider.qml
        qml/qt6/Layers.qml
        qml/qt6/LayersItemCompact.qml
		qml/qt6/LayerView.qml
        qml/qt6/BackgroundImage.qml
        qml/qt6/ForegroundImage.qml
        qml/qt6/SaveAsCPlayFile.qml
        qml/qt6/ViewPlaylistItem.qml
    )

    qt_add_qml_module(${TARGET_NAME}
        URI org.ctoolbox.cplay
        QML_FILES
        ${QML_SRC_FILES}
    )
    source_group(Qml FILES ${QML_SRC_FILES})
	
	set(RCC_FILE_DIR ${CMAKE_CURRENT_BINARY_DIR}/.rcc)
	set(QMLCACHE_FILE_DIR ${RCC_FILE_DIR}/qmlcache)
	set(QMLCACHE_FILES "")
	foreach(QML_SRC_FILE ${QML_SRC_FILES})
		string(REPLACE ".qml" "_qml.cpp" QML_CACHE_FILE ${QML_SRC_FILE})
		list(APPEND QMLCACHE_FILES ${QMLCACHE_FILE_DIR}/C-Play_${QML_CACHE_FILE})
	endforeach()
	list(APPEND QMLCACHE_FILES ${RCC_FILE_DIR}/qrc_cplay_raw_qml_0.cpp)
	list(APPEND QMLCACHE_FILES ${RCC_FILE_DIR}/qrc_C-Play_raw_qml_0.cpp)
	list(APPEND QMLCACHE_FILES ${RCC_FILE_DIR}/qrc_qmake_org_ctoolbox_cplay.cpp)
	list(APPEND QMLCACHE_FILES ${QMLCACHE_FILE_DIR}/C-Play_qml_loader_file_list.cpp)
	list(APPEND QMLCACHE_FILES ${QMLCACHE_FILE_DIR}/C-Play_qmlcache_loader.cpp)
	list(APPEND QMLCACHE_FILES ${CMAKE_CURRENT_BINARY_DIR}/C-play_qmltyperegistrations.cpp)
	source_group(${AUTO_SOURCE_NAME} FILES ${QMLCACHE_FILES})

    set(TARGET_LIBRARIES
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2

        KF6::ColorScheme
        KF6::ConfigCore
        KF6::ConfigGui
        KF6::CoreAddons
        KF6::FileMetaData
        KF6::I18n
        KF6::IconThemes
        KF6::KIOWidgets
        KF6::Kirigami
        KF6::WindowSystem
        KF6::XmlGui

        Libmpv::Libmpv
        sgct
        glm
    )
	
	set_target_properties(all_qmltyperegistrations PROPERTIES FOLDER ${AUTO_TARGET_NAME})
	set_target_properties(${TARGET_NAME}_automoc_json_extraction PROPERTIES FOLDER ${AUTO_TARGET_NAME})
	set_target_properties(${TARGET_NAME}_other_files PROPERTIES FOLDER ${AUTO_TARGET_NAME})
	set_target_properties(${TARGET_NAME}_qmltyperegistration PROPERTIES FOLDER ${AUTO_TARGET_NAME})
	set_target_properties(${TARGET_NAME}_tooling PROPERTIES FOLDER ${AUTO_TARGET_NAME})
	set_target_properties(fetch-translations PROPERTIES FOLDER ${AUTO_TARGET_NAME})

else()
    set(QML_IMPORT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/qml/qt5" CACHE STRING "Qt Creator 4.1 extra qml import paths" FORCE)

    target_sources(${TARGET_NAME} 
        PRIVATE 
        ${SOURCE_FILES}
        qml/qml.qrc
        ../data/images/images.qrc
        ../data/images/icon.rc
    )

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
endif()

target_include_directories(${TARGET_NAME} PRIVATE playlist)

target_compile_definitions(${TARGET_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

if(WIN32)
	set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(BUILD_CPLAY_WITH_NDI)
    list(APPEND TARGET_LIBRARIES NDI::NDI)
    target_compile_definitions(${TARGET_NAME} PUBLIC NDI_SUPPORT)
endif()

option(SGCT_FREETYPE_SUPPORT "Build SGCT with Freetype2" OFF)
option(SGCT_DEP_INCLUDE_FREETYPE "Include FreeType library" OFF)
option(SGCT_BUILD_TESTS "Build SGCT tests" OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sgct)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIBRARIES})

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)
target_compile_definitions(${TARGET_NAME} PUBLIC CPLAY_VERSION="${CPLAY_VERSION_MAJOR}.${CPLAY_VERSION_MINOR}.${CPLAY_VERSION_PATCH}")
target_compile_definitions(${TARGET_NAME} PUBLIC CPLAY_VERSION_MAJOR=${CPLAY_VERSION_MAJOR})
target_compile_definitions(${TARGET_NAME} PUBLIC CPLAY_VERSION_MINOR=${CPLAY_VERSION_MINOR})
target_compile_definitions(${TARGET_NAME} PUBLIC CPLAY_VERSION_PATCH=${CPLAY_VERSION_PATCH})

install(TARGETS ${TARGET_NAME} DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

if(CPLAY_INSTALL_DEPENDENCIES)
    if(BUILD_CPLAY_WITH_QT6)
        get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)
    else()
        get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
    endif()
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    get_filename_component(_qt_dir "${_qt_bin_dir}" DIRECTORY)
    get_filename_component(_mpv_bin_dir "${Libmpv_LIBRARIES}" DIRECTORY)

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

    option(CPLAY_INSTALL_GET_RUNTIME_DEPENDENCIES "When running INSTALL, look for dependencies with GET_RUNTIME_DEPENDENCIES and copy them" ON)
    if(CPLAY_INSTALL_GET_RUNTIME_DEPENDENCIES)
        set(MY_DEPENDENCY_PATHS ${_mpv_bin_dir} ${_qt_bin_dir})
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
				CONFLICTING_DEPENDENCIES_PREFIX CONFLICTING_DEPENDENCIES
                RESOLVED_DEPENDENCIES_VAR RESOLVED_DEPENDENCIES
                UNRESOLVED_DEPENDENCIES_VAR UNRESOLVED_DEPENDENCIES
                DIRECTORIES ${MY_DEPENDENCY_PATHS}
            )
            foreach(DEPENDENCY ${UNRESOLVED_DEPENDENCIES})
                message(WARNING "Unable to resolve dependency ${DEPENDENCY}")
            endforeach()
			foreach(DEPENDENCY ${CONFLICTING_DEPENDENCIES})
                message(WARNING "Conflicting dependency ${DEPENDENCY}")
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
        if(BUILD_CPLAY_WITH_NDI)
            install(CODE "set(_ndi_dll \"${NDI_BINARY_DIR}/${NDI_LIBS}.dll\")")
            install(CODE [[
                message("Copying ${_ndi_dll}")
                file(INSTALL
                    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                    FILES "${_ndi_dll}"
                )
            ]])
        endif()
    endif()
endif()