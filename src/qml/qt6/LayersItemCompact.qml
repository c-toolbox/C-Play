/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

ItemDelegate {
    id: root

    property string iconName: "edit-select-all-layers"
    property int iconWidth: 0
    property string rowNumber: (index + 1).toString()

    function layersNumText() {
        const rowNumber = pad(root.rowNumber, layersView.count.toString().length) + ". ";
        return rowNumber;
    }
    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
    function subText() {
        if(model.type === "Audio")
            return model.type;
        else
            return model.type + model.page + " - " + model.stereoVideo + " " + model.gridToMapOn;
    }

    font.pointSize: 9
    highlighted: layersView.currentIndex === index
    implicitWidth: ListView.view.width
    padding: 0

    Menu {
        id: copyLayerMenu
        MenuItem { 
            action: actions.layerCopyAction
            visible: actions.layerCopyAction.enabled
        }
        MenuItem { 
            action: actions.layerPastePropertiesAction
            visible: actions.layerPastePropertiesAction.enabled
        }
    }

    background: Rectangle {
        anchors.fill: parent
        color: {
            if (highlighted) {
                return Qt.alpha(Kirigami.Theme.highlightColor, 0.6);
            }
            if (hovered) {
                return Qt.alpha(Kirigami.Theme.hoverColor, 0.4);
            }
            if (down) {
                return Qt.alpha(Kirigami.Theme.positiveBackgroundColor, 0.8);
            }
            return Kirigami.Theme.backgroundColor;
        }
    }
    contentItem: Rectangle {
        color: "transparent"
        implicitHeight: 50
        implicitWidth: root.width

        MouseArea {
            id: layerIC_MA
            implicitHeight: parent.height
            implicitWidth: parent.width
            acceptedButtons: Qt.RightButton
            onClicked: {
                copyLayerMenu.popup();
                app.slides.selected.layerToCopy = index;
            }

            Kirigami.IconTitleSubtitle {
                id: its

                anchors.bottomMargin: 15
                anchors.fill: parent
                color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                icon.color: color
                icon.name: iconName
                icon.width: iconWidth
                implicitHeight: 50
                selected: root.down
                subtitle: subText()
                title: (layersView.currentIndex === index ? layersNumText() : layersNumText() + model.title)
            }
            TextInput {
                id: slideTitleField

                anchors.left: its.left
                anchors.leftMargin: 12
                anchors.top: parent.top
                anchors.topMargin: 3
                color: Kirigami.Theme.textColor
                font.pointSize: 9
                maximumLength: 30
                text: model.title
                visible: layersView.currentIndex === index

                onAccepted: {
                    layerView.layerItem.layerTitle = slideTitleField.text;
                }
            }
            Button  {
                flat: true
                visible: app.slides.selected.layersCanBeLocked
                hoverEnabled: false
                anchors.leftMargin: 93
                anchors.bottomMargin: -3
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                icon.name: model.locked ? "object-locked" : "object-unlocked"
                icon.color: model.locked ? "darkred" : "gray"
                icon.width: 20
                icon.height: 20

                onClicked: {
                    if(model.locked) {
                        app.slides.selected.unlockLayer(index);
                    }
                    else {
                        app.slides.selected.lockLayer(index)
                    }
                    if(layersView.currentIndex === index) {
                        layersView.currentIndex = -1;
                    }
                    layersView.currentIndex = index;
                    app.slides.updateSelectedSlide();
                }
            }
            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.topMargin: 5
                width: 10
                height: 10
                radius: 5
                color: (model.status === 2 ? "lime" : model.status === 1 ? "orange" : model.status === 0 ? "crimson" : "black")
            }
            Item {
                anchors.bottom: parent.bottom
                anchors.right: its.right
                implicitHeight: 25
                implicitWidth: 100
                visible: !visibilitySlider.visible

                Rectangle {
                    color: Kirigami.Theme.highlightColor
                    height: parent.height
                    radius: 0
                    width: model.visibility * 0.01 * parent.width
                }

                Label {
                    anchors.centerIn: parent
                    anchors.fill: parent
                    topPadding: 5
                    horizontalAlignment: Text.AlignHCenter
                    text: model.visibility + "%"
                    color:"#fff"
                    font.pointSize: 9
                    layer.enabled: true
                    layer.effect: DropShadow {
                        color: "#111"
                        radius: 5
                        samples: 17
                        spread: 0.3
                        verticalOffset: 1
                    }
                }
            }
            VisibilitySlider {
                id: visibilitySlider

                anchors.bottom: parent.bottom
                anchors.right: its.right
                visible: layersView.currentIndex === index
                implicitWidth: 100
                overlayLabel: qsTr("")

                onValueChanged: {
                    if (!layersView.enabled || visibilitySlider.enabled) {
                        if (value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                            layerView.layerItem.layerVisibility = value.toFixed(0);
                            app.slides.needsSync = true;
                        }
                    }
                }
                Component.onCompleted: {
                    visibilitySlider.value = layerView.layerItem.layerVisibility;
                }
            }
            Item {
                anchors.bottom: parent.bottom
                anchors.right: its.right
                implicitHeight: 20
                implicitWidth: 100
                visible: layersView.currentIndex !== index

                Label {
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    text: model.visibility + "%"
                }
            }
        }
    }

    onClicked: {
        layersView.currentIndex = index;
        slideTitleField.text = model.title;
        layerView.layerItem.layerIdx = index;
        app.slides.selected.layerToCopy = index;
    }
    onDoubleClicked: {
        layerView.layerItem.layerIdx = index;
        if (layerView.layerItem.layerVisibility === 100 && !visibility_fade_out_animation.running) {
            visibility_fade_out_animation.start();
        } else if (layerView.layerItem.layerVisibility < 100 && !visibility_fade_in_animation.running) {
            visibility_fade_in_animation.start();
        }
    }

    PropertyAnimation {
        id: visibility_fade_out_animation

        duration: PlaybackSettings.fadeDuration
        property: "value"
        target: visibilitySlider
        to: 0

        onFinished: {
            layersView.enabled = true;
        }
        onStarted: {
            layersView.enabled = false;
        }
    }
    PropertyAnimation {
        id: visibility_fade_in_animation

        duration: PlaybackSettings.fadeDuration * ((100 - visibilitySlider.value) / 100)
        property: "value"
        target: visibilitySlider
        to: 100

        onFinished: {
            layersView.enabled = true;
        }
        onStarted: {
            layersView.enabled = false;
        }
    }
    Connections {
        function onLayerChanged() {
            if (visibilitySlider.value !== layerView.layerItem.layerVisibility)
                visibilitySlider.value = layerView.layerItem.layerVisibility;
        }
        function onLayerValueChanged() {
            if (visibilitySlider.value !== layerView.layerItem.layerVisibility)
                visibilitySlider.value = layerView.layerItem.layerVisibility;
        }

        target: layerView.layerItem
    }
}
