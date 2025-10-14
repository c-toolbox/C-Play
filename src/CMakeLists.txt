#
# Copyright: 2021-2025 Erik Sunden <eriksunden85@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later
#

set(TARGET_NAME C-Play)
set(CPLAY_VERSION_MAJOR 2)
set(CPLAY_VERSION_MINOR 2)
set(CPLAY_VERSION_PATCH 0)
set(CPLAY_VERSION_PR_NAME "Beta")
set(CPLAY_VERSION_PR_VERS 8)

qt_add_executable(${TARGET_NAME})
set_compile_options(${TARGET_NAME})

if (MSVC)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
endif()

set(SOURCE_FILES
    application.cpp
    application.h
    haction.cpp
    haction.h
    httpserverthread.cpp
    httpserverthread.h
    layers/audiolayer.cpp
    layers/audiolayer.h
    layers/baselayer.cpp
    layers/baselayer.h
    layers/imagelayer.cpp
    layers/imagelayer.h
    layers/mpvlayer.cpp
    layers/mpvlayer.h
    layers/streamlayer.cpp
    layers/streamlayer.h
    layers/streammodel.cpp
    layers/streammodel.h
    layers/textlayer.cpp
    layers/textlayer.h
    layers/videolayer.cpp
    layers/videolayer.h
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
    screensmodel.cpp
    screensmodel.h
    slidesmodel.cpp
    slidesmodel.h
    slidesqtitem.cpp
    slidesqtitem.h
    track.cpp
    track.h
    tracksmodel.cpp
    tracksmodel.h
    worker.cpp
    worker.h
    utils/domegrid.cpp
    utils/domegrid.h
    utils/planegrid.cpp
    utils/planegrid.h
    utils/spheregrid.cpp
    utils/spheregrid.h
)

if(BUILD_CPLAY_WITH_NDI)
    list(APPEND SOURCE_FILES ndi/ndilayer.cpp)
    list(APPEND SOURCE_FILES ndi/ndilayer.h)
    list(APPEND SOURCE_FILES ndi/ndimodel.cpp)
    list(APPEND SOURCE_FILES ndi/ndimodel.h)
    set(OFX_NDI_FILES
        ndi/ofxNDI/ofxNDIdynloader.cpp
        ndi/ofxNDI/ofxNDIdynloader.h
        ndi/ofxNDI/ofxNDIplatforms.h
        ndi/ofxNDI/ofxNDIreceive.cpp
        ndi/ofxNDI/ofxNDIreceive.h
        ndi/ofxNDI/ofxNDIutils.cpp
        ndi/ofxNDI/ofxNDIutils.h
    )
    list(APPEND SOURCE_FILES ${OFX_NDI_FILES})
    target_include_directories(${TARGET_NAME} PRIVATE ndi)
endif()

if(BUILD_CPLAY_WITH_SPOUT)
    list(APPEND SOURCE_FILES layers/spoutlayer.cpp)
    list(APPEND SOURCE_FILES layers/spoutlayer.h)
    list(APPEND SOURCE_FILES layers/spoutmodel.cpp)
    list(APPEND SOURCE_FILES layers/spoutmodel.h)
endif()

if(BUILD_CPLAY_WITH_MDK)
    list(APPEND SOURCE_FILES layers/adaptivevideolayer.cpp)
    list(APPEND SOURCE_FILES layers/adaptivevideolayer.h)
    list(APPEND SOURCE_FILES layers/mdklayer.cpp)
    list(APPEND SOURCE_FILES layers/mdklayer.h)
endif()

if(BUILD_CPLAY_WITH_POPPLER)
    list(APPEND SOURCE_FILES layers/pdflayer.cpp)
    list(APPEND SOURCE_FILES layers/pdflayer.h)
endif()

