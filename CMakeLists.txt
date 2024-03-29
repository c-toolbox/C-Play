cmake_minimum_required(VERSION 3.25 FATAL_ERROR)
cmake_policy(VERSION 3.25)
get_filename_component(C_PLAY_SOURCE_PARENT ${CMAKE_SOURCE_DIR} DIRECTORY)
set(CMAKE_INSTALL_PREFIX "${C_PLAY_SOURCE_PARENT}/install" CACHE PATH "")

project(c-play LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(ECM 1.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(${PROJECT_SOURCE_DIR}/src/sgct/support/cmake/set_compile_options.cmake)
include(KDEInstallDirs)
include(ECMInstallIcons)
include(FeatureSummary)

find_package(Qt5Core)
set_package_properties(Qt5Core PROPERTIES TYPE REQUIRED)

find_package(Qt5DBus)
set_package_properties(Qt5DBus PROPERTIES TYPE REQUIRED)

find_package(Qt5Qml)
set_package_properties(Qt5Qml PROPERTIES TYPE REQUIRED)

find_package(Qt5Quick)
set_package_properties(Qt5Quick PROPERTIES TYPE REQUIRED)

find_package(Qt5QuickControls2)
set_package_properties(Qt5QuickControls2 PROPERTIES TYPE REQUIRED)

find_package(Qt53DCore)
set_package_properties(Qt53DCore PROPERTIES TYPE REQUIRED)

find_package(Qt53DRender)
set_package_properties(Qt53DRender PROPERTIES TYPE REQUIRED)

find_package(Qt53DInput)
set_package_properties(Qt53DInput PROPERTIES TYPE REQUIRED)

find_package(Qt53DLogic)
set_package_properties(Qt53DLogic PROPERTIES TYPE REQUIRED)

find_package(Qt53DExtras)
set_package_properties(Qt53DExtras PROPERTIES TYPE REQUIRED)

find_package(Libmpv)
set_package_properties(Libmpv PROPERTIES TYPE REQUIRED
    URL "https://mpv.io")

find_package(Breeze)
option(InstallColorSchemes "" ON)
set_package_properties(Breeze PROPERTIES TYPE RECOMMENDED
    URL "https://api.kde.org/frameworks/breeze-icons/html/index.html")

find_package(KF5Config)
set_package_properties(KF5Config PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kconfig/html/index.html")

find_package(KF5CoreAddons)
set_package_properties(KF5CoreAddons PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kcoreaddons/html/index.html")

find_package(KF5FileMetaData)
set_package_properties(KF5FileMetaData PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kfilemetadata/html/index.html")

find_package(KF5I18n)
set_package_properties(KF5I18n PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/ki18n/html/index.html")

find_package(KF5IconThemes)
set_package_properties(KF5IconThemes PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kiconthemes/html/index.html")

find_package(KF5KIO)
set_package_properties(KF5KIO PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kio/html/index.html")

find_package(KF5Kirigami2)
set_package_properties(KF5Kirigami2 PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kirigami/html/index.html")

find_package(KF5XmlGui)
set_package_properties(KF5XmlGui PROPERTIES TYPE REQUIRED
    URL "https://api.kde.org/frameworks/kxmlgui/html/index.html")

option(InstallColorSchemes "Installs breeze color schemes." OFF)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

add_subdirectory(data)
add_subdirectory(src)
