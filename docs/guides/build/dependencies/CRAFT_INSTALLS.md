# Craft Guide

[Craft on Windows](https://community.kde.org/Guidelines_and_HOWTOs/Build_from_source/Windows)

Use Visual Studio as compiler. (However MSYS / MinGW will be installed later anyway)

NOTE: C-Play has currently been tested with Qt 5.15.5. To reproduce this build, add the following lines to the etc/BlueprintSettings.ini in your craft folder before "crafting": 

```
[libs/qt5]
version = 5.15.5
```

### Packages to install (via Craft)

Run command
```
craft libs/qt5/qtbase libs/qt5/qtdeclarative libs/qt5/qtquickcontrols2 libs/qt5/qt3d libs/qt5/qttools extra-cmake-modules kconfig kcoreaddons kfilemetadata ki18n kiconthemes kio kirigami kio-extras breeze breeze-icons qqc2-breeze-style qqc2-desktop-style
```
These are the list of packages needed for this project
- Qt5Core (within Base)
- Qt5DBus (within Base)
- Qt5Qml (within Declarative)
- Qt5Quick (within Declarative)
- Qt5QuickControls2 (qtquickcontrols2)
- Qt3D
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

NOTE: Due to Qt3DInput and QtWidgets both having QAction, some includes in KDE do not handle this, and might need changes in header file, from to <QtWidgets/qaction.h>

The add Craft bin and the dev-utils\bin folder to your environmental "Path".
