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

    label: mainText()
    subtitle: subText()
    padding: 0
    font.pointSize: 9
    backgroundColor: {
        let color = Kirigami.Theme.backgroundColor
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1)
    }

    onClicked: {
        layerView.layerItem.layerIdx = index
    }

    onDoubleClicked: {
        layerView.layerItem.layerIdx = index

        if(layerView.layerItem.layerVisibility === 100 && !visibility_fade_out_animation.running){
            visibility_fade_out_animation.start()
        }

        if(layerView.layerItem.layerVisibility === 0 && !visibility_fade_in_animation.running){
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

    VisibilitySlider {
        id: visibilitySlider
        visible: layersView.currentIndex === index
        overlayLabel: qsTr("")
        implicitWidth: 50
        onValueChanged: {
            if(value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                layerView.layerItem.layerVisibility = value.toFixed(0)
            }
        }
    }

    PropertyAnimation {
        id: visibility_fade_in_animation;
        target: visibilitySlider;
        property: "value";
        to: 100;
        duration: PlaybackSettings.fadeDuration;
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

    function mainText() {
        const rowNumber = pad(root.rowNumber, layersView.count.toString().length) + ". "
        return rowNumber + model.title
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
