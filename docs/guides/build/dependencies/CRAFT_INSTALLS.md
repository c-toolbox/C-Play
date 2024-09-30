# Qt and KDE Frameworks for C-Play Master UI

C-Play can be built with either Qt6/KF6 or Qt5/KF5, but for best support for DPI scaling Qt6/KF6 is default and recommended.
*NOTE: C-Play Version 2.1 and newer will only support Qt6.*

The CMake option *BUILD_CPLAY_WITH_QT6* is enabled by default. Disable it to detect Qt5 installation/dependencies instead.
KDE Frameworks have the same versioning, as the depend on Qt themselves.

Read [Craft on Windows](https://community.kde.org/Get_Involved/development/Windows) and follow the guide with instruction below.

## Option 1 (default): Build Qt6/KF6 for C-Play Master UI

Use Visual Studio 2022 as compiler. (However MSYS / MinGW will be installed later anyway, if you compile/install FFmpeg + MPV + JACK)

### Packages to install (via Craft)

Run command
```
craft libs/qt6 extra-cmake-modules kconfig kcoreaddons kfilemetadata ki18n kiconthemes kio kirigami kio-extras breeze breeze-icons qqc2-breeze-style qqc2-desktop-style
```
These are the list of packages needed for this project
- Qt6Core (within Base)
- Qt6DBus (within Base)
- Qt6Qml (within Declarative)
- Qt6Quick (within Declarative)
- Qt6QuickControls2 (qtquickcontrols2)
- ExtraCmakeModules
- KF6Config (kconfig)
- KF6CoreAddons (kcoreaddons)
- KF6FileMetaData (kfilemetadata)
- KF6I18n (ki18n)
- KF6IconThemes (kiconthemes)
- KF6KIO (kio)
- KF6Kirigami2 (kirigami)
- KF6XmlGui (kxmlgui)
- Kio-extras (kio-extras)
- Breeze
- Breeze icons (breeze-icons)
- Breeze widgets style (qqc2-breeze-style)
- QQC2-Desktop-Style

## Option 2 (older, not available from C-Play version 2.1 and newer): Build Qt5/KF5 for C-Play Master UI

Use Visual Studio 2019 as compiler. (However MSYS / MinGW will be installed later anyway, if you compile/install FFmpeg + MPV + JACK)

*Note, Visual Studio 2019 is needed for Qt5 with Craft. You can use Visual Studio 2022 for C-Play even if these dependencies require VS2019. C-Play has currently been tested with Qt 5.15 LTS. It should be the latest QT5 in the Craft source currently (2024-05-21).*

### Packages to install (via Craft)

Run command
```
craft libs/qt5/qtbase libs/qt5/qtdeclarative libs/qt5/qtquickcontrols2 libs/qt5/qttools extra-cmake-modules kconfig kcoreaddons kfilemetadata ki18n kiconthemes kio kirigami kio-extras breeze breeze-icons qqc2-breeze-style qqc2-desktop-style
```
These are the list of packages needed for this project
- Qt5Core (within Base)
- Qt5DBus (within Base)
- Qt5Qml (within Declarative)
- Qt5Quick (within Declarative)
- Qt5QuickControls2 (qtquickcontrols2)
- ExtraCmakeModules
- KF5Config (kconfig)
- KF5CoreAddons (kcoreaddons)
- KF5FileMetaData (kfilemetadata)
- KF5I18n (ki18n)
- KF5IconThemes (kiconthemes)
- KF5KIO (kio)
- KF5Kirigami2 (kirigami)
- KF5XmlGui (kxmlgui)
- Kio-extras (kio-extras)
- Breeze
- Breeze icons (breeze-icons)
- Breeze widgets style (qqc2-breeze-style)
- QQC2-Desktop-Style