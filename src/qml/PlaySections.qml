/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtGraphicalEffects 1.12

import org.kde.kirigami 2.11 as Kirigami
import org.ctoolbox.cplay 1.0

Rectangle {
    id: sectionsRoot

    property alias scrollPositionTimer: scrollPositionTimer
    property alias sectionsView: sectionsView
    property alias mediaTitle: mediaTitle
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property int bigFont: PlaylistSettings.bigFontFullscreen

    height: mpv.height
    width: {
            const w = Kirigami.Units.gridUnit * 19
            return (parent.width * 0.17) < w ? w : parent.width * 0.17
    }
    x: position !== "right" ? parent.width : -width
    y: 0
    z: position === "right" ? 41 : 40
    state: "hidden"
    color: Kirigami.Theme.backgroundColor

    ColumnLayout {
        id: sectionsHeader
        spacing: 10

        Frame {
            id: addSectionsFrame
            enabled: false

            Connections {
                target: mpv.playSectionsModel
                function onCurrentEditItemChanged() {
                    addSectionsFrame.enabled = !mpv.playSectionsModel.isEmpty()
                }
            }

            GridLayout {
                id: sectionBuilder
                columns: 3
                Layout.preferredWidth: parent.width
                rowSpacing: 1

                Label {
                    text: qsTr("Title:")
                    Layout.alignment: Qt.AlignRight
                    font.pointSize: 9
                }

                TextField {
                    id: sectionTitle
                    text: ""
                    placeholderText: "Section title"
                    maximumLength: 20
                    Layout.preferredWidth: font.pointSize * 20
                    font.pointSize: 9
                    Layout.columnSpan: 2
                }

                Label {
                    text: qsTr("Start:")
                    Layout.alignment: Qt.AlignRight
                    font.pointSize: 9
                }

                TextField {
                    id: startTimeTextField
                    text: "00:00:00"
                    maximumLength: 8
                    inputMask: "99:99:99"
                    validator: RegExpValidator { regExp: /^([0-1\s]?[0-9\s]|2[0-3\s]):([0-5\s][0-9\s]):([0-5\s][0-9\s])$ / }
                    Layout.preferredWidth: font.pointSize * 8
                    font.pointSize: 9
                }
                Button {
                    id: takeStartTimeFromCurrentTime
                    text: "Copy current time"
                    icon.name: "go-previous-skip"
                    icon.height: 16
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        startTimeTextField.text = app.formatTime(mpv.position)
                    }
                    Layout.alignment: Qt.AlignLeft
                    font.pointSize: 9
                }

                Label {
                    text: qsTr("End:")
                    Layout.alignment: Qt.AlignRight
                    font.pointSize: 9
                }

                TextField {
                    id: endTimeTextField
                    text: "00:00:00"
                    maximumLength: 8
                    inputMask: "99:99:99"
                    validator: RegExpValidator { regExp: /^([0-1\s]?[0-9\s]|2[0-3\s]):([0-5\s][0-9\s]):([0-5\s][0-9\s])$ / }
                    Layout.preferredWidth: font.pointSize * 8
                    font.pointSize: 9
                }
                Button {
                    id: takeEndTimeFromCurrentTime
                    text: "Copy current time"
                    icon.name: "go-previous-skip"
                    icon.height: 16
                    focusPolicy: Qt.NoFocus
                    onClicked: {
                        endTimeTextField.text = app.formatTime(mpv.position)
                    }
                    Layout.alignment: Qt.AlignLeft
                    font.pointSize: 9
                }

                Label {
                    text: qsTr("At end of section:")
                    Layout.alignment: Qt.AlignLeft
                    font.pointSize: 9
                    Layout.columnSpan: 2
                }
                ComboBox {
                    id: eosComboBox
                    textRole: "mode"
                    model: ListModel {
                        id: eosToSave
                        ListElement { mode: "Pause"; value: 0 }
                        ListElement { mode: "Fade out"; value: 1}
                        ListElement { mode: "Continue"; value: 2 }
                        ListElement { mode: "Next"; value: 3 }
                        ListElement { mode: "Loop"; value: 4 }
                    }

                    onActivated: {
                    }

                    Layout.fillWidth: true
                }

                RowLayout {
                    Button {
                        icon.name: "list-add"
                        enabled: true
                        focus: true
                        onClicked: {
                            mpv.playSectionsModel.addSection(sectionTitle.text, startTimeTextField.text, endTimeTextField.text, eosComboBox.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Add section")
                        }
                    }

                    Button {
                        icon.name: "list-remove"
                        onClicked: {
                           mpv.playSectionsModel.removeSection(sectionsView.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Remove selected sections")
                        }
                    }
                    Button {
                        icon.name: "edit-entry"
                        onClicked: {
                            sectionTitle.text = mpv.playSectionsModel.sectionTitle(sectionsView.currentIndex)
                            startTimeTextField.text = app.formatTime(mpv.playSectionsModel.sectionStartTime(sectionsView.currentIndex))
                            endTimeTextField.text = app.formatTime(mpv.playSectionsModel.sectionEndTime(sectionsView.currentIndex))
                            eosComboBox.currentIndex = mpv.playSectionsModel.sectionEOSMode(sectionsView.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Edit section entry")
                        }
                    }
                    Button {
                        icon.name: "kdenlive-zindex-up"
                        onClicked: {
                            mpv.playSectionsModel.moveSectionUp(sectionsView.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Move selected upwards")
                        }
                    }
                    Button {
                        icon.name: "kdenlive-zindex-down"
                        onClicked: {
                            mpv.playSectionsModel.moveSectionDown(sectionsView.currentIndex)
                        }
                        ToolTip {
                            text: qsTr("Move selected downwards")
                        }
                    }
                    Button {
                        icon.name: "edit-reset"
                        onClicked: {
                            mpv.loadSection(-1)
                            mpv.playSectionsModel.setPlayingSection(-1)
                            sectionsView.currentIndex = -1
                        }
                        ToolTip {
                            text: qsTr("Reset to full file")
                        }
                    }
                    Layout.columnSpan: 3
                }
            }
        }

        Item {
            // spacer item
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        RowLayout {
            Rectangle {
                width: Kirigami.Units.gridUnit + 10
                height: 1
                color: Kirigami.Theme.alternateBackgroundColor
            }

            Label {
                id: mediaTitle
                text: qsTr("Sections: ")
                font.pointSize: 9
            }
        }
    }

    Connections {
        target: mpv
        function onFileLoaded() {
            sectionsView.currentIndex = -1
        }
        function onRewind() {
            mpv.loadSection(-1)
            mpv.playSectionsModel.setPlayingSection(-1)
            sectionsView.currentIndex = -1
        }
    }

    ScrollView {
        id: sectionsScrollView

        z: 20
        anchors.fill: parent
        anchors.topMargin: sectionsHeader.height + 5
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ListView {
            id: sectionsView

            model: mpv.playSectionsModel
            spacing: 1
            delegate: playSectionsItemCompact
        }
    }

    Component {
        id: playSectionsItemCompact
        PlaySectionsItemCompact {}
    }

    Timer {
        id: scrollPositionTimer
        interval: 50; running: true; repeat: true

        onTriggered: {
            //setSectionsScrollPosition()
            scrollPositionTimer.stop()
        }
    }

    states: [
        State {
            name: "hidden"
            PropertyChanges { target: sectionsRoot; x: position === "right" ? parent.width : -width }
            PropertyChanges { target: sectionsRoot; visible: false }
        },
        State {
            name : "visible-without-partner"
            PropertyChanges { target: sectionsRoot; x: position === "right" ? parent.width - sectionsRoot.width : 0 }
            PropertyChanges { target: sectionsRoot; visible: true }
        },
        State {
            name : "visible-with-partner"
            PropertyChanges { target: sectionsRoot; x: position === "right" ? parent.width - sectionsRoot.width : sectionsRoot.width }
            PropertyChanges { target: sectionsRoot; visible: true }
        }
    ]

    transitions: [
        Transition {
            from: "visible-without-partner"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    target: root
                    property: "x"
                    duration: 120
                    easing.type: Easing.InQuad
                }
                PropertyAction {
                    target: root
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
                    target: root
                    property: "visible"
                    value: true
                }
                NumberAnimation {
                    target: root
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
                    target: root
                    property: "x"
                    duration: 120
                    easing.type: Easing.InQuad
                }
                PropertyAction {
                    target: root
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
                    target: root
                    property: "visible"
                    value: true
                }
                NumberAnimation {
                    target: root
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
                    target: root
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
                    target: root
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        }
    ]

}
