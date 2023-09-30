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
import com.georgefb.haruna 1.0

Rectangle {
    id: sectionsRoot

    property alias scrollPositionTimer: scrollPositionTimer
    property alias sectionsView: sectionsView
    property alias mediaTitle: mediaTitle
    property bool canToggleWithMouse: PlaylistSettings.canToggleWithMouse
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

                Button {
                    text: qsTr("Add section")
                    icon.name: "list-add"
                    enabled: true
                    focus: true
                    onClicked: {
                        mpv.playSectionsModel.addSection(sectionTitle.text, startTimeTextField.text, endTimeTextField.text, eosComboBox.currentIndex)
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
            PropertyChanges { target: sectionsRoot; x: position !== "right" ? parent.width : -width }
            PropertyChanges { target: sectionsRoot; visible: false }
        },
        State {
            name : "visible"
            PropertyChanges { target: sectionsRoot; x: position !== "right" ? parent.width - sectionsRoot.width : 0 }
            PropertyChanges { target: sectionsRoot; visible: true }
        }
    ]

    transitions: [
        Transition {
            from: "visible"
            to: "hidden"

            SequentialAnimation {
                NumberAnimation {
                    target: sectionsRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.InQuad
                }
                PropertyAction {
                    target: sectionsRoot
                    property: "visible"
                    value: false
                }
            }
        },
        Transition {
            from: "hidden"
            to: "visible"

            SequentialAnimation {
                PropertyAction {
                    target: sectionsRoot
                    property: "visible"
                    value: true
                }
                NumberAnimation {
                    target: sectionsRoot
                    property: "x"
                    duration: 120
                    easing.type: Easing.OutQuad
                }
            }
        }
    ]

}
