# Craft Guide

[Craft on Windows](https://community.kde.org/Guidelines_and_HOWTOs/Build_from_source/Windows)

Use Visual Studio 2019 as compiler. (However MSYS / MinGW will be installed later anyway, if you compile/install FFmpeg + MPV + JACK)

*Note, this is needed for Qt5. You can use Visual Studio 2022 for C-Play even if these dependencies require VS2019. C-Play has currently been tested with Qt 5.15 LTS. It should be the latest QT5 in the Craft source currently (2024-05-21).*

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
