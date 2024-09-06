/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtGraphicalEffects 1.15
import Qt.labs.platform 1.0 as Platform
import QtQuick.Dialogs 1.3

import org.kde.kirigami 2.15 as Kirigami
import org.ctoolbox.cplay 1.0

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
        const w = Kirigami.Units.gridUnit * 17;
        return (parent.width * 0.15) < w ? w : parent.width * 0.15;
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
            mpv.focus = true;
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
                    MessageDialog {
                        id: clearSlidesDialog

                        standardButtons: StandardButton.Yes | StandardButton.No
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
                id: masterSlideButton

                anchors.right: slidesRoot.right
                checkable: true
                checked: slidesView.currentIndex === -1
                icon.name: "backgroundtool"
                text: qsTr("Master slide")

                onClicked: {
                    slidesView.currentIndex = -1;
                    if (layers.state === "hidden") {
                        actions.toggleLayersAction.trigger();
                    }
                }

                ToolTip {
                    text: qsTr("Master slide with perminent layers")
                }
            }
        }
    }
    ScrollView {
        id: slidesScrollView

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.fill: parent
        anchors.topMargin: slidesHeader.height + 5
        z: 20

        ListView {
            id: slidesView

            delegate: slidesItemCompact
            model: app.slides
            spacing: 1

            Component.onCompleted: {
                slidesView.currentIndex = -1;
            }
            onCurrentIndexChanged: {
                app.slides.selectedSlideIdx = slidesView.currentIndex;
                layers.layersView.currentIndex = -1;
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
}
