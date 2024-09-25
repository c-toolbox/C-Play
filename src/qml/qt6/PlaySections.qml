/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

Rectangle {
    id: sectionsRoot

    property int bigFont: PlaylistSettings.bigFontFullscreen
    property alias mediaTitle: mediaTitle
    property string position: PlaylistSettings.position
    property int rowHeight: PlaylistSettings.rowHeight
    property alias scrollPositionTimer: scrollPositionTimer
    property alias sectionsView: sectionsView

    color: Kirigami.Theme.backgroundColor
    height: mpv.height
    state: "hidden"
    width: {
        const w = Kirigami.Units.gridUnit * 17;
        return (parent.width * 0.17) < w ? w : parent.width * 0.17;
    }
    x: position !== "right" ? parent.width : -width
    y: 0
    z: position === "right" ? 41 : 40

    states: [
        State {
            name: "hidden"

            PropertyChanges {
                target: sectionsRoot
                x: position === "right" ? parent.width : -width
            }
            PropertyChanges {
                target: sectionsRoot
                visible: false
            }
        },
        State {
            name: "visible-without-partner"

            PropertyChanges {
                target: sectionsRoot
                x: position === "right" ? parent.width - sectionsRoot.width : 0
            }
            PropertyChanges {
                target: sectionsRoot
                visible: true
            }
        },
        State {
            name: "visible-with-partner"

            PropertyChanges {
                target: sectionsRoot
                x: position === "right" ? parent.width - sectionsRoot.width : sectionsRoot.width
            }
            PropertyChanges {
                target: sectionsRoot
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
                    target: sectionsRoot
                }
                PropertyAction {
                    property: "visible"
                    target: sectionsRoot
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
                    target: sectionsRoot
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: sectionsRoot
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
                    target: sectionsRoot
                }
                PropertyAction {
                    property: "visible"
                    target: sectionsRoot
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
                    target: sectionsRoot
                    value: true
                }
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutQuad
                    property: "x"
                    target: sectionsRoot
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
                    target: sectionsRoot
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
                    target: sectionsRoot
                }
            }
        }
    ]

    ColumnLayout {
        id: sectionsHeader

        spacing: 10

        Frame {
            id: addSectionsFrame

            enabled: false

            Connections {
                function onCurrentEditItemChanged() {
                    addSectionsFrame.enabled = !mpv.playSectionsModel.isEmpty();
                }

                target: mpv.playSectionsModel
            }
            GridLayout {
                id: sectionBuilder

                Layout.preferredWidth: parent.width
                columns: 3
                rowSpacing: 1

                Label {
                    Layout.alignment: Qt.AlignRight
                    font.pointSize: 9
                    text: qsTr("Title:")
                }
                TextField {
                    id: sectionTitle

                    Layout.columnSpan: 2
                    Layout.preferredWidth: font.pointSize * 20
                    font.pointSize: 9
                    maximumLength: 20
                    placeholderText: "Section title"
                    text: ""
                }
                Label {
                    Layout.alignment: Qt.AlignRight
                    font.pointSize: 9
                    text: qsTr("Start:")
                }
                TextField {
                    id: startTimeTextField

                    Layout.preferredWidth: font.pointSize * 8
                    font.pointSize: 9
                    inputMask: "99:99:99"
                    maximumLength: 8
                    text: "00:00:00"

                    validator: RegularExpressionValidator {
                        regularExpression: /^([0-1\s]?[0-9\s]|2[0-3\s]):([0-5\s][0-9\s]):([0-5\s][0-9\s])$ /
                    }
                }
                Button {
                    id: takeStartTimeFromCurrentTime

                    Layout.alignment: Qt.AlignLeft
                    focusPolicy: Qt.NoFocus
                    font.pointSize: 9
                    icon.height: 16
                    icon.name: "go-previous-skip"
                    text: "Copy current time"

                    onClicked: {
                        startTimeTextField.text = app.formatTime(mpv.position);
                    }
                }
                Label {
                    Layout.alignment: Qt.AlignRight
                    font.pointSize: 9
                    text: qsTr("End:")
                }
                TextField {
                    id: endTimeTextField

                    Layout.preferredWidth: font.pointSize * 8
                    font.pointSize: 9
                    inputMask: "99:99:99"
                    maximumLength: 8
                    text: "00:00:00"

                    validator: RegularExpressionValidator {
                        regularExpression: /^([0-1\s]?[0-9\s]|2[0-3\s]):([0-5\s][0-9\s]):([0-5\s][0-9\s])$ /
                    }
                }
                Button {
                    id: takeEndTimeFromCurrentTime

                    Layout.alignment: Qt.AlignLeft
                    focusPolicy: Qt.NoFocus
                    font.pointSize: 9
                    icon.height: 16
                    icon.name: "go-previous-skip"
                    text: "Copy current time"

                    onClicked: {
                        endTimeTextField.text = app.formatTime(mpv.position);
                    }
                }
                Label {
                    Layout.alignment: Qt.AlignLeft
                    Layout.columnSpan: 2
                    font.pointSize: 9
                    text: qsTr("At end of section:")
                }
                ComboBox {
                    id: eosComboBox

                    Layout.fillWidth: true
                    textRole: "mode"

                    model: ListModel {
                        id: eosToSave

                        ListElement {
                            mode: "Pause"
                            value: 0
                        }
                        ListElement {
                            mode: "Fade out"
                            value: 1
                        }
                        ListElement {
                            mode: "Continue"
                            value: 2
                        }
                        ListElement {
                            mode: "Next"
                            value: 3
                        }
                        ListElement {
                            mode: "Loop"
                            value: 4
                        }
                    }

                    onActivated: {}
                }
                RowLayout {
                    Layout.columnSpan: 3

                    Button {
                        enabled: true
                        focus: true
                        icon.name: "list-add"

                        onClicked: {
                            mpv.playSectionsModel.addSection(sectionTitle.text, startTimeTextField.text, endTimeTextField.text, eosComboBox.currentIndex);
                        }

                        ToolTip {
                            text: qsTr("Add section to bottom of list")
                        }
                    }
                    Button {
                        icon.name: "list-remove"

                        onClicked: {
                            mpv.playSectionsModel.removeSection(sectionsView.currentIndex);
                        }

                        ToolTip {
                            text: qsTr("Remove selected section")
                        }
                    }
                    Button {
                        icon.name: "document-replace"

                        onClicked: {
                            mpv.playSectionsModel.replaceSection(sectionsView.currentIndex, sectionTitle.text, startTimeTextField.text, endTimeTextField.text, eosComboBox.currentIndex);
                        }

                        ToolTip {
                            text: qsTr("Replace selected section")
                        }
                    }
                    Button {
                        icon.name: "edit-entry"

                        onClicked: {
                            sectionTitle.text = mpv.playSectionsModel.sectionTitle(sectionsView.currentIndex);
                            startTimeTextField.text = app.formatTime(mpv.playSectionsModel.sectionStartTime(sectionsView.currentIndex));
                            endTimeTextField.text = app.formatTime(mpv.playSectionsModel.sectionEndTime(sectionsView.currentIndex));
                            eosComboBox.currentIndex = mpv.playSectionsModel.sectionEOSMode(sectionsView.currentIndex);
                        }

                        ToolTip {
                            text: qsTr("Copy values from selected section")
                        }
                    }
                    Button {
                        icon.name: "pan-up-symbolic"

                        onClicked: {
                            mpv.playSectionsModel.moveSectionUp(sectionsView.currentIndex);
                        }

                        ToolTip {
                            text: qsTr("Move selected section upwards")
                        }
                    }
                    Button {
                        icon.name: "pan-down-symbolic"

                        onClicked: {
                            mpv.playSectionsModel.moveSectionDown(sectionsView.currentIndex);
                        }

                        ToolTip {
                            text: qsTr("Move selected section downwards")
                        }
                    }
                    Button {
                        icon.name: "edit-reset"

                        onClicked: {
                            mpv.loadSection(-1);
                            sectionsView.currentIndex = -1;
                        }

                        ToolTip {
                            text: qsTr("Reset to full file")
                        }
                    }
                }
            }
        }
        Item {
            Layout.fillHeight: true
            // spacer item
            Layout.fillWidth: true
        }
        RowLayout {
            Rectangle {
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit + 10
            }
            Label {
                id: mediaTitle

                font.pointSize: 9
                text: qsTr("Sections")
            }
            Rectangle {
                Layout.fillWidth: true
                color: Kirigami.Theme.alternateBackgroundColor
                height: 1
                width: Kirigami.Units.gridUnit + 10
            }
        }
    }
    Connections {
        function onFileLoaded() {
            sectionsView.currentIndex = -1;
        }
        function onRewind() {
            mpv.loadSection(-1);
            sectionsView.currentIndex = -1;
        }

        target: mpv
    }
    ScrollView {
        id: sectionsScrollView

        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        anchors.fill: parent
        anchors.topMargin: sectionsHeader.height + 5
        clip: true
        z: 20

        ListView {
            id: sectionsView

            delegate: playSectionsItemCompact
            model: mpv.playSectionsModel
            spacing: 1
        }
    }
    Component {
        id: playSectionsItemCompact

        PlaySectionsItemCompact {
            highlighted: sectionsView.currentIndex === index

            onClicked: {
                sectionsView.currentIndex = index;
            }
        }
    }
    Timer {
        id: scrollPositionTimer

        interval: 50
        repeat: true
        running: true

        onTriggered: {
            //setSectionsScrollPosition()
            scrollPositionTimer.stop();
        }
    }
}
