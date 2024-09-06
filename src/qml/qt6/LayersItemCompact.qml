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

    implicitWidth: ListView.view.width
    padding: 0
    font.pointSize: 9
    highlighted: layersView.currentIndex === index

    background: Rectangle {
        anchors.fill: parent
        color: {
            if (highlighted) {
                return Qt.alpha(Kirigami.Theme.highlightColor, 0.6)
            }

            if (hovered) {
                return Qt.alpha(Kirigami.Theme.hoverColor, 0.4)
            }

            if(down) {
                return Qt.alpha(Kirigami.Theme.positiveBackgroundColor, 0.8)
            }

            return Kirigami.Theme.backgroundColor
        }
    }

    PropertyAnimation {
        id: visibility_fade_out_animation;
        target: visibilitySlider;
        property: "value";
        to: 0;
        duration: PlaybackSettings.fadeDuration;
        onStarted: {
            layersView.enabled = false
        }
        onFinished: {
            layersView.enabled = true
        }
    }

    PropertyAnimation {
        id: visibility_fade_in_animation;
        target: visibilitySlider;
        property: "value";
        to: 100;
        duration: PlaybackSettings.fadeDuration * ((100 - visibilitySlider.value) / 100);
        onStarted: {
            layersView.enabled = false
        }
        onFinished: {
            layersView.enabled = true
        }
    }

    contentItem: Rectangle {
        implicitWidth: root.width
        implicitHeight: 50
        color: "transparent"
        Kirigami.IconTitleSubtitle {
            id: its
            anchors.fill: parent
            anchors.bottomMargin: 15
            implicitHeight: 50
            icon.name: iconName
            icon.color: color
            icon.width: iconWidth
            title: (layersView.currentIndex === index ? layersNumText() : layersNumText() + model.title)
            subtitle: subText()
            color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
            selected: root.down
        }

        TextInput {
            id: slideTitleField
            text: model.title
            anchors.top: parent.top
            anchors.left: its.left
            anchors.topMargin: 3
            anchors.leftMargin: 12
            visible: layersView.currentIndex === index
            color: Kirigami.Theme.textColor
            font.pointSize: 9
            maximumLength: 18
            onAccepted: {
                layerView.layerItem.layerTitle = slideTitleField.text
            }
        }

        VisibilitySlider {
            id: visibilitySlider
            overlayLabel: qsTr("")
            visible: layersView.currentIndex === index
            enabled: app.slides.selectedSlideIdx === -1
            implicitWidth: 100
            anchors.bottom: parent.bottom
            anchors.right: its.right
            value: model.visibility
            onValueChanged: {
                if(!layersView.enabled || visibilitySlider.enabled){
                    if(value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                        layerView.layerItem.layerVisibility = value.toFixed(0)
                        app.slides.needsSync = true
                    }
                }
            }
        }

        Item {
            visible: layersView.currentIndex !== index
            implicitWidth: 100
            implicitHeight: 20
            anchors.bottom: parent.bottom
            anchors.right: its.right
            Label {
                text: model.visibility + "%"
                horizontalAlignment: Text.AlignHCenter
                anchors.fill: parent
            }
        }
    }

    onClicked: {
        slideTitleField.text = model.title
        layerView.layerItem.layerIdx = index
    }

    onDoubleClicked: {
        layerView.layerItem.layerIdx = index

        if(layerView.layerItem.layerVisibility === 100 && !visibility_fade_out_animation.running){
            visibility_fade_out_animation.start()
        }
        else if(layerView.layerItem.layerVisibility < 100 && !visibility_fade_in_animation.running){
            visibility_fade_in_animation.start()
        }
    }

    Connections {
        target: layerView.layerItem
        function onLayerValueChanged(){
            if(visibilitySlider.value !== layerView.layerItem.layerVisibility)
                visibilitySlider.value = layerView.layerItem.layerVisibility
        }
    }

    function layersNumText() {
        const rowNumber = pad(root.rowNumber, layersView.count.toString().length) + ". "
        return rowNumber;
    }

    function subText() {
        return model.type + " - " + model.stereoVideo + " " + model.gridToMapOn
    }

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
}
