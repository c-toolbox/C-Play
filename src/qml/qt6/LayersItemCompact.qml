/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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
        return model.type + " - " + model.stereoVideo + " " + model.gridToMapOn;
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
                anchors.leftMargin: 15
                anchors.top: parent.top
                anchors.topMargin: 3
                color: Kirigami.Theme.textColor
                font.pointSize: 9
                maximumLength: 18
                text: model.title
                visible: layersView.currentIndex === index

                onAccepted: {
                    layerView.layerItem.layerTitle = slideTitleField.text;
                }
            }
            VisibilitySlider {
                id: visibilitySlider

                anchors.bottom: parent.bottom
                anchors.right: its.right
                enabled: app.slides.selectedSlideIdx === -1
                implicitWidth: 100
                overlayLabel: qsTr("")
                visible: layersView.currentIndex === index

                onValueChanged: {
                    if (!layersView.enabled || visibilitySlider.enabled) {
                        if (value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                            layerView.layerItem.layerVisibility = value.toFixed(0);
                            app.slides.needsSync = true;
                        }
                    }
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
