/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    id: layerWindow

    property var layerItem: layerViewItem
    property var roiEdit: undefined
    property var selection: undefined
    property var pdfPage: undefined
    property var mediaControls: undefined
    property var audioControls: undefined
    property var streamControls: undefined

    function createRoiComponents() {
        if (!selection) {
            selection = selectionComponent.createObject(layerViewItem, {
                "x": layerViewItem.roiOffset.x,
                "y": layerViewItem.roiOffset.y,
                "width": layerViewItem.roiSize.width,
                "height": layerViewItem.roiSize.height
            });
        }
        if (!roiEdit) {
            roiEdit = roiEditComponent.createObject(layerViewItem);
        }
    }
    function destroyRoiComponents() {
        if (selection)
            selection.destroy();
        if (roiEdit)
            roiEdit.destroy();
    }

    function createPageComponents() {
        if (!pdfPage) {
            pdfPage = pdfPageComponent.createObject(layerViewItem);
        }
    }
    function destroyPageComponents() {
        if (pdfPage) {
            pdfPage.destroy();
        }
    }

    function createMediaComponents() {
        if (!mediaControls) {
            mediaControls = mediaComponent.createObject(layerViewItem);
        }
    }
    function destroyMediaComponents() {
        if (mediaControls) {
            mediaControls.destroy();
        }
    }

    function createStreamComponents() {
        if (!streamControls) {
            streamControls = streamComponent.createObject(layerViewItem);
        }
    }
    function destroyStreamComponents() {
        if (streamControls) {
            streamControls.destroy();
        }
    }

    function createAudioComponents() {
        if (!audioControls) {
            audioControls = audioComponent.createObject(layerViewItem);
        }
    }
    function destroyAudioComponents() {
        if (audioControls) {
            audioControls.destroy();
        }
    }

    color: Kirigami.Theme.alternateBackgroundColor
    height: 630
    minimumWidth: 690
    title: qsTr("")
    visible: false
    width: 690

    Component.onCompleted: {
        if (window.x > width) {
            x = window.x - width;
        } else {
            x = Screen.width / 2 - width;
        }
        y = Screen.height / 2 - height / 2;
    }
    onClosing: {
        destroyRoiComponents();
        destroyPageComponents();
        destroyAudioComponents();
        destroyMediaComponents();
        destroyStreamComponents();
    }
    onVisibilityChanged: {
        if (visible) {
            if(layerViewItem.layerIdx !== -1){
                if (layerViewItem.layerRoiEnabled) {
                    createRoiComponents();
                }
                if (layerViewItem.layerTypeName === "PDF") {
                    createPageComponents();
                }
                else if (layerViewItem.layerTypeName === "Video" 
                        || layerViewItem.layerTypeName === "Audio") {
                    createAudioComponents();
                    createMediaComponents();
                }
                else if (layerViewItem.layerTypeName === "Stream") {
                    createStreamComponents();
                }
                else if (layerViewItem.layerTypeName === "NDI") {
                    createAudioComponents();
                }
            }
            else {
                destroyRoiComponents();
                destroyPageComponents();
                destroyAudioComponents();
                destroyMediaComponents();
                destroyStreamComponents();
            }
        }
    }

    ToolBar {
        id: toolBarEmptyLeft

        anchors.left: parent.left
        anchors.right: toolBarLayerView.left
        visible: layerViewItem.layerIdx !== -1
    }
    ToolBar {
        id: toolBarLayerView

        anchors.horizontalCenter: parent.horizontalCenter
        visible: layerViewItem.layerIdx !== -1

        Row {
            RowLayout {
                id: layerHeaderRow

                Item {
                    Layout.fillHeight: true
                    Layout.fillWidth: true
                }
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Stereo:")
                    enabled: layerViewItem.layerTypeName !== "Audio"
                }
                ComboBox {
                    id: stereoscopicModeForLayer

                    Layout.fillWidth: true
                    focusPolicy: Qt.NoFocus
                    textRole: "mode"
                    enabled: layerViewItem.layerTypeName !== "Audio"

                    model: ListModel {
                        id: stereoscopicModeForLayerList

                        ListElement {
                            mode: "2D (mono)"
                            value: 0
                        }
                        ListElement {
                            mode: "3D (side-by-side)"
                            value: 1
                        }
                        ListElement {
                            mode: "3D (top-bottom)"
                            value: 2
                        }
                        ListElement {
                            mode: "3D (top-bottom+flip)"
                            value: 3
                        }
                    }

                    Component.onCompleted: {}
                    onActivated: {
                        layerViewItem.layerStereoMode = model.get(index).value;
                    }
                }
                Label {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Grid:")
                    enabled: layerViewItem.layerTypeName !== "Audio"
                }
                ComboBox {
                    id: gridModeForLayer

                    Layout.fillWidth: true
                    focusPolicy: Qt.NoFocus
                    textRole: "mode"
                    enabled: layerViewItem.layerTypeName !== "Audio"

                    model: ListModel {
                        id: gridModeForLayerList

                        ListElement {
                            mode: "None/Pre-split"
                            value: 0
                        }
                        ListElement {
                            mode: "Plane"
                            value: 1
                        }
                        ListElement {
                            mode: "Dome"
                            value: 2
                        }
                        ListElement {
                            mode: "Sphere EQR"
                            value: 3
                        }
                        ListElement {
                            mode: "Sphere EAC"
                            value: 4
                        }
                    }

                    Component.onCompleted: {}
                    onActivated: {
                        layerViewItem.layerGridMode = model.get(index).value;
                    }
                }
                ToolButton {
                    id: configureGridParameters

                    checkable: true
                    checked: layerViewGridParams.visible
                    enabled: layerViewItem.layerTypeName !== "Audio"
                    focusPolicy: Qt.NoFocus
                    icon.name: "configure"

                    onClicked: {
                        layerViewGridParams.visible = checked;
                    }

                    ToolTip {
                        text: qsTr("Configure Grid Parameters")
                    }
                }
                PropertyAnimation {
                    id: visibility_fade_out_animation

                    duration: PlaybackSettings.fadeDuration
                    property: "value"
                    target: visibilitySlider
                    to: 0
                }
                ToolButton {
                    id: fade_image_out

                    enabled: layerViewItem.layerVisibility !== 0
                    focusPolicy: Qt.NoFocus
                    icon.name: "view-hidden"

                    onClicked: {
                        if (!visibility_fade_out_animation.running) {
                            visibility_fade_out_animation.start();
                        }
                    }
                }
                VisibilitySlider {
                    id: visibilitySlider

                    overlayLabel: qsTr("Layer visibility: ")

                    onValueChanged: {
                        if (value.toFixed(0) !== layerViewItem.layerVisibility) {
                            layerViewItem.layerVisibility = value.toFixed(0);
                            app.slides.needsSync = true;
                        }
                    }
                    Component.onCompleted: {
                        visibilitySlider.value = layerViewItem.layerVisibility;
                    }
                }
                PropertyAnimation {
                    id: visibility_fade_in_animation

                    duration: PlaybackSettings.fadeDuration
                    property: "value"
                    target: visibilitySlider
                    to: 100
                }
                ToolButton {
                    id: fade_image_in

                    enabled: layerViewItem.layerVisibility !== 100
                    focusPolicy: Qt.NoFocus
                    icon.name: "view-visible"

                    onClicked: {
                        if (!visibility_fade_in_animation.running) {
                            visibility_fade_in_animation.start();
                        }
                    }
                }
                ToolButton {
                    id: roiButton

                    checkable: true
                    checked: layerViewItem.layerRoiEnabled
                    enabled: layerViewItem.layerTypeName !== "Audio"
                    focusPolicy: Qt.NoFocus
                    icon.color: (checked ? "lime" : "crimson")
                    icon.name: "trim-to-selection"
                    text: qsTr("ROI")

                    onClicked: {
                        layerViewItem.layerRoiEnabled = checked;
                        if (checked) {
                            createRoiComponents();
                        } else {
                            destroyRoiComponents();
                        }
                    }

                    ToolTip {
                        text: qsTr("Region of interest")
                    }
                }
            }
        }
    }
    ToolBar {
        id: toolBarEmptyRight

        anchors.left: toolBarLayerView.right
        anchors.right: parent.right
        visible: layerViewItem.layerIdx !== -1
    }
    LayerQtItem {
        id: layerViewItem

        anchors.top: toolBarLayerView.bottom
        height: parent.height - toolBarLayerView.height
        width: parent.width

        Label {
            id: noLayerLabel

            anchors.fill: parent
            font.family: "Helvetica"
            font.pointSize: 18
            horizontalAlignment: Text.AlignHCenter
            text: "Select a layer in the list to view it here..."
            verticalAlignment: Text.AlignVCenter
            visible: layerViewItem.layerIdx === -1
            wrapMode: Text.WordWrap
        }
        MouseArea {
            anchors.fill: parent
            visible: layerViewItem.layerIdx !== -1

            onClicked: {}
        }
        Component {
            id: roiEditComponent

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                topPadding: 60
                visible: selection != undefined

                RowLayout {
                    Label {
                        font.pointSize: 10
                        text: "Region of Interest - Pos X:"
                    }
                    SpinBox {
                        id: spinX

                        to: layerViewItem.textureSize.width
                        value: layerViewItem.roiTexOffset.x

                        onValueChanged: {
                            if (value != layerViewItem.roiTexOffset.x)
                                layerViewItem.roiTexOffset = Qt.point(value, layerViewItem.roiTexOffset.y);
                        }
                    }
                    Label {
                        font.pointSize: 10
                        text: "Pos Y:"
                    }
                    SpinBox {
                        id: spinY

                        to: layerViewItem.textureSize.height
                        value: layerViewItem.roiTexOffset.y

                        onValueChanged: {
                            if (value != layerViewItem.roiTexOffset.y)
                                layerViewItem.roiTexOffset = Qt.point(layerViewItem.roiTexOffset.x, value);
                        }
                    }
                    Label {
                        font.pointSize: 10
                        text: "Width:"
                    }
                    SpinBox {
                        id: spinW

                        to: layerViewItem.textureSize.width
                        value: layerViewItem.roiTexSize.width

                        onValueChanged: {
                            if (value != layerViewItem.roiTexSize.width)
                                layerViewItem.roiTexSize = Qt.size(value, layerViewItem.roiTexSize.height);
                        }
                    }
                    Label {
                        font.pointSize: 10
                        text: "Height:"
                    }
                    SpinBox {
                        id: spinH

                        to: layerViewItem.textureSize.height
                        value: layerViewItem.roiTexSize.height

                        onValueChanged: {
                            if (value != layerViewItem.roiTexSize.height)
                                layerViewItem.roiTexSize = Qt.size(layerViewItem.roiTexSize.width, value);
                        }
                    }
                    Button {
                        icon.name: "edit-reset"
                        text: "Reset"

                        onClicked: {
                            layerViewItem.roiTexOffset = Qt.point(0, 0);
                            layerViewItem.roiTexSize = layerViewItem.textureSize;
                        }

                        ToolTip {
                            text: qsTr("Reset ROI values to full image")
                        }
                    }
                }
                Connections {
                    function onRoiChanged() {
                        spinX.value = layerViewItem.roiTexOffset.x;
                        spinY.value = layerViewItem.roiTexOffset.y;
                        spinW.value = layerViewItem.roiTexSize.width;
                        spinH.value = layerViewItem.roiTexSize.height;
                    }

                    target: layerViewItem
                }
            }
        }
        Component {
            id: selectionComponent

            Rectangle {
                id: selComp

                property int rulersSize: 18

                color: "#354682B4"
                height: layerViewItem.roiSize.height
                width: layerViewItem.roiSize.width
                x: layerViewItem.roiOffset.x
                y: layerViewItem.roiOffset.y

                border {
                    color: "steelblue"
                    width: 2
                }
                Connections {
                    function onRoiChanged() {
                        selComp.x = layerViewItem.roiOffset.x;
                        selComp.y = layerViewItem.roiOffset.y;
                        selComp.width = layerViewItem.roiSize.width;
                        selComp.height = layerViewItem.roiSize.height;
                    }

                    target: layerViewItem
                }
                MouseArea {
                    id: dragArea

                    // drag mouse area
                    anchors.fill: parent

                    onDoubleClicked:
                    // destroy component
                    {}
                    onPositionChanged: {
                        if (drag.active) {
                            layerViewItem.setRoi(Qt.point(selComp.x, selComp.y), Qt.size(selComp.width, selComp.height));
                        }
                    }

                    drag {
                        axis: Drag.XAndYAxis
                        maximumX: layerViewItem.viewOffset.x + layerViewItem.viewSize.width - selComp.width
                        maximumY: layerViewItem.viewOffset.y + layerViewItem.viewSize.height - selComp.height
                        minimumX: layerViewItem.viewOffset.x
                        minimumY: layerViewItem.viewOffset.y
                        smoothed: true
                        target: parent
                    }
                }
                Rectangle { // Left
                    id: rectangleLeft

                    anchors.horizontalCenter: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    color: "green"
                    height: rulersSize
                    radius: rulersSize
                    width: rulersSize

                    MouseArea {
                        anchors.fill: parent

                        onMouseXChanged: {
                            if (drag.active) {
                                layerViewItem.dragLeft(mouseX);
                            }
                        }

                        drag {
                            axis: Drag.XAxis
                            target: parent
                        }
                    }
                }
                Rectangle { //Right
                    id: rectangleRight

                    anchors.horizontalCenter: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    color: "red"
                    height: rulersSize
                    radius: rulersSize
                    width: rulersSize

                    MouseArea {
                        anchors.fill: parent

                        onMouseXChanged: {
                            if (drag.active) {
                                layerViewItem.dragRight(mouseX);
                            }
                        }

                        drag {
                            axis: Drag.XAxis
                            target: parent
                        }
                    }
                }
                Rectangle { //Top
                    id: rectangleTop

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.top
                    color: "yellow"
                    height: rulersSize
                    radius: rulersSize
                    width: rulersSize
                    x: parent.x / 2
                    y: 0

                    MouseArea {
                        anchors.fill: parent

                        onMouseYChanged: {
                            if (drag.active) {
                                layerViewItem.dragTop(mouseY);
                            }
                        }

                        drag {
                            axis: Drag.YAxis
                            target: parent
                        }
                    }
                }
                Rectangle { //Bottom
                    id: rectangleBottom

                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.verticalCenter: parent.bottom
                    color: "blue"
                    height: rulersSize
                    radius: rulersSize
                    width: rulersSize
                    x: parent.x / 2
                    y: parent.y

                    MouseArea {
                        anchors.fill: parent

                        onMouseYChanged: {
                            if (drag.active) {
                                layerViewItem.dragBottom(mouseY);
                            }
                        }

                        drag {
                            axis: Drag.YAxis
                            target: parent
                        }
                    }
                }
            }
        }
        Component {
            id: audioComponent
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                topPadding: 20
                enabled: layerViewItem.layerHasAudio

                onEnabledChanged: {
                    if (enabled){
                        layerViewItem.loadTracks();
                    }
                }

                RowLayout {
                    Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        implicitHeight: 35
                        implicitWidth: 100
                        radius: 5
                        visible: !layerViewItem.layerHasAudio || layerViewItem.audioTracksModel.countTracks() > 0

                        ToolButton {
                            focusPolicy: Qt.NoFocus
                            icon.color: layerViewItem.layerHasAudio ? "lime" : "crimson"
                            icon.name: "new-audio-alarm"
                            text: qsTr("Audio File")

                            onClicked: {
                                if(layerViewItem.audioTracksModel.countTracks() > 0) {
                                    if (audioMenuInstantiator.model === 0) {
                                        audioMenuInstantiator.model = layerViewItem.audioTracksModel;
                                        layerViewItem.layerValueChanged();
                                    }
                                    audioMenu.visible = !audioMenu.visible;
                                }
                            }

                            ToolTip {
                                text: "Choose the audio track/file that was loaded with the media."
                            }
                            Menu {
                                id: audioMenu

                                y: parent.height

                                Instantiator {
                                    id: audioMenuInstantiator

                                    model: 0

                                    delegate: MenuItem {
                                        id: audioMenuItem

                                        checkable: true
                                        checked: model.id === layerViewItem.layerAudioId
                                        text: model.text

                                        onTriggered: layerViewItem.layerAudioId = model.id
                                    }

                                    onObjectAdded: audioMenu.insertItem(index, object)
                                    onObjectRemoved: audioMenu.removeItem(object)
                                }
                            }
                        }
                    }
                    Slider {
                        id: volumeSlider

                        from: 0
                        implicitHeight: 25
                        implicitWidth: 130
                        leftPadding: 0
                        rightPadding: 0
                        to: 100
                        wheelEnabled: false

                        onValueChanged: {
                            if (value.toFixed(0) !== layerViewItem.layerVolume) {
                                layerViewItem.layerVolume = value.toFixed(0);
                            }
                        }
                        Component.onCompleted: {
                            volumeSlider.value = layerViewItem.layerVolume;
                        }

                        background: Rectangle {
                            color: Kirigami.Theme.alternateBackgroundColor

                            Rectangle {
                                color: Kirigami.Theme.highlightColor
                                height: parent.height
                                radius: 0
                                width: volumeSlider.visualPosition * parent.width
                            }
                        }
                        handle: Item {
                            visible: false
                        }

                        Label {
                            id: progressBarToolTip

                            anchors.centerIn: volumeSlider
                            color: enabled ? "white" : "grey"
                            font.pointSize: 9
                            layer.enabled: true
                            text: qsTr("Volume: %1\%").arg(Number(volumeSlider.value.toFixed(0)))

                            layer.effect: DropShadow {
                                color: "#111"
                                radius: 5
                                samples: 17
                                spread: 0.3
                                verticalOffset: 1
                            }
                        }
                    }
                    Connections {
                        function onLayerChanged() {
                            if(layerViewItem.layerIdx !== -1) {
                                if(volumeSlider)
                                    volumeSlider.value = layerViewItem.layerVolume;
                            }
                        }
                        function onLayerValueChanged() {
                            if (volumeSlider !== undefined){
                                if (volumeSlider.value !== layerViewItem.layerVolume) {
                                    volumeSlider.value = layerViewItem.layerVolume;
                                }
                            }
                        }

                        target: layerViewItem
                    }
                }
            }
        }
        Component {
            id: mediaComponent

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                bottomPadding: 20

                RowLayout {
                    Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        implicitHeight: 35
                        implicitWidth: 35
                        radius: 5

                        ToolButton {
                            id: playPauseButton

                            property string iconName: "media-playback-start"
                            property string toolTipText: "Start Playback"

                            focusPolicy: Qt.NoFocus
                            icon.name: playPauseButton.iconName
                            text: ""

                            onClicked: {
                                if(layerViewItem.layerPause){
                                    layerViewItem.layerPause = false
                                }
                                else{
                                    layerViewItem.layerPause = true
                                    layerViewItem.layerPosition = mediaSlider.value
                                }
                                app.slides.needsSync = true;
                            }

                            ToolTip {
                                id: playPauseButtonToolTip
                                text: playPauseButton.toolTipText
                            }
                        }
                    }
                    Slider {
                        id: mediaSlider

                        property bool seekStarted: false

                        from: 0
                        implicitHeight: 25
                        implicitWidth: 200
                        leftPadding: 0
                        rightPadding: 0
                        to: layerViewItem.layerDuration

                        background: Rectangle {
                            id: progressBarBackground

                            color: Kirigami.Theme.alternateBackgroundColor

                            Rectangle {
                                color: Kirigami.Theme.highlightColor
                                height: parent.height
                                width: mediaSlider.visualPosition * parent.width
                            }
                            ToolTip {
                                id: progressBarToolTip

                                delay: 0
                                timeout: -1
                                visible: progressBarMouseArea.containsMouse
                            }
                            MouseArea {
                                id: progressBarMouseArea

                                acceptedButtons: Qt.MiddleButton | Qt.RightButton
                                anchors.fill: parent
                                hoverEnabled: true

                                onClicked: {}
                                onEntered: {
                                    progressBarToolTip.x = mouseX - (progressBarToolTip.width * 0.5);
                                    progressBarToolTip.y = mediaSlider.height;
                                }
                                onMouseXChanged: {
                                    progressBarToolTip.x = mouseX - (progressBarToolTip.width * 0.5);
                                    const time = mouseX * 100 / progressBarBackground.width * mediaSlider.to / 100;
                                    progressBarToolTip.text = app.formatTime(time);
                                }
                            }
                        }
                        handle: Item {
                            visible: false
                        }

                        onPressedChanged: {
                            if (pressed) {
                                mediaSlider.seekStarted = true;
                            } else {
                                layerViewItem.layerPause = true;
                                layerViewItem.layerPosition = value;
                                app.slides.needsSync = true;
                                mediaSlider.seekStarted = false;
                            }
                        }
                        onToChanged: value = layerViewItem.layerPosition

                        Connections {
                            function onLayerPositionChanged() {
                                if (!mediaSlider.seekStarted) {
                                    mediaSlider.value = layerViewItem.layerPosition;
                                }
                                if (layerViewItem.layerPause) {
                                    playPauseButton.iconName = "media-playback-start";
                                    playPauseButton.toolTipText = "Start Playback";
                                } else {
                                    playPauseButton.iconName = "media-playback-pause";
                                    playPauseButton.toolTipText = "Pause Playback";
                                }
                            }

                            target: layerViewItem
                        }
                    }
                    Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        implicitHeight: 35
                        implicitWidth: 35
                        radius: 5

                        ToolButton {
                            id: rewindButton

                            focusPolicy: Qt.NoFocus
                            icon.name: "media-playback-stop"
                            text: ""

                            onClicked: {
                                layerViewItem.layerPause = true;
                                layerViewItem.layerPosition = 0;
                                app.slides.needsSync = true;
                            }

                            ToolTip {
                                id: rewindButtonToolTip

                                text: PlaybackSettings.fadeDownBeforeRewind ? qsTr("Fade down then stop/rewind") : qsTr("Stop/rewind")
                            }
                        }
                    }
                    Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        implicitHeight: 35
                        implicitWidth: 120
                        radius: 5

                        Row{
                            anchors.fill: parent
                            height: parent.height
                            width: parent.width

                            LabelWithTooltip {
                                topPadding: 8.5

                                id: timeInfo

                                alwaysShowToolTip: true
                                font.pointSize: 9
                                fontSizeMode: Text.Fit
                                horizontalAlignment: Qt.AlignHCenter
                                text: app.formatTime(layerViewItem.layerPosition) + " / " + app.formatTime(layerViewItem.layerDuration)
                                toolTipFontSize: timeInfo.font.pointSize + 2
                                toolTipText: qsTr("Remaining: ") + app.formatTime(layerViewItem.layerRemaining)
                            }
                        }
                    }
                }
            }
        }
        Component {
            id: streamComponent

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                bottomPadding: 20

                RowLayout {
                    Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        implicitHeight: 35
                        implicitWidth: 35
                        radius: 5

                        ToolButton {
                            id: streamPlayPauseButton

                            focusPolicy: Qt.NoFocus
                            icon.name: layerViewItem.layerPause ? "media-playback-start" : "media-playback-pause"
                            text: ""

                            onClicked: {
                                if(layerViewItem.layerPause){
                                    layerViewItem.layerPause = false
                                }
                                else{
                                    layerViewItem.layerPause = true
                                }
                                app.slides.needsSync = true;
                            }

                            ToolTip {
                                id: streamPlayPauseButtonToolTip
                                text: layerViewItem.layerPause ? qsTr("Play Stream") : qsTr("Pause Stream");
                            }
                        }

                        Connections {
                            function onLayerPositionChanged() {
                                if (layerViewItem.layerPause) {
                                    streamPlayPauseButton.iconName = "media-playback-start";
                                    streamPlayPauseButtonToolTip.toolTipText = "Play Stream";
                                } else {
                                    streamPlayPauseButton.iconName = "media-playback-pause";
                                    streamPlayPauseButtonToolTip.toolTipText = "Pause Stream";
                                }
                            }

                            target: layerViewItem
                        }
                    }
                }
            }
        }
        Component {
            id: pdfPageComponent

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                bottomPadding: 20

                RowLayout {
                    Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        implicitHeight: 30
                        implicitWidth: 50
                        radius: 5

                        Label {
                            anchors.right: parent.right
                            topPadding: 5
                            font.pointSize: 10
                            text: "Page:"
                        }
                    }
                    SpinBox {
                        id: pageNum

                        from: 1
                        to: 200
                        value: 1

                        onValueChanged: {
                            if (pageNum.value !== layerViewItem.layerPage)
                                layerViewItem.layerPage = pageNum.value
                        }

                        Component.onCompleted: {
                            pageNum.value = layerViewItem.layerPage
                            pageNum.to = layerViewItem.layerNumPages
                        }
                    }
                    Rectangle {
                        color: Kirigami.Theme.alternateBackgroundColor
                        implicitHeight: 30
                        implicitWidth: 100
                        radius: 5

                        Label {
                            anchors.left: parent.left
                            topPadding: 5
                            id: labelNumPages
                            font.pointSize: 10
                            text: qsTr("of %1 pages.").arg(Number(layerViewItem.layerNumPages));
                        }
                    }
                }
            }
        }
        Connections {
            function onLayerChanged() {
                if(layerViewItem.layerIdx !== -1) {
                    layerViewItem.loadTracks();
                    layerWindow.title = layerViewItem.layerTitle;
                    if(visibilitySlider)
                        visibilitySlider.value = layerViewItem.layerVisibility;
                    for (let sm = 0; sm < stereoscopicModeForLayerList.count; ++sm) {
                        if (stereoscopicModeForLayerList.get(sm).value === layerViewItem.layerStereoMode) {
                            stereoscopicModeForLayer.currentIndex = sm;
                            break;
                        }
                    }
                    for (let gm = 0; gm < gridModeForLayerList.count; ++gm) {
                        if (gridModeForLayerList.get(gm).value === layerViewItem.layerGridMode) {
                            gridModeForLayer.currentIndex = gm;
                            break;
                        }
                    }
                    roiButton.checked = layerViewItem.layerRoiEnabled;
                    if (layerViewItem.layerRoiEnabled) {
                        createRoiComponents();
                    }
                    else {
                        destroyRoiComponents();
                    }
                    
                    if (layerViewItem.layerTypeName === "PDF") {
                        createPageComponents();
                    }
                    else {
                        destroyPageComponents();
                    }

                    if (layerViewItem.layerTypeName === "Video" 
                        || layerViewItem.layerTypeName === "Audio") {
                        createAudioComponents();
                        createMediaComponents();
                    }
                    else {
                        destroyAudioComponents();
                        destroyMediaComponents();
                    }

                    if (layerViewItem.layerTypeName === "Stream") {
                        createStreamComponents();
                    }
                    else {
                        destroyStreamComponents();
                    }

                    if (layerViewItem.layerTypeName === "NDI") {
                        createAudioComponents();
                    }
                    else {
                        destroyAudioComponents();
                    }
                }
                else {
                    destroyRoiComponents();
                    destroyPageComponents();
                    destroyAudioComponents();
                    destroyMediaComponents();
                    destroyStreamComponents();
                }
            }
            function onLayerValueChanged() {
                if (visibilitySlider){
                    if(visibilitySlider.value !== layerViewItem.layerVisibility){
                        visibilitySlider.value = layerViewItem.layerVisibility;
                    }
                }
            }

            target: layerViewItem
        }
    }
}
