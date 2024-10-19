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

    function pad(number, length) {
        while (number.length < length)
            number = "0" + number;
        return number;
    }
    function slideNumText() {
        const rowNumber = pad(root.rowNumber, slidesView.count.toString().length) + ". ";
        return rowNumber;
    }
    function subText() {
        return model.layers + (model.layers === 1 ? " layer" : " layers");
    }

    down: app.slides.triggeredSlideIdx === index
    font.pointSize: 9
    highlighted: slidesView.currentIndex === index
    implicitWidth: ListView.view.width
    padding: 0

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
    
    Menu {
        id: pasteLayerMenu
        MenuItem { 
            action: actions.layerPasteAction 
            visible: actions.layerPasteAction.enabled
        }
    }
    
    contentItem: Rectangle {
        color: "transparent"
        implicitHeight: 50
        implicitWidth: root.width

        MouseArea {
            id: slideIC_MA
            implicitHeight: parent.height
            implicitWidth: parent.width
            acceptedButtons: Qt.RightButton
            onClicked: {
                if(app.slides.copyIsAvailable()){
                    pasteLayerMenu.popup();
                    app.slides.slideToPaste = index;
                }
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
                title: (slidesView.currentIndex === index ? slideNumText() : slideNumText() + model.name)
            }
            TextInput {
                id: slideNameField

                anchors.left: its.left
                anchors.leftMargin: 12
                anchors.top: parent.top
                anchors.topMargin: 3
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
            VisibilitySlider {
                id: visibilitySlider

                anchors.bottom: parent.bottom
                anchors.right: its.right
                enabled: false
                implicitWidth: 100
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
            Item {
                anchors.bottom: parent.bottom
                anchors.right: its.right
                implicitHeight: 20
                implicitWidth: 100
                visible: slidesView.currentIndex !== index

                Label {
                    anchors.fill: parent
                    horizontalAlignment: Text.AlignHCenter
                    text: model.visibility + "%"
                }
            }
        }
    }

    onClicked: {
        slidesView.currentIndex = index;
        app.slides.slideToPaste = index;
    }
    onDoubleClicked: {
        slidesView.currentIndex = index;
        app.slides.slideFadeTime = PresentationSettings.fadeDurationToNextSlide;
        app.slides.triggeredSlideIdx = index;
    }

    PropertyAnimation {
        id: visibility_fade_out_animation

        duration: PlaybackSettings.fadeDuration
        property: "value"
        target: visibilitySlider
        to: 0

        onFinished: {
            slidesView.enabled = true;
            app.slides.pauseLayerUpdate = false;
            app.action("slidePrevious").enabled = true;
            app.action("slideNext").enabled = true;
        }
        onStarted: {
            slidesView.enabled = false;
            app.slides.pauseLayerUpdate = true;
            app.action("slidePrevious").enabled = false;
            app.action("slideNext").enabled = false;
        }
    }
    PropertyAnimation {
        id: visibility_fade_in_animation

        duration: PlaybackSettings.fadeDuration
        property: "value"
        target: visibilitySlider
        to: 100

        onFinished: {
            slidesView.enabled = true;
            app.slides.pauseLayerUpdate = false;
            app.action("slidePrevious").enabled = true;
            app.action("slideNext").enabled = true;
        }
        onStarted: {
            slidesView.enabled = false;
            app.slides.pauseLayerUpdate = true;
            app.action("slidePrevious").enabled = false;
            app.action("slideNext").enabled = false;
        }
    }
    Connections {
        function onSelectedSlideChanged() {
            if(app.slides.selected) {
                visibilitySlider.value = app.slides.selected.layersVisibility;
            }
        }

        function onTriggeredSlideChanged() {
            if(app.slides.selected) {
                if (app.slides.selected.layersVisibility === 100 && !visibility_fade_out_animation.running) {
                    visibility_fade_out_animation.duration = app.slides.slideFadeTime / 2;
                    visibility_fade_out_animation.start();
                }
                if (app.slides.selected.layersVisibility === 0 && !visibility_fade_in_animation.running) {
                    visibility_fade_in_animation.duration = app.slides.slideFadeTime / 2;
                    visibility_fade_in_animation.start();
                }
            }
        }

        target: app.slides
    }
}
