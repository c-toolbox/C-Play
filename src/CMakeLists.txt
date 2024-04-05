#
# Copyright: 2021-2024 Erik Sunden <eriksunden85@gmail.com>
#
# SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

qt5_add_dbus_interface(haruna_screensaver_inhibit_SRCS ../data/org.freedesktop.ScreenSaver.xml screensaverdbusinterface)

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
    trackballcameracontroller.cpp
    trackballcameracontroller.h
    tracksmodel.cpp
    tracksmodel.h
    worker.cpp
    worker.h
    qml.qrc
    ../data/images/images.qrc
    ../data/images/icon.rc
)

kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/audiosettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/generalsettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/mousesettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/playbacksettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/playlistsettings.kcfgc)
kconfig_add_kcfg_files(${TARGET_NAME} GENERATE_MOC settings/videosettings.kcfgc)

target_include_directories(${TARGET_NAME} PRIVATE playlist)

target_compile_definitions(${TARGET_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

if(WIN32)
        set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()


option(SGCT_FREETYPE_SUPPORT "Build SGCT with Freetype2" OFF)
option(SGCT_DEP_INCLUDE_FREETYPE "Include FreeType library" OFF)
option(SGCT_BUILD_TESTS "Build SGCT tests" OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sgct)

target_link_libraries(${TARGET_NAME} PRIVATE
    Qt5::Core
    Qt5::DBus
    Qt5::Qml
    Qt5::Quick
    Qt5::QuickControls2
    Qt5::3DCore
    Qt5::3DRender
    Qt5::3DInput
    Qt5::3DLogic
    Qt5::3DExtras

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

target_compile_features(${TARGET_NAME} PRIVATE cxx_std_17)

install(TARGETS ${TARGET_NAME} DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
