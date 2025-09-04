cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
cmake_policy(VERSION 3.25)
get_filename_component(C_PLAY_SOURCE_PARENT ${CMAKE_SOURCE_DIR} DIRECTORY)
set(CMAKE_INSTALL_PREFIX "${C_PLAY_SOURCE_PARENT}/install" CACHE PATH "")

option(BUILD_WITH_VCPKG_SUPPORT "Global On/Off for Vcpkg usage" ON)
if(BUILD_WITH_VCPKG_SUPPORT)
    file(TO_CMAKE_PATH $ENV{VCPKG_ROOT} VCPKG_ROOT)
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
    set(VCPKG_TARGET_ARCHITECTURE x64)
    set(VCPKG_LIBRARY_LINKAGE dynamic)
    set(VCPKG_CRT_LINKAGE dynamic)
    include(${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)
    set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL MinSizeRel RelWithDebInfo Release Debug)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release MinSizeRel Debug)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE Release RelWithDebInfo MinSizeRel Debug)
endif()

project(c-play LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set(AUTO_TARGET_NAME "Generated Targets")
set(AUTO_SOURCE_NAME "Generated Files")
set_property(GLOBAL PROPERTY AUTOMOC_TARGETS_FOLDER ${AUTO_TARGET_NAME})
set_property(GLOBAL PROPERTY AUTORCC_TARGETS_FOLDER ${AUTO_TARGET_NAME})
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER ${AUTO_TARGET_NAME})
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP ${AUTO_SOURCE_NAME})
set_property(GLOBAL PROPERTY AUTOMOC_SOURCE_GROUP ${AUTO_SOURCE_NAME})

set(QT_MIN_VERSION 6.6.0)
set(KF6_MIN_VERSION 6.0.0)

include(FeatureSummary)

find_package(ECM ${KF6_MIN_VERSION} NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/ecm/"
    DESCRIPTION "extra cmake modules")
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (MSVC)
include(${PROJECT_SOURCE_DIR}/src/sgct/support/cmake/common-compile-settings/platforms/msvc.cmake)
endif()

include(${PROJECT_SOURCE_DIR}/src/sgct/support/cmake/common-compile-settings/common-compile-settings.cmake)
include(${PROJECT_SOURCE_DIR}/src/sgct/support/cmake/set_compile_options.cmake)

find_package(Qt6Core ${QT_MIN_VERSION})
set_package_properties(Qt6Core PROPERTIES TYPE REQUIRED)

find_package(Qt6Gui ${QT_MIN_VERSION})
set_package_properties(Qt6Gui PROPERTIES TYPE REQUIRED)

find_package(Qt6Qml ${QT_MIN_VERSION})
set_package_properties(Qt6Qml PROPERTIES TYPE REQUIRED)

find_package(Qt6Quick ${QT_MIN_VERSION})
set_package_properties(Qt6Quick PROPERTIES TYPE REQUIRED)

find_package(Qt6QuickControls2 ${QT_MIN_VERSION})
set_package_properties(Qt6QuickControls2 PROPERTIES TYPE REQUIRED)

find_package(Breeze)
set_package_properties(Breeze PROPERTIES TYPE RECOMMENDED
    URL "https://api.kde.org/frameworks/breeze-icons/html/index.html")

find_package(KF6ColorScheme ${KF6_MIN_VERSION})
set_package_properties(KF6ColorScheme PROPERTIES TYPE REQUIRED
    URL "https://invent.kde.org/frameworks/kcolorscheme")

