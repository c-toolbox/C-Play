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

    label: (layersView.currentIndex === index ? layersNumText() : layersNumText() + model.title)
    subtitle: subText()
    padding: 2
    font.pointSize: 9
    backgroundColor: {
        let color = Kirigami.Theme.backgroundColor
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1)
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

    Item {
        implicitWidth: 50
        visible: layersView.currentIndex !== index
        Label {
            text: model.visibility + "%"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.fill: parent
        }
    }

    Item {
        implicitWidth: 100
        visible: layersView.currentIndex === index
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 14
        TextInput {
            id: slideTitleField
            text: model.title
            color: Kirigami.Theme.textColor
            font.pointSize: 9
            maximumLength: 18
            onAccepted: {
                layerView.layerItem.layerTitle = slideTitleField.text
            }
        }
    }

    VisibilitySlider {
        id: visibilitySlider
        visible: layersView.currentIndex === index
        enabled: app.slides.selectedSlideIdx === -1
        value: model.visibility
        overlayLabel: qsTr("")
        implicitWidth: 50
        onValueChanged: {
            if(!layersView.enabled || visibilitySlider.enabled){
                if(value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                    layerView.layerItem.layerVisibility = value.toFixed(0)
                    app.slides.needsSync = true
                }
            }
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

    Connections {
        target: layerView.layerItem
        function onLayerValueChanged(){
            if(visibilitySlider.value !== layerView.layerItem.layerVisibility)
                visibilitySlider.value = layerView.layerItem.layerVisibility
        }
    }

    function layersNumText() {
        const rowNumber = pad(root.rowNumber, layersView.count.toString().length) + ". ";
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
