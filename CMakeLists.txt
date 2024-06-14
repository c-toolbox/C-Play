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
set(AUTO_TARGET_NAME "Generated Targets")
set(AUTO_SOURCE_NAME "Generated Files")
set_property(GLOBAL PROPERTY AUTOMOC_TARGETS_FOLDER ${AUTO_TARGET_NAME})
set_property(GLOBAL PROPERTY AUTORCC_TARGETS_FOLDER ${AUTO_TARGET_NAME})
set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER ${AUTO_TARGET_NAME})
set_property(GLOBAL PROPERTY AUTOGEN_SOURCE_GROUP ${AUTO_SOURCE_NAME})
set_property(GLOBAL PROPERTY AUTOMOC_SOURCE_GROUP ${AUTO_SOURCE_NAME})

option(BUILD_CPLAY_WITH_QT6 "Global On/Off for Qt6 vs Qt5" ON)

if(BUILD_CPLAY_WITH_QT6)
    set(QT_MIN_VERSION 6.6.0)
    set(KF6_MIN_VERSION 6.0.0)

    include(FeatureSummary)

    find_package(ECM ${KF6_MIN_VERSION} NO_MODULE)
    set_package_properties(ECM PROPERTIES TYPE REQUIRED
        URL "https://api.kde.org/ecm/"
        DESCRIPTION "extra cmake modules")
    set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

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
else()
	if(BUILD_WITH_QT6)
		option(BUILD_WITH_QT6 "Build against Qt 6" OFF)
	 endif()

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

    find_package(Breeze)
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
endif()

find_package(Libmpv)
set_package_properties(Libmpv PROPERTIES TYPE REQUIRED
    URL "https://mpv.io")

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

option(CPLAY_INSTALL_DEPENDENCIES "Global On/Off for all CPLAY_INSTALL_* dependencies" OFF)

add_subdirectory(data)
add_subdirectory(help)
add_subdirectory(src)
add_subdirectory(help/jack)
