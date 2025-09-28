/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform as Platform

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Rectangle {
    id: slidesRoot

    property int bigFont: PlaylistSettings.bigFontFullscreen
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property alias scrollPositionTimer: scrollPositionTimer
    property alias slidesView: slidesView
    property bool busyIndicator: false
    property bool shouldBeVisible: true
    property string presentationToLoad: ""

    visible: shouldBeVisible && !window.hideUI
    color: Kirigami.Theme.backgroundColor
    height: mpv.height
    state: "hidden"
    width: {
        const w = Kirigami.Units.gridUnit * 13.25;
        return (parent.width * 0.1125) < w ? w : parent.width * 0.1125;
    }
    x: position === "left" ? parent.width : -width
    y: 0
    z: position === "left" ? 40 : 41

    states: [
        State {
            name: "hidden"

            PropertyChanges {
                target: slidesRoot
                x: position === "left" ? parent.width : -width
            }
            PropertyChanges {
                target: slidesRoot
                shouldBeVisible: false
            }
        },
        State {
            name: "visible-without-partner"

            PropertyChanges {
                target: slidesRoot
                x: position === "left" ? parent.width - slidesRoot.width : 0
            }
            PropertyChanges {
                target: slidesRoot
                shouldBeVisible: true
            }
        },
        State {
            name: "visible-with-partner"

            PropertyChanges {
                target: slidesRoot
                x: position === "left" ? parent.width - (slidesRoot.width * 2) : 0
            }
            PropertyChanges {
                target: slidesRoot
                shouldBeVisible: true
            }
        }
    ]
    transitions: [
        Transition {
            from: "visible-without-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.InQuad
                    property: "x"
                    target: slidesRoot
                }
                PropertyAction {
                    property: "shouldBeVisible"
                    target: slidesRoot
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-without-partner"

            SequentialAnimation {
                PropertyAction {
                    property: "shouldBeVisible"
                    target: slidesRoot
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: slidesRoot
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.InQuad
                    property: "x"
                    target: slidesRoot
                }
                PropertyAction {
                    property: "shouldBeVisible"
                    target: slidesRoot
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-with-partner"

            SequentialAnimation {
                PropertyAction {
                    property: "shouldBeVisible"
                    target: slidesRoot
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: slidesRoot
                }
            }
        },
        Transition {
            from: "visible-without-partner"
            to: "visible-with-partner"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: slidesRoot
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "visible-without-partner"

            SequentialAnimation {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: slidesRoot
                }
            }
        }
    ]
    
    function clearAndLoadPresentation() {
        clearAndLoadPresentationTimer.start();
    }

    function clearSlides() {
        clearSlidesTimer.start();
    }

    function removeSlide() {
        removeSlideTimer.start();
    }

    function openCPlayPresentation() {
        busyIndicator = true;
        app.slides.pauseLayerUpdate = true;
        mpv.focus = true
        Qt.callLater(clearAndLoadPresentation);
    }

    Platform.FileDialog {
        id: openCPlayPresentationDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play Presentation (*.cplaypres)"]
        title: "Open C-Play Presentation"

        onAccepted: {
            presentationToLoad = openCPlayPresentationDialog.file.toString();
            openCPlayPresentation();
        }
        onRejected: mpv.focus = true
    }
    Platform.FileDialog {
        id: saveCPlayPresentationDialog

        fileMode: Platform.FileDialog.SaveFile
        folder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play Presentation (*.cplaypres)"]
        title: "Save C-Play Presentation"

        onAccepted: {
            app.slides.saveAsJSONFile(saveCPlayPresentationDialog.file.toString());
            mpv.focus = true;
        }
        onRejected: mpv.focus = true
    }
    ColumnLayout {
        id: slidesHeader
        enabled: !busyIndicator

        spacing: 10

        ColumnLayout {
            id: slidesMenu

            spacing: 1

            RowLayout {
                Layout.preferredWidth: parent.width
                anchors.rightMargin: Kirigami.Units.largeSpacing
                spacing: 1

                Button {
                    icon.name: "list-add"

                    onClicked: {
                        app.slides.addSlide();
                    }

                    ToolTip {
                        text: qsTr("Add slider to bottom of list")
                    }
                }
                Button {
                    icon.name: "list-remove"

                    onClicked: {
                        busyIndicator = true;
                        app.slides.pauseLayerUpdate = true;
                        Qt.callLater(removeSlide);
                    }

                    ToolTip {
                        text: qsTr("Remove selected slider")
                    }
                }
                Button {
                    icon.name: "pan-up-symbolic"

                    onClicked: {
                        app.slides.moveSlideUp(slidesView.currentIndex);
                    }

                    ToolTip {
                        text: qsTr("Move selected slider upwards")
                    }
                }
                Button {
                    icon.name: "pan-down-symbolic"

                    onClicked: {
                        app.slides.moveSlideDown(slidesView.currentIndex);
                    }

                    ToolTip {
                        text: qsTr("Move selected slider downwards")
                    }
                }
                Button {
                    icon.name: "document-open"

                    onClicked: {
                        openCPlayPresentationDialog.open();
                    }

                    ToolTip {
                        text: qsTr("Open presentation")
                    }
                }
                Button {
                    icon.color: app.slides.slidesNeedsSave ? "orange" : "lime"
                    icon.name: "system-save-session"

                    onClicked: {
                        saveCPlayPresentationDialog.currentFile = app.slides.getSlidesPathAsURL();
                        saveCPlayPresentationDialog.open();
                    }

                    ToolTip {
                        text: qsTr("Save presentation")
                    }
                }
                Button {
                    icon.color: "crimson"
                    icon.name: "trash-empty"

                    onClicked: {
                        clearSlidesDialog.open();
                    }

                    ToolTip {
                        text: qsTr("Clear slides list")
                    }
                    Dialog {
                        id: clearSlidesDialog
                        standardButtons: Dialog.Ok | Dialog.Cancel

                        Label {
                            text: "Confirm clearing of all items in slides list."
                        }

                        onAccepted: {
                            app.slides.pauseLayerUpdate = true;
                            busyIndicator = true;
                            Qt.callLater(clearSlides);
                        }
                    }
                }
            }
        }
        Item {
            Layout.fillHeight: true
            // spacer item
            Layout.fillWidth: true
            Layout.leftMargin: 5
            Layout.margins: 2

            Label {
                id: sliderName

                color: Kirigami.Theme.disabledTextColor
                font.italic: true
                font.pointSize: 7
                text: app.slides.slidesName
            }
        }
        RowLayout {
            Layout.preferredWidth: slidesRoot.width
            spacing: 1

            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: 10
            }
            Label {
                font.pointSize: 9
                text: qsTr("Slides")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit
            }
            Button {
                id: preLoadLayersButton

                checkable: true
                checked: app.slides.preLoadLayers
                icon.color: app.slides.preLoadLayers ? "lime" : "crimson"
                icon.name: app.slides.preLoadLayers ? "task-complete" : "address-book-new"

                onCheckedChanged: {
                    app.slides.preLoadLayers = preLoadLayersButton.checked;
                }

                ToolTip {
                    id: preLoadLayersTooltip

                    text: {
                        app.slides.preLoadLayers ? qsTr("Preload Layers is ON") : qsTr("Preload Layers is OFF");
                    }
                }
            }
            Button {
                id: visibilityViewButton

                checkable: true
                checked: slidesVisView.visible
                icon.name: "table"
                text: qsTr("Visibility")

                onClicked: {
                    slidesVisView.visible = checked;
                }

                ToolTip {
                    text: qsTr("Slide Visibility Table View")
                }
            }
            Button {
                id: masterSlideButton

                checkable: true
                checked: slidesView.currentIndex === -1
                icon.name: "backgroundtool"
                text: qsTr("Master")

                onClicked: {
                    slidesView.currentIndex = -1;
                    app.slides.slideToPaste = -1;
                    app.slides.selectedSlideIdx = -1;
                    if (layers.state === "hidden") {
                        actions.toggleLayersAction.trigger();
                    }
                }

                ToolTip {
                    text: qsTr("Master slide with perminent background layers")
                }
            }
        }
    }

    WorkingIndicator {
        id: layerLoadingIndicator
        running: slides.busyIndicator
        visible: slides.busyIndicator
        Layout.preferredWidth: parent.width
        Layout.preferredHeight: parent.height
        anchors.fill: parent
        z: 30
    }

    ScrollView {
        id: slidesScrollView

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.fill: parent
        anchors.topMargin: slidesHeader.height + 5
        clip: true
        z: 20

        ListView {
            id: slidesView

            delegate: slidesItemCompact
            model: app.slides
            spacing: 1
            enabled: !busyIndicator

            Component.onCompleted: {
                if (PresentationSettings.presentationToLoadOnStartup !== "") {
                    busyIndicator = true;
                    app.slides.pauseLayerUpdate = true;
                    app.slides.loadFromJSONFile(PresentationSettings.presentationToLoadOnStartup);
                }

                slidesView.currentIndex = -1;
                layers.layersView.currentIndex = -1;
            }
            onCurrentIndexChanged: {
                if(app.slides.selectedSlideIdx !== slidesView.currentIndex)
                    app.slides.selectedSlideIdx = slidesView.currentIndex;

                layers.layersView.currentIndex = -1;
            }

            Connections {
                function onVisibilityChanged() {
                    app.slides.checkMasterLayersRunBasedOnMediaVisibility(mpv.visibility);
                }

                function onVolumeChanged() {
                    if(PresentationSettings.masterVolumeControlLayersVolume)
                        app.slides.runUpdateVolumeOnLayers(mpv.volume);
                } 

                function onAudioOutputChanged() {
                    app.slides.runUpdateAudioOutputOnLayers();
                }

                target: mpv
            }

            Connections {
                function onPresentationHasLoaded() {
                    if (slidesView.count > 0 && slides.state === "hidden") {
                        actions.toggleSlidesAction.trigger();
                    }
                    if (layers.layersView.count > 0 && layers.state === "hidden") {
                        actions.toggleLayersAction.trigger();
                    }
                    app.slides.pauseLayerUpdate = false;
                    syncAfterLoad.start();
                    startAfterLoad.start();
                }

                function onSelectedSlideChanged() {
                    layers.layersView.currentIndex = -1;
                    slidesView.currentIndex = app.slides.selectedSlideIdx;
                }

                function onCopyCleared() {
                    app.action("layerPaste").enabled = false;
                    app.action("layerPasteProperties").enabled = false;
                }

                function onPreviousSlide() {
                    actions.slidePreviousAction.trigger();
                }

                function onNextSlide() {
                    actions.slideNextAction.trigger();
                }

                target: app.slides
            }
        }
    }
    Component {
        id: slidesItemCompact

        SlidesItemCompact {
        }
    }
    Timer {
        id: scrollPositionTimer

        interval: 50
        repeat: true
        running: true

        onTriggered: {
            scrollPositionTimer.stop();
        }
    }
    Timer {
        id: loadPresentation

        interval: PresentationSettings.clearAndLoadDelay

        onTriggered: {
            app.slides.loadFromJSONFile(presentationToLoad);
            slidesView.currentIndex = -1;
            layers.layersView.currentIndex = -1;
        }
    }
    Timer {
        id: syncAfterLoad

        interval: PresentationSettings.syncAfterLoadDelay

        onTriggered: {
            app.slides.needsSync = true;
        }
    }
    Timer {
        id: startAfterLoad

        interval: PresentationSettings.startAfterLoadDelay

        onTriggered: {
            busyIndicator = false;
            app.slides.runStartAfterPresentationLoad();
        }
    }

    Timer {
        id: clearAndLoadPresentationTimer

        interval: 500

        onTriggered: {
            app.slides.clearSlides();
            loadPresentation.start();
        }
    }
    Timer {
        id: clearSlidesTimer

        interval: 500

        onTriggered: {
            app.slides.clearSlides();
            app.slides.pauseLayerUpdate = false;
            busyIndicator = false;
            masterSlideButton.clicked();
        }
    }
    Timer {
        id: removeSlideTimer

        interval: 500

        onTriggered: {
            app.slides.removeSlide(slidesView.currentIndex);
            app.slides.selectedSlideIdx = slidesView.currentIndex;
            app.slides.pauseLayerUpdate = false;
            busyIndicator = false;
            if(slidesView.count === 0) {
                masterSlideButton.clicked();
            }
        }
    }
}
