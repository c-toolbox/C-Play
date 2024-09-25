/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0
import Haruna.Components 1.0 as HC

Kirigami.BasicListItem {
    id: root

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

    backgroundColor: {
        let color = Kirigami.Theme.backgroundColor;
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1);
    }
    font.pointSize: 9
    label: (layersView.currentIndex === index ? layersNumText() : layersNumText() + model.title)
    padding: 2
    subtitle: subText()

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

    Item {
        Layout.fillHeight: true
        Layout.fillWidth: true
        visible: layersView.currentIndex !== index

        Item {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 7
            anchors.rightMargin: -15
            implicitWidth: 50
            visible: layersView.currentIndex !== index

            Label {
                horizontalAlignment: Text.AlignHCenter
                text: model.visibility + "%"
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
 
    MouseArea {
        id: layerIC_MA
        implicitHeight: parent.height
        implicitWidth: parent.width
        acceptedButtons: Qt.RightButton
        onClicked: {
            copyLayerMenu.popup();
            app.slides.selected.layerToCopy = index;
        }
        visible: layersView.currentIndex === index

        Item {
            x: 10
            implicitWidth: 100
            visible: layersView.currentIndex === index

            TextInput {
                id: slideTitleField

                color: Kirigami.Theme.textColor
                font.pointSize: 9
                maximumLength: 18
                text: model.title

                onAccepted: {
                    layerView.layerItem.layerTitle = slideTitleField.text;
                }
            }
        }

        VisibilitySlider {
            id: visibilitySlider

            anchors.right: parent.right
            enabled: app.slides.selectedSlideIdx === -1
            implicitWidth: 50
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
