/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.platform 1.0 as Platform
import QtQuick.Dialogs

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Rectangle {
    id: slidesRoot

    property int bigFont: PlaylistSettings.bigFontFullscreen
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property alias scrollPositionTimer: scrollPositionTimer
    property alias slidesView: slidesView

    color: Kirigami.Theme.backgroundColor
    height: mpv.height
    state: "hidden"
    width: {
        const w = Kirigami.Units.gridUnit * 15;
        return (parent.width * 0.13) < w ? w : parent.width * 0.13;
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
                visible: false
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
                visible: true
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
                visible: true
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
                    property: "visible"
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
                    property: "visible"
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
                    property: "visible"
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
                    property: "visible"
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

    Platform.FileDialog {
        id: openCPlayPresentationDialog

        fileMode: Platform.FileDialog.OpenFile
        folder: LocationSettings.cPlayFileLocation !== "" ? app.pathToUrl(LocationSettings.cPlayFileLocation) : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        nameFilters: ["C-Play Presentation (*.cplaypres)"]
        title: "Open C-Play Presentation"

        onAccepted: {
            app.slides.loadFromJSONFile(openCPlayPresentationDialog.file.toString());
            slidesView.currentIndex = -1;
            layers.layersView.currentIndex = -1;
            mpv.focus = true;
            syncAfterLoad.start();
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
                        app.slides.removeSlide(slidesView.currentIndex);
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
                    icon.name: "folder-open"

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
                    MessageDialog {
                        id: clearSlidesDialog

                        buttons: MessageDialog.Yes | MessageDialog.No
                        text: "Confirm clearing of all items in slides list."
                        title: "Clear presentation/slides list"

                        Component.onCompleted: visible = false
                        onAccepted: {
                            app.slides.clearSlides();
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

            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit + 10
            }
            Label {
                font.pointSize: 9
                text: qsTr("Slides")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit + 10
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

            Component.onCompleted: {
                if (PresentationSettings.presentationToLoadOnStartup !== "") {
                    app.slides.loadFromJSONFile(PresentationSettings.presentationToLoadOnStartup);
                    syncAfterLoad.start();
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
                function onSelectedSlideChanged() {
                    slidesView.currentIndex = app.slides.selectedSlideIdx;
                }

                function onCopyCleared() {
                    app.action("layerPaste").enabled = false;
                    app.action("layerPasteProperties").enabled = false;
                }

                function onPreviousSlide() {
                    layers.layersView.currentIndex = -1;
                    if(app.slides.selectedSlideIdx === app.slides.triggeredSlideIdx){
                        app.slides.selectedSlideIdx = app.slides.selectedSlideIdx - 1;
                        app.slides.triggeredSlideIdx = app.slides.triggeredSlideIdx - 1;
                    }
                    else {
                        app.slides.triggeredSlideIdx = app.slides.selectedSlideIdx;
                    }   
                }

                function onNextSlide() {
                    layers.layersView.currentIndex = -1;
                    if(app.slides.selectedSlideIdx === app.slides.triggeredSlideIdx){
                        app.slides.selectedSlideIdx = app.slides.selectedSlideIdx + 1;
                        app.slides.triggeredSlideIdx = app.slides.triggeredSlideIdx + 1;
                    }
                    else {
                        app.slides.triggeredSlideIdx = app.slides.selectedSlideIdx;
                    }
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
        id: syncAfterLoad

        interval: 1000

        onTriggered: {
            app.slides.needsSync = true;
        }
    }
}