find_package(KF6Config ${KF6_MIN_VERSION})
set_package_properties(KF6Config PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kconfig/html/index.html")

find_package(KF6CoreAddons ${KF6_MIN_VERSION})
set_package_properties(KF6CoreAddons PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kcoreaddons/html/index.html")

find_package(KF6DocTools ${KF6_MIN_VERSION})
set_package_properties(KF6DocTools PROPERTIES
    DESCRIPTION "https://api.kde.org/frameworks/kdoctools/html/index.html")

find_package(KF6FileMetaData ${KF6_MIN_VERSION})
set_package_properties(KF6FileMetaData PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kfilemetadata/html/index.html")

find_package(KF6I18n ${KF6_MIN_VERSION})
set_package_properties(KF6I18n PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/ki18n/html/index.html")

find_package(KF6IconThemes ${KF6_MIN_VERSION})
set_package_properties(KF6IconThemes PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kiconthemes/html/index.html")

find_package(KF6KIO ${KF6_MIN_VERSION})
set_package_properties(KF6KIO PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kio/html/index.html")

find_package(KF6Kirigami ${KF6_MIN_VERSION})
set_package_properties(KF6Kirigami PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kirigami/html/index.html")

find_package(KF6WindowSystem ${KF6_MIN_VERSION})
set_package_properties(KF6WindowSystem PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kwindowsystem/html/index.html")
    
find_package(KF6XmlGui ${KF6_MIN_VERSION})
set_package_properties(KF6XmlGui PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kxmlgui/html/index.html")

include(KDEInstallDirs)
include(ECMInstallIcons)
include(ECMAddAppIcon)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Libmpv)
set_package_properties(Libmpv PROPERTIES TYPE REQUIRED
    URL "https://mpv.io")

option(BUILD_CPLAY_WITH_JACK "Global On/Off for JACK support" ON)
if(BUILD_CPLAY_WITH_JACK)
    find_package(Jack2 REQUIRED)

    if(NOT JACK2_FOUND)
        message("Remember to install Jack2 from jackaudio.org")
    endif()
    set_package_properties(Jack2 PROPERTIES TYPE REQUIRED
        URL "https://jackaudio.org/" DESCRIPTION "Jack2 Audio Connection Kit")
endif()

option(BUILD_CPLAY_WITH_MDK "Global On/Off for MDK support" OFF)
if(BUILD_CPLAY_WITH_MDK)
    find_path(MDK_CMAKE_MODULE NAMES FindMDK.cmake PATHS $ENV{MDK_SDK_ROOT}/lib/cmake)
    list(APPEND CMAKE_MODULE_PATH "${MDK_CMAKE_MODULE}")

    find_package(mdk REQUIRED)
    set_package_properties(mdk PROPERTIES TYPE REQUIRED
        URL "https://github.com/wang-bin/mdk-sdk/" DESCRIPTION "multimedia development kit")

    set_target_properties(mdk PROPERTIES
        IMPORTED_LOCATION "${MDK_RUNTIME}"
        IMPORTED_LOCATION_RELEASE "${MDK_RUNTIME}"
        IMPORTED_IMPLIB "${MDK_LIBRARY}"
        IMPORTED_IMPLIB_RELEASE "${MDK_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${MDK_INCLUDE_DIRS}"
    )
endif()

option(BUILD_CPLAY_WITH_NDI "Global On/Off for NDI support" OFF)
if(BUILD_CPLAY_WITH_NDI)
    find_package(NDI)
    set_package_properties(NDI PROPERTIES TYPE REQUIRED
        URL "https://ndi.video/for-developers/ndi-sdk/download/" DESCRIPTION "video and audio over network")

    find_package(PortAudio REQUIRED)
    set_package_properties(PortAudio PROPERTIES TYPE REQUIRED 
        URL "https://www.portaudio.com/" DESCRIPTION "for NDI audio output support.")
endif()

option(BUILD_CPLAY_WITH_SPOUT "Global On/Off for SPOUT support" OFF)
if(BUILD_CPLAY_WITH_SPOUT)
    find_package(Spout2)
    # Install portaudio with vcpkg
    if(NOT Spout2_FOUND)
        message("Remember to run: vcpkg install spout2")
    endif()
    set_package_properties(Spout2 PROPERTIES TYPE REQUIRED
        URL "https://spout.zeal.co/" DESCRIPTION "video sharing on windows")
endif()

option(BUILD_CPLAY_WITH_POPPLER "Global On/Off for PDF support via Poppler library" OFF)
if(BUILD_CPLAY_WITH_POPPLER)
    find_package(unofficial-poppler REQUIRED)
    if(NOT unofficial-poppler_FOUND)
        message("Remember to run: vcpkg install poppler")
    endif()
    set_package_properties(unofficial-poppler PROPERTIES TYPE REQUIRED 
        URL "https://poppler.freedesktop.org/" DESCRIPTION "Support for PDF page rendering.")
endif()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

option(CPLAY_INSTALL_DEPENDENCIES "Global On/Off for all CPLAY_INSTALL_* dependencies" OFF)

add_subdirectory(data)
add_subdirectory(help)
add_subdirectory(src)
