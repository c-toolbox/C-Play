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

    padding: 2
    font.pointSize: 9
    backgroundColor: {
        let color = Kirigami.Theme.backgroundColor
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1)
    }

    onClicked: {
    }

    onDoubleClicked: {
        app.slides.triggeredSlideIdx = index

        if(app.slides.selected.layersVisibility === 100 && !visibility_fade_out_animation.running){
            visibility_fade_out_animation.start()
        }

        if(app.slides.selected.layersVisibility === 0 && !visibility_fade_in_animation.running){
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
            slidesView.enabled = false
        }
        onFinished: {
            slidesView.enabled = true
        }
    }

    RowLayout {
        Label {
            id: slideNum
            text: slideNumText()
            font.pointSize: 9
        }

        Label {
            id: slideNameLbl
            visible: slidesView.currentIndex !== index
            text: model.name
            font.pointSize: 9
        }

        TextInput {
            id: slideNameField
            text: model.name
            visible: slidesView.currentIndex === index
            color: Kirigami.Theme.textColor
            font.pointSize: 9
            maximumLength: 12
            onEditingFinished: {
                app.slides.selected.layersName = slideNameField.text
                app.slides.updateSelectedSlide()
            }
        }

        Item {
            // spacer item
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }

    Label {
        id: numLayers
        text: model.layers + (model.layers === 1 ?  " layer " : " layers");
        font.pointSize: 9
    }

    Item {
        implicitWidth: 50
        visible: slidesView.currentIndex !== index
        Label {
            text: model.visibility + "%"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            anchors.fill: parent
        }
    }

    VisibilitySlider {
        id: visibilitySlider
        visible: slidesView.currentIndex === index
        enabled: false
        overlayLabel: qsTr("")
        implicitWidth: 50
        onValueChanged: {
            if(!slidesView.enabled){
                if(value.toFixed(0) !== app.slides.triggeredSlideVisibility) {
                    app.slides.triggeredSlideVisibility = value.toFixed(0)
                }
                if(value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                    layerView.layerItem.layerVisibility = value.toFixed(0)
                }
                app.slides.updateSelectedSlide()
            }
        }
    }

    Connections {
        target: app.slides
        function onSelectedSlideChanged(){
            visibilitySlider.value = app.slides.selected.layersVisibility
        }
    }

    PropertyAnimation {
        id: visibility_fade_in_animation;
        target: visibilitySlider;
        property: "value";
        to: 100;
        duration: PlaybackSettings.fadeDuration;
        onStarted: {
            slidesView.enabled = false
        }
        onFinished: {
            slidesView.enabled = true
        }
    }

    function slideNumText() {
        const rowNumber = pad(root.rowNumber, slidesView.count.toString().length) + ". ";
        return rowNumber;
    }

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
}
