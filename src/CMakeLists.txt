#
# SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
#

qt5_add_dbus_interface(haruna_screensaver_inhibit_SRCS ../org.freedesktop.ScreenSaver.xml screensaverdbusinterface)

set(QML_IMPORT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/qml" CACHE STRING "Qt Creator 4.1 extra qml import paths" FORCE)

add_executable(C-Play)

target_sources(C-Play
    PRIVATE
    application.cpp
    haction.cpp
    lockmanager.cpp
    main.cpp
    mpris2/mediaplayer2.cpp
    mpris2/mediaplayer2player.cpp
    mpvobject.cpp
    qthelper.h
    playlist/playlistitem.cpp
    playlist/playlistmodel.cpp
    subtitlesfoldersmodel.cpp
    thumbnailimageprovider.cpp
    track.cpp
    tracksmodel.cpp
    worker.cpp
    qml.qrc
    ../help/help.qrc
    ${haruna_screensaver_inhibit_SRCS}
    ../data/images/images.qrc
    ../data/images/icon.rc
)

kconfig_add_kcfg_files(C-Play GENERATE_MOC settings/audiosettings.kcfgc)
kconfig_add_kcfg_files(C-Play GENERATE_MOC settings/generalsettings.kcfgc)
kconfig_add_kcfg_files(C-Play GENERATE_MOC settings/mousesettings.kcfgc)
kconfig_add_kcfg_files(C-Play GENERATE_MOC settings/playbacksettings.kcfgc)
kconfig_add_kcfg_files(C-Play GENERATE_MOC settings/playlistsettings.kcfgc)
kconfig_add_kcfg_files(C-Play GENERATE_MOC settings/subtitlessettings.kcfgc)
kconfig_add_kcfg_files(C-Play GENERATE_MOC settings/videosettings.kcfgc)

target_include_directories(C-Play PRIVATE playlist mpris2)

target_compile_definitions(C-Play PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

if(WIN32)
        set_target_properties(C-Play PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

target_link_libraries(C-Play PRIVATE
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
)

install(TARGETS C-Play DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
