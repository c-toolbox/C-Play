# Craft Guide for Qt6/KF6

[Craft on Windows](https://community.kde.org/Get_Involved/development/Windows)

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

## C-Play with Qt6 (Still experimental)

Enable *BUILD_CPLAY_WITH_QT6* in CMake to use Qt6 instead of Qt5.
* Note: C-Play with Qt6 is still experimental and requires more work. Specifically a lot of QML changes from Qt5 to Qt6.*