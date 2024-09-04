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
    highlighted: slidesView.currentIndex === index
    down: app.slides.triggeredSlideIdx === index

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
            slidesView.enabled = false
        }
        onFinished: {
            slidesView.enabled = true
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
            title: (slidesView.currentIndex === index ? slideNumText() : slideNumText() + model.name)
            subtitle: subText()
            color: root.hovered || root.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
            selected: root.down
        }

        TextInput {
            id: slideNameField
            text: model.name
            anchors.top: parent.top
            anchors.left: its.left
            anchors.topMargin: 3
            anchors.leftMargin: 12
            visible: slidesView.currentIndex === index
            color: Kirigami.Theme.textColor
            font.pointSize: 9
            maximumLength: 12
            onEditingFinished: {
                app.slides.selected.layersName = slideNameField.text
                app.slides.updateSelectedSlide()
            }
        }
 
        VisibilitySlider {
            id: visibilitySlider
            overlayLabel: qsTr("")
            visible: slidesView.currentIndex === index
            enabled: false
            implicitWidth: 100
            anchors.bottom: parent.bottom
            anchors.right: its.right
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

        Item {
            visible: slidesView.currentIndex !== index
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
        slidesView.currentIndex = index
    }

    onDoubleClicked: {
        slidesView.currentIndex = index
        app.slides.triggeredSlideIdx = index

        if(app.slides.selected.layersVisibility === 100 && !visibility_fade_out_animation.running){
            visibility_fade_out_animation.start()
        }

        if(app.slides.selected.layersVisibility === 0 && !visibility_fade_in_animation.running){
            visibility_fade_in_animation.start()
        }
    }

    Connections {
        target: app.slides
        function onSelectedSlideChanged(){
            visibilitySlider.value = app.slides.selected.layersVisibility
        }
    }

    function slideNumText() {
        const rowNumber = pad(root.rowNumber, slidesView.count.toString().length) + ". ";
        return rowNumber;
    }

    function subText() {
        return model.layers + (model.layers === 1 ?  " layer" : " layers");
    }

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
}
