/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

SettingsBasePage {
    id: root

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        GridLayout {
            id: content

            columns: 3

            SettingsHeader {
                Layout.columnSpan: 3
                Layout.fillWidth: true
                text: qsTr("Window & UI")
            }

            Item {
                height: 1
                width: 1
            }
            CheckBox {
                checked: UserInterfaceSettings.mappingModeOnOpenFile
                text: qsTr("Specify mapping modes for \"Open File\" (if NOT a *.cplayfile).")

                onCheckedChanged: {
                    UserInterfaceSettings.mappingModeOnOpenFile = checked;
                    UserInterfaceSettings.save();
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Window fade duration:")
            }
            RowLayout {
                SpinBox {
                    id: winFadeDuration

                    from: 0
                    to: 20000
                    value: UserInterfaceSettings.windowFadeDuration

                    onValueChanged: {
                        UserInterfaceSettings.windowFadeDuration = value;
                        UserInterfaceSettings.save();
                    }
                }
                LabelWithTooltip {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: {
                        qsTr("ms = Fades out/in %1 seconds").arg(Number((winFadeDuration.value * 1.0) / 1000.0).toFixed(3));
                    }
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Item {
                height: 1
                width: 1
            }
            CheckBox {
                checked: UserInterfaceSettings.windowOnTopAtStartup
                text: qsTr("Node windows always on top at startup")

                onCheckedChanged: {
                    UserInterfaceSettings.windowOnTopAtStartup = checked;
                    UserInterfaceSettings.save();
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Item {
                height: 1
                width: 1
            }
            CheckBox {
                checked: UserInterfaceSettings.showHeader
                text: qsTr("Show Top Bar / Header")

                onCheckedChanged: {
                    UserInterfaceSettings.showHeader = checked;
                    UserInterfaceSettings.save();
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Item {
                height: 1
                width: 1
            }
            CheckBox {
                checked: UserInterfaceSettings.showFooter
                text: qsTr("Show Bottom Bar / Footer")

                onCheckedChanged: {
                    UserInterfaceSettings.showFooter = checked;
                    UserInterfaceSettings.save();
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Item {
                height: 1
                width: 1
            }
            CheckBox {
                id: idleModeCheckBox
                checked: UserInterfaceSettings.idleModeOn
                text: qsTr("Enable idle mode to hide UI after inactivity")

                onCheckedChanged: {
                    UserInterfaceSettings.idleModeOn = checked;
                    UserInterfaceSettings.save();
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Idle mode time:")
                enabled: UserInterfaceSettings.idleModeOn
            }
            RowLayout {
                SpinBox {
                    editable: true
                    from: 1
                    to: 3600
                    value: UserInterfaceSettings.idleModeTime
                    enabled: UserInterfaceSettings.idleModeOn

                    onValueChanged: {
                        UserInterfaceSettings.idleModeTime = value.toFixed(0);
                        UserInterfaceSettings.save();
                    }
                }
                Label {
                    Layout.alignment: Qt.AlignLeft
                    text: qsTr("(seconds)")
                    enabled: UserInterfaceSettings.idleModeOn
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Color scheme")
            }
            ComboBox {
                id: colorThemeSwitcher

                model: app.colorSchemesModel
                textRole: "display"

                delegate: ItemDelegate {
                    Kirigami.Theme.colorSet: Kirigami.Theme.View
                    highlighted: model.display === UserInterfaceSettings.colorScheme
                    width: colorThemeSwitcher.width

                    contentItem: RowLayout {
                        Kirigami.Icon {
                            Layout.preferredHeight: Kirigami.Units.iconSizes.small
                            Layout.preferredWidth: Kirigami.Units.iconSizes.small
                            source: model.decoration
                        }
                        Label {
                            color: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            text: model.display
                        }
                    }
                }

                Component.onCompleted: currentIndex = find(UserInterfaceSettings.colorScheme)
                onActivated: {
                    UserInterfaceSettings.colorScheme = colorThemeSwitcher.textAt(index);
                    UserInterfaceSettings.save();
                    app.activateColorScheme(UserInterfaceSettings.colorScheme);
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("GUI Style")
            }
            ComboBox {
                id: guiStyleComboBox

                textRole: "key"

                model: ListModel {
                    id: stylesModel

                    ListElement {
                        key: "Breeeze Dark"
                    }
                }

                Component.onCompleted: {
                    // populate the model with the available styles
                    for (let i = 0; i < app.availableGuiStyles().length; ++i) {
                        stylesModel.append({
                            key: app.availableGuiStyles()[i]
                        });
                    }

                    // set the saved style as the current item in the combo box
                    for (let j = 0; j < stylesModel.count; ++j) {
                        if (stylesModel.get(j).key === UserInterfaceSettings.guiStyle) {
                            currentIndex = j;
                            break;
                        }
                    }
                }
                onActivated: {
                    UserInterfaceSettings.guiStyle = model.get(index).key;
                    app.setGuiStyle(UserInterfaceSettings.guiStyle);
                    // some themes can cause a crash
                    // the timer prevents saving the crashing theme,
                    // which would cause the app to crash on startup
                    saveGuiStyleTimer.start();
                }

                Timer {
                    id: saveGuiStyleTimer

                    interval: 1000
                    repeat: false
                    running: false

                    onTriggered: UserInterfaceSettings.save()
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Item {
                height: 1
                width: 1
            }
            CheckBox {
                checked: UserInterfaceSettings.useBreezeIconTheme
                text: qsTr("Use Breeze icon theme")

                onCheckedChanged: {
                    UserInterfaceSettings.useBreezeIconTheme = checked;
                    UserInterfaceSettings.save();
                }

                ToolTip {
                    text: qsTr("Sets the icon theme to breeze.\nRequires restart.")
                }
            }
            Item {
                Layout.fillWidth: true
            }

            // OSD Font Size
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Osd font size")
            }
            Item {
                height: osdFontSize.height

                SpinBox {
                    id: osdFontSize

                    // used to prevent osd showing when opening the page
                    property bool completed: false

                    editable: true
                    from: 0
                    to: 100
                    value: UserInterfaceSettings.osdFontSize

                    Component.onCompleted: completed = true
                    onValueChanged: {
                        if (completed) {
                            osd.label.font.pointSize = osdFontSize.value;
                            osd.message("Test osd font size");
                            UserInterfaceSettings.osdFontSize = osdFontSize.value;
                            UserInterfaceSettings.save();
                        }
                    }
                }
            }
            Item {
                Layout.fillWidth: true
            }
        }

        GridLayout {
            id: contentFloatingLayerWindow

            columns: 3

            SettingsHeader {
                Layout.columnSpan: 3
                Layout.fillWidth: true
                text: qsTr("Floating layer settings")
                level: 4
            }
            
            RowLayout {
                Layout.columnSpan: 3
                Layout.leftMargin: 100
                Layout.rightMargin: 250

                Button {
                    icon.name: "layer-new"
                    text: qsTr("Save values below and create floating window layer")

                    onClicked: {
                        if (layerCoreProps.typeComboBox.currentIndex >= 0) {
                            UserInterfaceSettings.floatingWindowLayerType = layerCoreProps.typeComboBox.currentIndex + 1;

                            if (layerCoreProps.typeComboBox.currentText === "NDI") {
                                UserInterfaceSettings.floatingWindowLayerPath = layerCoreProps.ndiSenderComboBox.currentText;
                            } else if (layerCoreProps.typeComboBox.currentText === "Spout") {
                                UserInterfaceSettings.floatingWindowLayerPath = layerCoreProps.spoutSenderComboBox.currentText;
                            } else if (layerCoreProps.typeComboBox.currentText === "Stream") {
                                if(layerCoreProps.streamsLayout.customEntry){
                                    UserInterfaceSettings.floatingWindowLayerPath = layerCoreProps.streamCustomEntryField.text;
                                } else {
                                    UserInterfaceSettings.floatingWindowLayerPath = layerCoreProps.streamsComboBox.currentValue;
                                }                            
                            } else if (layerCoreProps.typeComboBox.currentText === "Text") {
                                UserInterfaceSettings.floatingWindowLayerPath = layerCoreProps.textForLayer.text;
                            } else {
                                UserInterfaceSettings.floatingWindowLayerPath = layerCoreProps.fileForLayer.text;
                            }
                        } else {
                            UserInterfaceSettings.floatingWindowLayerType = -1;
                            UserInterfaceSettings.floatingWindowLayerPath = "";
                        }

                        UserInterfaceSettings.floatingWindowWidth = floatingWindowWidth.value;
                        UserInterfaceSettings.floatingWindowHeight = floatingWindowHeight.value;
                        UserInterfaceSettings.floatingWindowPosX = floatingWindowPosX.value;
                        UserInterfaceSettings.floatingWindowPosY = floatingWindowPosY.value;
                        UserInterfaceSettings.floatingWindowVisibleAtStartup = showFloatingWindowAtStartupCheckBox.checked;
                        UserInterfaceSettings.floatingWindowVolume = floatingWindowVolume.value.toFixed(0);
                        UserInterfaceSettings.floatingWindowShowsMainVideoLayer = !showCustomLayerInFloatingWindowCheckBox.checked;
                        UserInterfaceSettings.save();

                        if(UserInterfaceSettings.floatingWindowLayerType >= 0 && UserInterfaceSettings.floatingWindowLayerPath !== ""){
                            floatingLayerViewItem.createLayer(UserInterfaceSettings.floatingWindowLayerType, UserInterfaceSettings.floatingWindowLayerPath);
                        }
                    }
                }
            }
            
            Item {
                height: 1
                width: 1
            }
            CheckBox {
                id: showFloatingWindowAtStartupCheckBox

                checked: UserInterfaceSettings.floatingWindowVisibleAtStartup
                enabled: true
                text: qsTr("Show floating window at startup")

                Component.onCompleted: {
                    checked = UserInterfaceSettings.floatingWindowVisibleAtStartup;
                }
            }
            Item {
                // spacer item
                Layout.fillWidth: true
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Window size:")
            }
            RowLayout {
                SpinBox {
                    id: floatingWindowWidth
                    editable: true
                    from: 32
                    to: 8192
                    value: UserInterfaceSettings.floatingWindowWidth

                    Component.onCompleted: {
                        floatingWindowWidth.value = UserInterfaceSettings.floatingWindowWidth;
                    }
                }
                Label {
                    Layout.alignment: Qt.AlignCenter
                    text: qsTr("x")
                }
                SpinBox {
                    id: floatingWindowHeight
                    editable: true
                    from: 32
                    to: 8192
                    value: UserInterfaceSettings.floatingWindowHeight

                    Component.onCompleted: {
                        floatingWindowHeight.value = UserInterfaceSettings.floatingWindowHeight;
                    }
                }
            }
            Item {
                // spacer item
                Layout.fillWidth: true
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Window position:")
            }
            RowLayout {
                SpinBox {
                    id: floatingWindowPosX
                    editable: true
                    from: 32
                    to: 8192
                    value: UserInterfaceSettings.floatingWindowPosX

                    Component.onCompleted: {
                        floatingWindowPosX.value = UserInterfaceSettings.floatingWindowPosX;
                    }
                }
                Label {
                    Layout.alignment: Qt.AlignCenter
                    text: qsTr("x")
                }
                SpinBox {
                    id: floatingWindowPosY
                    editable: true
                    from: 32
                    to: 8192
                    value: UserInterfaceSettings.floatingWindowPosY

                    Component.onCompleted: {
                        floatingWindowPosY.value = UserInterfaceSettings.floatingWindowPosY;
                    }
                }
            }
            Item {
                // spacer item
                Layout.fillWidth: true
            }

            Item {
                height: 1
                width: 1
            }
            CheckBox {
                id: showCustomLayerInFloatingWindowCheckBox

                checked: !UserInterfaceSettings.floatingWindowShowsMainVideoLayer
                enabled: true
                text: qsTr("Show custom layer instead of main video layer")

                Component.onCompleted: {
                    checked = !UserInterfaceSettings.floatingWindowShowsMainVideoLayer;
                }
            }
            Item {
                // spacer item
                Layout.fillWidth: true
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("Layer Volume:")
            }
            SpinBox {
                id: floatingWindowVolume
                editable: true
                enabled: showCustomLayerInFloatingWindowCheckBox.checked
                from: 0
                to: 100
                value: UserInterfaceSettings.floatingWindowVolume

                onValueChanged: {
                    UserInterfaceSettings.floatingWindowVolume = value.toFixed(0);
                    floatingLayerViewItem.layerVolume = UserInterfaceSettings.floatingWindowVolume;
                }

                Component.onCompleted: {
                    floatingLayerViewItem.layerVolume = UserInterfaceSettings.floatingWindowVolume;
                }
            }
            Item {
                // spacer item
                Layout.fillWidth: true
            }
        }
            
        LayerCoreProperties {
            id: layerCoreProps
            columns: 3
            enabled: showCustomLayerInFloatingWindowCheckBox.checked
            Layout.leftMargin: 50
            Layout.rightMargin: 250

            showSpacers: true
            showTitleParams: false
            showGridParams: false
            showStereoParams: false

            Component.onCompleted: {
                // set the saved values
                if (UserInterfaceSettings.floatingWindowLayerType > 0) {
                    typeComboBox.currentIndex = UserInterfaceSettings.floatingWindowLayerType - 1;
                } else {
                    typeComboBox.currentIndex = -1;
                }
                if (UserInterfaceSettings.floatingWindowLayerPath !== "") {
                    fileForLayer.text = UserInterfaceSettings.floatingWindowLayerPath;
                } else {
                    fileForLayer.text = "";
                }
            }
        }
    }
}