target_sources(${TARGET_NAME} 
    PRIVATE 
    ${SOURCE_FILES}
    ../data/images/images.qrc
    ../data/images/icon.rc
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Cpp" FILES ${SOURCE_FILES}) 

if(BUILD_WITH_VCPKG_SUPPORT)
    option(SGCT_DEP_INCLUDE_LIBPNG "Include LibPNG library" OFF)
    option(SGCT_DEP_INCLUDE_ZLIB "Include ZLIB library" OFF)
    option(SGCT_DEP_INCLUDE_TINYXML "Include TinyXML library" OFF)
else()
    option(SGCT_DEP_INCLUDE_LIBPNG "Include LibPNG library" ON)
    option(SGCT_DEP_INCLUDE_ZLIB "Include ZLIB library" ON)
    option(SGCT_DEP_INCLUDE_ZLIB "Include ZLIB library" ON)
endif()

find_package(QT NAMES Qt6)
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6 AND ${QT_VERSION_MINOR} GREATER_EQUAL 8)
    qt_standard_project_setup(REQUIRES 6.8)
else()
    qt_standard_project_setup(REQUIRES 6.5)
endif()
qt_policy(SET QTP0001 NEW)

set(QML_SRC_FILES
    qml/qt6/Components/ImageAdjustmentSlider.qml
    qml/qt6/Components/LabelWithTooltip.qml
    qml/qt6/Components/LayerCoreProperties.qml
    qml/qt6/Components/SelectActionPopup.qml
    qml/qt6/Components/SettingsBasePage.qml
    qml/qt6/Components/SettingsHeader.qml
    qml/qt6/Menus/AudioMenu.qml
    qml/qt6/Menus/FileMenu.qml
    qml/qt6/Menus/HelpMenu.qml
    qml/qt6/Menus/PlaybackMenu.qml
    qml/qt6/Menus/SettingsMenu.qml
    qml/qt6/Menus/SubtitleMenu.qml
    qml/qt6/Settings/AudioSettings.qml
    qml/qt6/Settings/GridSettings.qml
    qml/qt6/Settings/ImageSettings.qml
    qml/qt6/Settings/LocationSettings.qml
    qml/qt6/Settings/MouseSettings.qml
    qml/qt6/Settings/Navigation.qml
    qml/qt6/Settings/PlaybackSettings.qml
    qml/qt6/Settings/PlaylistSettings.qml
    qml/qt6/Settings/PresentationSettings.qml
    qml/qt6/Settings/SubtitleSettings.qml
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
    qml/qt6/LayersAddNew.qml
    qml/qt6/LayersItemCompact.qml
    qml/qt6/LayerView.qml
    qml/qt6/LayerViewGridParams.qml
    qml/qt6/Slides.qml
    qml/qt6/SlidesItemCompact.qml
    qml/qt6/SlidesVisibilityView.qml
    qml/qt6/BackgroundImage.qml
    qml/qt6/ForegroundImage.qml
    qml/qt6/SaveAsCPlayFile.qml
    qml/qt6/ViewPlaylistItem.qml
    qml/qt6/WorkingIndicator.qml
)

option(CPLAY_FAST_QML_BUILD "Skip QmlLint and QmlCacheGen. Should be OFF for release build." OFF)   
if(CPLAY_FAST_QML_BUILD)
    foreach(QML_FILE ${QML_SRC_FILES})
        set_source_files_properties(${QML_FILE} PROPERTIES
            QT_QML_SKIP_QMLLINT TRUE
            QT_QML_SKIP_CACHEGEN TRUE
        )
    endforeach()
endif()

qt_add_qml_module(${TARGET_NAME}
    URI org.ctoolbox.cplay
    QML_FILES
    ${QML_SRC_FILES}
)
qt_import_qml_plugins(${TARGET_NAME})
ecm_find_qmlmodule(org.kde.kirigami REQUIRED)

source_group(Qml FILES ${QML_SRC_FILES})

set(KCFCG_FILE_NAMES
    audiosettings
    gridsettings
    imagesettings
    locationsettings
    mousesettings
    playbacksettings
    playlistsettings
    presentationsettings
    subtitlesettings
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
source_group(${AUTO_SOURCE_NAME} FILES ${KCFCG_MOC_FILES})

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

if (MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE
    /wd26439
    /wd26444
    /wd26495
    /wd26813 
    /wd4702 # unreachable code
    /experimental:external /external:anglebrackets /external:templates- /external:W0)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6 AND ${QT_VERSION_MINOR} GREATER_EQUAL 8)
    #set_target_properties(all_aotstats PROPERTIES FOLDER ${AUTO_TARGET_NAME})
    set_target_properties(${TARGET_NAME}_copy_qml PROPERTIES FOLDER ${AUTO_TARGET_NAME})
    set_target_properties(${TARGET_NAME}_copy_res PROPERTIES FOLDER ${AUTO_TARGET_NAME})
