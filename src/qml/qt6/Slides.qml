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

    property alias scrollPositionTimer: scrollPositionTimer
    property alias slidesView: slidesView
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property int bigFont: PlaylistSettings.bigFontFullscreen

    height: mpv.height
    width: {
            const w = Kirigami.Units.gridUnit * 15
            return (parent.width * 0.13) < w ? w : parent.width * 0.13
    }
    x: position === "left" ? parent.width : -width
    y: 0
    z: position === "left" ? 40 : 41
    state: "hidden"
    color: Kirigami.Theme.backgroundColor

    Platform.FileDialog {
        id: openCPlayPresentationDialog

        folder: LocationSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayFileLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Open C-Play Presentation"
        fileMode: Platform.FileDialog.OpenFile
        nameFilters: [ "C-Play Presentation (*.cplaypres)" ]

        onAccepted: {
            app.slides.loadFromJSONFile(openCPlayPresentationDialog.file.toString())
            mpv.focus = true
        }
        onRejected: mpv.focus = true
    }

    Platform.FileDialog {
        id: saveCPlayPresentationDialog

        folder: LocationSettings.cPlayFileLocation !== ""
                ? app.pathToUrl(LocationSettings.cPlayFileLocation)
                : app.pathToUrl(LocationSettings.fileDialogLastLocation)
        title: "Save C-Play Presentation"
        fileMode: Platform.FileDialog.SaveFile
        nameFilters: [ "C-Play Presentation (*.cplaypres)" ]

        onAccepted: {
            app.slides.saveAsJSONFile(saveCPlayPresentationDialog.file.toString())
            mpv.focus = true
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
                spacing: 1
                anchors.rightMargin: Kirigami.Units.largeSpacing
                Layout.preferredWidth: parent.width

                Button {
                    icon.name: "list-add"
                    onClicked: {
                        app.slides.addSlide()
                    }
                    ToolTip {
                        text: qsTr("Add slider to bottom of list")
                    }
                }

                Button {
                    icon.name: "list-remove"
                    onClicked: {
                       app.slides.removeSlide(slidesView.currentIndex)
                    }
                    ToolTip {
                        text: qsTr("Remove selected slider")
                    }
                }
                Button {
                    icon.name: "pan-up-symbolic"
                    onClicked: {
                        app.slides.moveSlideUp(slidesView.currentIndex)
                    }
                    ToolTip {
                        text: qsTr("Move selected slider upwards")
                    }
                }
                Button {
                    icon.name: "pan-down-symbolic"
                    onClicked: {
                        app.slides.moveSlideDown(slidesView.currentIndex)
                    }
                    ToolTip {
                        text: qsTr("Move selected slider downwards")
                    }
                }
                Button {
                    icon.name: "folder-open"
                    onClicked: {
                        openCPlayPresentationDialog.open()
                    }
                    ToolTip {
                        text: qsTr("Open presentation")
                    }
                }
                Button {
                    icon.name: "system-save-session"
                    icon.color: app.slides.slidesNeedsSave ? "orange" : "lime"
                    onClicked: {
                        saveCPlayPresentationDialog.currentFile = app.slides.getSlidesPathAsURL()
                        saveCPlayPresentationDialog.open()
                    }
                    ToolTip {
                        text: qsTr("Save presentation")
                    }
                }
                Button {
                    icon.name: "trash-empty"
                    icon.color: "crimson"
                    onClicked: {
                        clearSlidesDialog.open()
                    }
                    ToolTip {
                        text: qsTr("Clear slides list")
                    }

                    MessageDialog {
                        id: clearSlidesDialog
                        title: "Clear presentation/slides list"
                        text: "Confirm clearing of all items in slides list."
                        buttons: MessageDialog.Yes | MessageDialog.No
                        onAccepted: {
                            app.slides.clearSlides();
                        }
                        Component.onCompleted: visible = false
                    }
                }
            }
        }

        Item {
            // spacer item
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 5
            Layout.margins: 2

            Label {
                id: sliderName
                text: app.slides.slidesName
                font.pointSize: 7
                font.italic: true
                color: Kirigami.Theme.disabledTextColor
            }
        }

        RowLayout {
            Rectangle {
                width: Kirigami.Units.gridUnit + 10
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
            }

            Label {
                text: qsTr("Slides")
                font.pointSize: 9
            }

            Rectangle {
                width: Kirigami.Units.gridUnit + 10
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
                Layout.fillWidth: true
            }

            Button {
                id: masterSlideButton
                anchors.right: slidesRoot.right
                icon.name: "backgroundtool"
                text: qsTr("Master slide")
                checkable: true
                checked: slidesView.currentIndex === -1
                onClicked: {
                    slidesView.currentIndex = -1
                    if(layers.state === "hidden"){
                        actions.toggleLayersAction.trigger()
                    }
                }
                ToolTip {
                    text: qsTr("Master slide with perminent layers")
                }
            }
            Layout.preferredWidth: slidesRoot.width
        }
    }

    ScrollView {
        id: slidesScrollView

        z: 20
        anchors.fill: parent
        anchors.topMargin: slidesHeader.height + 5
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ListView {
            id: slidesView

            model: app.slides
            spacing: 1
            delegate: slidesItemCompact

            onCurrentIndexChanged: {
                app.slides.selectedSlideIdx = slidesView.currentIndex
                layers.layersView.currentIndex = -1
            }
        }
    }

    Component {
        id: slidesItemCompact
        SlidesItemCompact {}
    }

    Timer {
        id: scrollPositionTimer
        interval: 50; running: true; repeat: true

        onTriggered: {
            scrollPositionTimer.stop()
        }
    }

    states: [
        State {
            name: "hidden"
            PropertyChanges { target: slidesRoot; x: position === "left" ? parent.width : -width }
            PropertyChanges { target: slidesRoot; visible: false }
        },
        State {
            name : "visible-without-partner"
            PropertyChanges { target: slidesRoot; x: position === "left" ? parent.width - slidesRoot.width : 0 }
            PropertyChanges { target: slidesRoot; visible: true }
        },
        State {
            name : "visible-with-partner"
            PropertyChanges { target: slidesRoot; x: position === "left" ? parent.width - (slidesRoot.width * 2) : 0 }
            PropertyChanges { target: slidesRoot; visible: true }
        }
    ]

    transitions: [
        Transition {
            from: "visible-without-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    target: slidesRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.InQuad
                }
                PropertyAction {
                    target: slidesRoot
                    property: "visible"
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-without-partner"

            SequentialAnimation {
                PropertyAction {
                    target: slidesRoot
                    property: "visible"
                    value: true
                }
                NumberAnimation {
                    target: slidesRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    target: slidesRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.InQuad
                }
                PropertyAction {
                    target: slidesRoot
                    property: "visible"
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible-with-partner"

            SequentialAnimation {
                PropertyAction {
                    target: slidesRoot
                    property: "visible"
                    value: true
                }
                NumberAnimation {
                    target: slidesRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        },
        Transition {
            from: "visible-without-partner"
            to: "visible-with-partner"

            SequentialAnimation {
                NumberAnimation {
                    target: slidesRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        },
        Transition {
            from: "visible-with-partner"
            to: "visible-without-partner"

            SequentialAnimation {
                NumberAnimation {
                    target: slidesRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        }
    ]

}
