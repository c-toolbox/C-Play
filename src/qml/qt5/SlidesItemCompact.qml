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

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
    function slideNumText() {
        const rowNumber = pad(root.rowNumber, slidesView.count.toString().length) + ". ";
        return rowNumber;
    }

    backgroundColor: {
        let color = Kirigami.Theme.backgroundColor;
        Qt.hsla(color.hslHue, color.hslSaturation, color.hslLightness, 1);
    }
    font.pointSize: 9
    padding: 2

    onClicked: {}
    onDoubleClicked: {
        app.slides.triggeredSlideIdx = index;
        if (app.slides.selected.layersVisibility === 100 && !visibility_fade_out_animation.running) {
            visibility_fade_out_animation.start();
        }
        if (app.slides.selected.layersVisibility === 0 && !visibility_fade_in_animation.running) {
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
            slidesView.enabled = true;
        }
        onStarted: {
            slidesView.enabled = false;
        }
    }
    RowLayout {
        Label {
            id: slideNum

            font.pointSize: 9
            text: slideNumText()
        }
        Label {
            id: slideNameLbl

            font.pointSize: 9
            text: model.name
            visible: slidesView.currentIndex !== index
        }
        TextInput {
            id: slideNameField

            color: Kirigami.Theme.textColor
            font.pointSize: 9
            maximumLength: 12
            text: model.name
            visible: slidesView.currentIndex === index

            onEditingFinished: {
                app.slides.selected.layersName = slideNameField.text;
                app.slides.updateSelectedSlide();
            }
        }
        Item {
            Layout.fillHeight: true
            // spacer item
            Layout.fillWidth: true
        }
    }
    Label {
        id: numLayers

        font.pointSize: 9
        text: model.layers + (model.layers === 1 ? " layer " : " layers")
    }
    Item {
        implicitWidth: 50
        visible: slidesView.currentIndex !== index

        Label {
            anchors.fill: parent
            horizontalAlignment: Text.AlignHCenter
            text: model.visibility + "%"
            verticalAlignment: Text.AlignVCenter
        }
    }
    VisibilitySlider {
        id: visibilitySlider

        enabled: false
        implicitWidth: 50
        overlayLabel: qsTr("")
        visible: slidesView.currentIndex === index

        onValueChanged: {
            if (!slidesView.enabled) {
                if (value.toFixed(0) !== app.slides.triggeredSlideVisibility) {
                    app.slides.triggeredSlideVisibility = value.toFixed(0);
                }
                if (value.toFixed(0) !== layerView.layerItem.layerVisibility) {
                    layerView.layerItem.layerVisibility = value.toFixed(0);
                }
                app.slides.updateSelectedSlide();
            }
        }
    }
    Connections {
        function onSelectedSlideChanged() {
            visibilitySlider.value = app.slides.selected.layersVisibility;
        }

        target: app.slides
    }
    PropertyAnimation {
        id: visibility_fade_in_animation

        duration: PlaybackSettings.fadeDuration
        property: "value"
        target: visibilitySlider
        to: 100

        onFinished: {
            slidesView.enabled = true;
        }
        onStarted: {
            slidesView.enabled = false;
        }
    }
}