endif()
    set_target_properties(all_qmltyperegistrations PROPERTIES FOLDER ${AUTO_TARGET_NAME})
    set_target_properties(${TARGET_NAME}_automoc_json_extraction PROPERTIES FOLDER ${AUTO_TARGET_NAME})
    #set_target_properties(${TARGET_NAME}_other_files PROPERTIES FOLDER ${AUTO_TARGET_NAME})
    set_target_properties(${TARGET_NAME}_qmltyperegistration PROPERTIES FOLDER ${AUTO_TARGET_NAME})
    set_target_properties(${TARGET_NAME}_tooling PROPERTIES FOLDER ${AUTO_TARGET_NAME})
    set_target_properties(fetch-translations PROPERTIES FOLDER "External")
endif()

target_include_directories(${TARGET_NAME} PRIVATE layers)
target_include_directories(${TARGET_NAME} PRIVATE playlist)
target_compile_definitions(${TARGET_NAME} PRIVATE $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:QT_QML_DEBUG>)

if(WIN32)
    set_target_properties(${TARGET_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(BUILD_CPLAY_WITH_JACK)
    list(APPEND TARGET_LIBRARIES Jack2::Jack)
    target_compile_definitions(${TARGET_NAME} PUBLIC JACK_SUPPORT)
endif()

if(BUILD_CPLAY_WITH_MDK)
    list(APPEND TARGET_LIBRARIES mdk)
    target_compile_definitions(${TARGET_NAME} PUBLIC MDK_SUPPORT)
endif()

if(BUILD_CPLAY_WITH_NDI)
    list(APPEND TARGET_LIBRARIES NDI::NDI)
    target_compile_definitions(${TARGET_NAME} PUBLIC NDI_SUPPORT)

    list(APPEND TARGET_LIBRARIES PortAudio::PortAudio)
endif()

if(BUILD_CPLAY_WITH_SPOUT)
    list(APPEND TARGET_LIBRARIES Spout2::SpoutLibrary)
    target_compile_definitions(${TARGET_NAME} PUBLIC SPOUT_SUPPORT)

    get_target_property(SPOUT_INCLUDE_DIRS Spout2::SpoutLibrary INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(${TARGET_NAME} PRIVATE ${SPOUT_INCLUDE_DIRS})
endif()

if(BUILD_CPLAY_WITH_POPPLER)
    list(APPEND TARGET_LIBRARIES Poppler::Cpp)
    target_compile_definitions(${TARGET_NAME} PUBLIC PDF_SUPPORT)

    get_target_property(POPPLER_INCLUDE_DIRS Poppler::Cpp INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(${TARGET_NAME} PRIVATE ${POPPLER_INCLUDE_DIRS})
endif()

option(SGCT_FREETYPE_SUPPORT "Build SGCT with Freetype2" ON)
option(SGCT_DEP_INCLUDE_FREETYPE "Include FreeType library" OFF)
option(SGCT_BUILD_TESTS "Build SGCT tests" OFF)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sgct)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_LIBRARIES})
target_compile_features(${TARGET_NAME} PRIVATE cxx_std_23)
if(DEFINED CPLAY_VERSION_PR_NAME AND DEFINED CPLAY_VERSION_PR_VERS)
  target_compile_definitions(${TARGET_NAME} PUBLIC CPLAY_VERSION="${CPLAY_VERSION_MAJOR}.${CPLAY_VERSION_MINOR}.${CPLAY_VERSION_PATCH} ${CPLAY_VERSION_PR_NAME} ${CPLAY_VERSION_PR_VERS}")
  set(CPLAY_PACKNAME "${TARGET_NAME}_${CPLAY_VERSION_MAJOR}_${CPLAY_VERSION_MINOR}_${CPLAY_VERSION_PATCH}_${CPLAY_VERSION_PR_NAME}_${CPLAY_VERSION_PR_VERS}")
else()
  target_compile_definitions(${TARGET_NAME} PUBLIC CPLAY_VERSION="${CPLAY_VERSION_MAJOR}.${CPLAY_VERSION_MINOR}.${CPLAY_VERSION_PATCH}")
  set(CPLAY_PACKNAME "${TARGET_NAME}_${CPLAY_VERSION_MAJOR}_${CPLAY_VERSION_MINOR}_${CPLAY_VERSION_PATCH}")
endif()

configure_file(../help/CPlay_Pack_Installer.nsi.in ../help/CPlay_Pack_Installer.nsi @ONLY)
configure_file(../help/CPlay_Pack_Zip.bat.in ../help/CPlay_Pack_Zip.bat @ONLY)

target_compile_definitions(${TARGET_NAME} PUBLIC CPLAY_VERSION_MAJOR=${CPLAY_VERSION_MAJOR})
target_compile_definitions(${TARGET_NAME} PUBLIC CPLAY_VERSION_MINOR=${CPLAY_VERSION_MINOR})
target_compile_definitions(${TARGET_NAME} PUBLIC CPLAY_VERSION_PATCH=${CPLAY_VERSION_PATCH})

install(TARGETS ${TARGET_NAME} DESTINATION ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

if(CPLAY_INSTALL_DEPENDENCIES)
    get_target_property(_qmake_executable Qt6::qmake IMPORTED_LOCATION)

    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    get_filename_component(_qt_dir "${_qt_bin_dir}" DIRECTORY)
    get_filename_component(_mpv_bin_dir "${Libmpv_LIBRARIES}" DIRECTORY)

    install(CODE "set(QT_BIN_DIR \"${_qt_bin_dir}\")")

    option(CPLAY_INSTALL_PLUGINS_FROM_CRAFT "Copy complete plugins folder for Qt/KDE" ON)   
    if(CPLAY_INSTALL_PLUGINS_FROM_CRAFT)
        install(CODE [[
            message("Copying Qt/KDE plugins folder")
        ]])
        install(DIRECTORY "${_qt_dir}/plugins" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" FILES_MATCHING PATTERN "*.dll")
    endif()

    option(CPLAY_INSTALL_QML_FROM_CRAFT "Copy complete qml folder for Qt/KDE" ON)
    if(CPLAY_INSTALL_QML_FROM_CRAFT)
        install(CODE [[
            message("Copying KDE qml folder")
        ]])
        #
        install(DIRECTORY "${_qt_dir}/qml" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
            FILES_MATCHING PATTERN "*.dll")
        install(DIRECTORY "${_qt_dir}/qml" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
            FILES_MATCHING PATTERN "*.qmltypes")
        install(DIRECTORY "${_qt_dir}/qml" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
            FILES_MATCHING PATTERN "qmldir")
        install(DIRECTORY "${_qt_dir}/qml/org" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin/qml")
    endif()

    # Check if we are on Windows 11 (Build number 22000 or higher) or Windows 10 (Build number 10240 or higher)
    cmake_host_system_information(RESULT OS_VERSION QUERY OS_VERSION)
    string(REGEX MATCH "[0-9]+" OS_BUILD_NUM "${OS_VERSION}")

    if(${OS_BUILD_NUM} VERSION_GREATER_EQUAL 22000)
        message("Windows 11 or above detected, recommended to copy all DLLS from path instead of using GET_RUNTIME_DEPENDENCIES")
        set(INSTALL_GRP FALSE)
        set(INSTALL_DFP TRUE)
    else()
        message("Windows 10 or below detected, recommended to use GET_RUNTIME_DEPENDENCIES instead of copying all DLLS from path")
        set(INSTALL_GRP TRUE)
        set(INSTALL_DFP FALSE)
    endif()

    option(CPLAY_INSTALL_GET_RUNTIME_DEPENDENCIES "When running INSTALL, look for dependencies with GET_RUNTIME_DEPENDENCIES and copy them" ${INSTALL_GRP})
    if(CPLAY_INSTALL_GET_RUNTIME_DEPENDENCIES)
        if(SGCT_DEP_INCLUDE_SCALABLE)
            set(_scalable_dir ${CMAKE_CURRENT_SOURCE_DIR}/sgct/ext/scalable)
        endif()

        set(MY_DEPENDENCY_PATHS ${_mpv_bin_dir} ${_qt_bin_dir} ${_scalable_dir})
        # Transfer the values into the install script
        install(CODE "set(MY_DEPENDENCY_PATHS \"${MY_DEPENDENCY_PATHS}\")")
        install(CODE "set(target \"$<TARGET_FILE:${TARGET_NAME}>\")")
        if(BUILD_CPLAY_WITH_MDK)
            install(CODE "set(_mdk_dll \"${MDK_RUNTIME}\")")
            install(CODE "set(_mdk_dlls \"${MDK_RUNTIMES}\")")
        endif()
        if(BUILD_CPLAY_WITH_NDI)
            install(CODE "set(_ndi_dll \"${NDI_BINARY_DIR}/${NDI_LIBS}.dll\")")
        endif()
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
            foreach(DEPENDENCY ${CONFLICTING_DEPENDENCIES})
                message(WARNING "Conflicting dependency ${DEPENDENCY}")
            endforeach()
            message("Copying detected dependencies")
            foreach(DEPENDENCY ${RESOLVED_DEPENDENCIES})
                message("Copying ${DEPENDENCY}")
                file(INSTALL
                    DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                    FILES "${DEPENDENCY}"
                )
            endforeach()
            message("Trying to fix unresolved dependencies")
            foreach(DEPENDENCY ${UNRESOLVED_DEPENDENCIES})
                if(DEFINED _ndi_dll AND "${_ndi_dll}" MATCHES "${DEPENDENCY}$")
                    message("Copying ${_ndi_dll}")
                    file(INSTALL
                        DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                        FILES "${_ndi_dll}"
                    )
                elseif(DEFINED _mdk_dll AND "${_mdk_dll}" MATCHES "${DEPENDENCY}$")
                    foreach(_mdk_dep_dll ${_mdk_dlls})
                        message("Copying ${_mdk_dep_dll}")
                        file(INSTALL
                            DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                            FILES "${_mdk_dep_dll}"
                        )
                    endforeach()
                else()
                    message(WARNING "Unable to resolve dependency ${DEPENDENCY}")
                endif()
            endforeach()
        ]])
    endif()

    option(CPLAY_INSTALL_DLLS_FROM_PATHS "When running INSTALL, copy all DLLS from CPLAY_INSTALL_DLLS_FROM_PATH_LIST to the INSTALL/bin folder" ${INSTALL_DFP})
    if(CPLAY_INSTALL_DLLS_FROM_PATHS)
        set(CIDFPL ${_qt_bin_dir} ${_mpv_bin_dir})

        if(BUILD_WITH_VCPKG_SUPPORT)
            list(APPEND CIDFPL "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
        endif()
        if(BUILD_CPLAY_WITH_MDK)
            list(APPEND CIDFPL ${MDK_SDK_ROOT})
        endif()
        if(BUILD_CPLAY_WITH_NDI)
            list(APPEND CIDFPL ${NDI_BINARY_DIR})
        endif()
        if(SGCT_DEP_INCLUDE_SCALABLE)
            list(APPEND CIDFPL ${CMAKE_CURRENT_SOURCE_DIR}/sgct/ext/scalable)
        endif()

        set(CPLAY_INSTALL_DLLS_FROM_PATH_LIST ${CIDFPL} CACHE STRING "The paths where to look for DLL:s")

        # Install dependencies
        install(CODE "set(_all_dll_copy_paths \"${CPLAY_INSTALL_DLLS_FROM_PATH_LIST}\")")
        install(CODE [[
                message("Copying all DLL:s from CPLAY_INSTALL_DLLS_FROM_PATH_LIST to install folder")
                foreach(DLL_DIR ${_all_dll_copy_paths})
                    message("Looking for DLL:s in ${DLL_DIR}")
                    if(EXISTS "${DLL_DIR}")
                        file(GLOB DLL_FILES "${DLL_DIR}/*.dll")
                        foreach(DLL_FILE ${DLL_FILES})
                            message("Copying ${DLL_FILE}")
                            file(INSTALL
                                DESTINATION "${CMAKE_INSTALL_PREFIX}/bin"
                                FILES "${DLL_FILE}"
                            )
                        endforeach()
                    else()
                        message("Directory ${DLL_DIR} does not exist")
                    endif()
                endforeach()
            ]])
    endif()


endif()