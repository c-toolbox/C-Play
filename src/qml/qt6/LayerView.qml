/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Window
import QtQuick.Layouts
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Shapes
import Qt.labs.platform 1.0 as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Kirigami.ApplicationWindow {
    id: layerWindow

    property var layerItem: layerViewItem
    property var roiEdit: undefined
    property var selection: undefined
    property var pdfPage: undefined
    property var videoControls: undefined

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

    function createVideoComponents() {
        if (!videoControls) {
            videoControls = videoComponent.createObject(layerViewItem);
        }
    }
    function destroyVideoComponents() {
        if (videoControls) {
            videoControls.destroy();
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
        destroyVideoComponents();
    }
    onVisibilityChanged: {
        if (visibility) {
            if (layerViewItem.layerRoiEnabled) {
                createRoiComponents();
            }
            if (layerViewItem.layerTypeName === "PDF") {
                createPageComponents();
            }
            else if (layerViewItem.layerTypeName === "Video") {
                createVideoComponents();
            }
        }
    }

    ToolBar {
        id: toolBarEmptyLeft

        anchors.left: parent.left
        anchors.right: toolBarLayerView.left
        visible: layers.layersView.currentIndex !== -1
    }
    ToolBar {
        id: toolBarLayerView

        anchors.horizontalCenter: parent.horizontalCenter
        visible: layers.layersView.currentIndex !== -1

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
                }
                ComboBox {
                    id: stereoscopicModeForLayer

                    Layout.fillWidth: true
                    enabled: true
                    focusPolicy: Qt.NoFocus
                    textRole: "mode"

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
                }
                ComboBox {
                    id: gridModeForLayer

                    Layout.fillWidth: true
                    enabled: true
                    focusPolicy: Qt.NoFocus
                    textRole: "mode"

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
            Connections {
                function onLayerChanged() {
                    layerWindow.title = layerViewItem.layerTitle;
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
                    } else {
                        destroyRoiComponents();
                    }
                    
                    if (layerViewItem.layerTypeName === "PDF") {
                        createPageComponents();
                    } else {
                        destroyPageComponents();
                    }
                }
                function onLayerValueChanged() {
                    if (visibilitySlider.value !== layerViewItem.layerVisibility)
                        visibilitySlider.value = layerViewItem.layerVisibility;
                }

                target: layerViewItem
            }
        }
    }
    ToolBar {
        id: toolBarEmptyRight

        anchors.left: toolBarLayerView.right
        anchors.right: parent.right
        visible: layers.layersView.currentIndex !== -1
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
            visible: layers.layersView.currentIndex === -1
            wrapMode: Text.WordWrap
        }
        MouseArea {
            anchors.fill: parent
            visible: layers.layersView.currentIndex !== -1

            onClicked: {}
        }
        Component {
            id: roiEditComponent

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                topPadding: 20
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
            id: videoComponent

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                bottomPadding: 20

                RowLayout {
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
                                layerViewItem.layerPosition = videoSlider.value
                            }
                            app.slides.needsSync = true;
                        }

                        ToolTip {
                            id: playPauseButtonToolTip
                            text: playPauseButton.toolTipText
                        }
                    }
                    Slider {
                        id: videoSlider

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
                                width: videoSlider.visualPosition * parent.width
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
                                    progressBarToolTip.y = videoSlider.height;
                                }
                                onMouseXChanged: {
                                    progressBarToolTip.x = mouseX - (progressBarToolTip.width * 0.5);
                                    const time = mouseX * 100 / progressBarBackground.width * videoSlider.to / 100;
                                    progressBarToolTip.text = app.formatTime(time);
                                }
                            }
                        }
                        handle: Item {
                            visible: false
                        }

                        onPressedChanged: {
                            if (pressed) {
                                videoSlider.seekStarted = true;
                            } else {
                                layerViewItem.layerPause = true;
                                layerViewItem.layerPosition = value;
                                app.slides.needsSync = true;
                                videoSlider.seekStarted = false;
                            }
                        }
                        onToChanged: value = layerViewItem.layerPosition

                        Connections {
                            function onLayerPositionChanged() {
                                if (!videoSlider.seekStarted) {
                                    videoSlider.value = layerViewItem.layerPosition;
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
                    LabelWithTooltip {
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
        Component {
            id: pdfPageComponent

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                bottomPadding: 20

                RowLayout {
                    Label {
                        font.pointSize: 10
                        text: "Page:"
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
                    Label {
                        id: labelNumPages
                        font.pointSize: 10
                        text: qsTr("of %1 pages.").arg(Number(layerViewItem.layerNumPages));
                    }
                }
            }
        }
    }
}
