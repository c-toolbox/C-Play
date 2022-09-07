/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml.Models 2.15
import QtQuick.Extras 1.4

import org.kde.kirigami 2.11 as Kirigami
import com.georgefb.haruna 1.0
import Haruna.Components 1.0

import "Menus"

ToolBar {
    id: root

    property var audioTracks
    property var subtitleTracks

    position: ToolBar.Header
    visible: !window.isFullScreen() && GeneralSettings.showHeader

    RowLayout {
        id: headerRow

        width: parent.width

        ToolButton {
            action: actions.openAction
            focusPolicy: Qt.NoFocus
        }

//        ToolButton {
//            action: actions.openUrlAction
//            focusPolicy: Qt.NoFocus
//            MouseArea {
//                anchors.fill: parent
//                acceptedButtons: Qt.MiddleButton
//                onClicked: {
//                    openUrlTextField.clear()
//                    openUrlTextField.paste()
//                    window.openFile(openUrlTextField.text, true, false)
//                }
//            }
//        }

        ToolSeparator {
            padding: vertical ? 10 : 2
            topPadding: vertical ? 2 : 10
            bottomPadding: vertical ? 2 : 10

            contentItem: Rectangle {
                implicitWidth: parent.vertical ? 1 : 24
                implicitHeight: parent.vertical ? 24 : 1
                color: Kirigami.Theme.textColor
            }
        }

        ToolButton {
            id: subtitleMenuButton

            property var model: 0

            text: qsTr("Subtitles")
            icon.name: "media-view-subtitles-symbolic"
            focusPolicy: Qt.NoFocus

            onClicked: {
                if (subtitleMenuButton.model === 0) {
                    subtitleMenuButton.model = mpv.subtitleTracksModel
                }

                subtitleMenu.visible = !subtitleMenu.visible
            }

            Menu {
                id: subtitleMenu

                y: parent.height

                Menu {
                    id: secondarySubtitleMenu

                    title: qsTr("Secondary Subtitle")

                    Instantiator {
                        id: secondarySubtitleMenuInstantiator
                        model: subtitleMenuButton.model
                        onObjectAdded: secondarySubtitleMenu.insertItem( index, object )
                        onObjectRemoved: secondarySubtitleMenu.removeItem( object )
                        delegate: MenuItem {
                            enabled: model.id !== mpv.subtitleId || model.id === 0
                            checkable: true
                            checked: model.id === mpv.secondarySubtitleId
                            text: model.text
                            onTriggered: mpv.secondarySubtitleId = model.id
                        }
                    }
                }

                MenuSeparator {}

                MenuItem {
                    text: qsTr("Primary Subtitle")
                    hoverEnabled: false
                }

                Instantiator {
                    id: primarySubtitleMenuInstantiator
                    model: subtitleMenuButton.model
                    onObjectAdded: subtitleMenu.addItem( object )
                    onObjectRemoved: subtitleMenu.removeItem( object )
                    delegate: MenuItem {
                        enabled: model.id !== mpv.secondarySubtitleId || model.id === 0
                        checkable: true
                        checked: model.id === mpv.subtitleId
                        text: model.text
                        onTriggered: mpv.subtitleId = model.id
                    }
                }
            }
        }

        ToolButton {
            text: qsTr("Audio")
            icon.name: "audio-volume-high"
            focusPolicy: Qt.NoFocus

            onClicked: {
                if (audioMenuInstantiator.model === 0) {
                    audioMenuInstantiator.model = mpv.audioTracksModel
                }
                audioMenu.visible = !audioMenu.visible
            }

            Menu {
                id: audioMenu

                y: parent.height

                Instantiator {
                    id: audioMenuInstantiator

                    model: 0
                    onObjectAdded: audioMenu.insertItem( index, object )
                    onObjectRemoved: audioMenu.removeItem( object )
                    delegate: MenuItem {
                        id: audioMenuItem
                        checkable: true
                        checked: model.id === mpv.audioId
                        text: model.text
                        onTriggered: mpv.audioId = model.id
                    }
                }
            }
        }

        ToolSeparator {
            padding: vertical ? 10 : 2
            topPadding: vertical ? 2 : 10
            bottomPadding: vertical ? 2 : 10

            contentItem: Rectangle {
                implicitWidth: parent.vertical ? 1 : 24
                implicitHeight: parent.vertical ? 24 : 1
                color: Kirigami.Theme.textColor
            }
        }

        ToolButton {
            id: gridMenuButton

            text: {
                if(mpv.gridToMapOn == 0)
                    gridMenuButton.text = qsTr("Grid (Pre-split)")
                else if(mpv.gridToMapOn == 1)
                    gridMenuButton.text = qsTr("Grid (Dome)")
                else if(mpv.gridToMapOn == 2)
                    gridMenuButton.text = qsTr("Grid (Sphere)")
            }
            icon.name: "kstars_hgrid"
            focusPolicy: Qt.NoFocus

            onClicked: {
                gridMenu.visible = !gridMenu.visible
            }

            Connections {
                target: mpv
                onGridToMapOnChanged: {
                    if(mpv.gridToMapOn == 0)
                        gridMenuButton.text = qsTr("Grid (Pre-split)")
                    else if(mpv.gridToMapOn == 1)
                        gridMenuButton.text = qsTr("Grid (Dome)")
                    else if(mpv.gridToMapOn == 2)
                        gridMenuButton.text = qsTr("Grid (Sphere)")
                }
            }

            Menu {
                id: gridMenu

                y: parent.height

                MenuSeparator {}

                ButtonGroup {
                    buttons: columnGrid.children
                }

                Column {
                    id: columnGrid

                    RadioButton {
                        id: presplit_grid
                        checked: PlaybackSettings.gridToMapOn == 0
                        text: qsTr("Flat/Pre-split")
                        onClicked: {
                            mpv.gridToMapOn = 0
                        }
                        Connections {
                            target: mpv
                            onGridToMapOnChanged: presplit_grid.checked = (mpv.gridToMapOn == 0)
                        }
                    }

                    RadioButton {
                        id: dome_grid
                        checked: PlaybackSettings.gridToMapOn == 1
                        text: qsTr("Dome")
                        onClicked: {
                             mpv.gridToMapOn = 1
                        }
                        Connections {
                            target: mpv
                            onGridToMapOnChanged: dome_grid.checked = (mpv.gridToMapOn == 1)
                        }
                    }

                    RadioButton {
                        id: sphere_grid
                        checked: PlaybackSettings.gridToMapOn == 2
                        text: qsTr("Sphere")
                        onClicked: {
                            mpv.gridToMapOn = 2
                        }
                        Connections {
                            target: mpv
                            onGridToMapOnChanged: dome_grid.checked = (mpv.gridToMapOn == 1)
                        }
                    }
                }
            }
        }

        ToolButton {
            id: stereoscopic
            action: actions.stereoscopicAction
            text: actions.stereoscopicAction.text
            focusPolicy: Qt.NoFocus

            Connections {
                target: mpv
                onStereoscopicVideoChanged: {
                    if (mpv.stereoscopicVideo) {
                        stereoscopic.text = qsTr("3D is On")
                    } else {
                        stereoscopic.text = qsTr("2D is On")
                    }
                }
            }
        }

        ToolButton {
            id: sync
            action: actions.syncAction
            text: actions.syncAction.text
            focusPolicy: Qt.NoFocus
        }

        ToolSeparator {
            padding: vertical ? 10 : 2
            topPadding: vertical ? 2 : 10
            bottomPadding: vertical ? 2 : 10

            contentItem: Rectangle {
                implicitWidth: parent.vertical ? 1 : 24
                implicitHeight: parent.vertical ? 24 : 1
                color: Kirigami.Theme.textColor
            }
        }

        ToolButton {
            id: eofMenuButton

            text: {
                const loopMode = mpv.playlistModel.loopMode(mpv.playlistModel.getPlayingVideo())
                if(loopMode===0){ //Continue
                    eofMenuButton.text = qsTr("EOF: Next ")
                }
                else if(loopMode===2){ //Loop
                    eofMenuButton.text = qsTr("EOF: Loop ")
                }
                else { // Pause (1)
                    eofMenuButton.text = qsTr("EOF: Pause")
                }
            }
            icon.name: "media-playback-pause"
            focusPolicy: Qt.NoFocus

            onClicked: {
                eofMenu.visible = !eofMenu.visible
            }

            Connections {
                target: mpv
                onFileLoaded: {
                    const loopMode = mpv.playlistModel.loopMode(mpv.playlistModel.getPlayingVideo())
                    if(loopMode===0 && playList.playlistView.count > 1){ //Continue
                        mpv.setProperty("loop-file", "no")
                        mpv.setProperty("keep-open", "no")
                        eofMenuButton.text = qsTr("EOF: Next ")
                        eofMenuButton.icon.name = "go-next"
                    }
                    else if(loopMode===2){ //Loop
                        mpv.setProperty("loop-file", "inf")
                        mpv.setProperty("keep-open", "yes")
                        eofMenuButton.text = qsTr("EOF: Loop ")
                        eofMenuButton.icon.name = "media-playlist-repeat"
                    }
                    else { // Pause (1)
                        mpv.setProperty("loop-file", "no")
                        mpv.setProperty("keep-open", "yes")
                        eofMenuButton.text = qsTr("EOF: Pause")
                        eofMenuButton.icon.name = "media-playback-pause"
                    }
                }
            }

            Menu {
                id: eofMenu

                y: parent.height

                MenuSeparator {}

                ButtonGroup {
                    buttons: columnEOF.children
                }

                Column {
                    id: columnEOF

                    RadioButton {
                        id: eof_pause
                        checked: true
                        text: qsTr("EOF: Pause")
                        onClicked: {
                            mpv.playlistModel.setLoopMode(mpv.playlistModel.getPlayingVideo(), 1)
                            mpv.setProperty("loop-file", "no")
                            mpv.setProperty("keep-open", "yes")
                            eofMenuButton.text = qsTr("EOF: Pause")
                            eofMenuButton.icon.name = "media-playback-pause"

                        }
                        Connections {
                            target: mpv
                            onFileLoaded: eof_pause.checked = (mpv.playlistModel.loopMode(mpv.playlistModel.getPlayingVideo()) === 1)
                        }
                    }

                    RadioButton {
                        id: eof_loop
                        checked: false
                        text: qsTr("EOF: Loop ")
                        onClicked: {
                            mpv.playlistModel.setLoopMode(mpv.playlistModel.getPlayingVideo(), 2)
                            mpv.setProperty("loop-file", "inf")
                            mpv.setProperty("keep-open", "yes")
                            eofMenuButton.text = qsTr("EOF: Loop")
                            eofMenuButton.icon.name = "media-playlist-repeat"
                        }
                        Connections {
                            target: mpv
                            onFileLoaded: eof_loop.checked = (mpv.playlistModel.loopMode(mpv.playlistModel.getPlayingVideo()) === 2)
                        }
                    }

                    RadioButton {
                        id: eof_next
                        checked: false
                        text: qsTr("EOF: Next ")
                        enabled: (playList.playlistView.count > 1)
                        onClicked: {
                           mpv.playlistModel.setLoopMode(mpv.playlistModel.getPlayingVideo(), 0)
                           mpv.setProperty("loop-file", "no")
                           mpv.setProperty("keep-open", "no")
                           eofMenuButton.text = qsTr("EOF: Next")
                           eofMenuButton.icon.name = "go-next"
                        }
                        Connections {
                            target: mpv
                            onFileLoaded: eof_next.checked = (mpv.playlistModel.loopMode(mpv.playlistModel.getPlayingVideo()) === 0)
                        }
                    }
                }
            }
        }

        Label {
            text: qsTr("Visibility")
            Layout.alignment: Qt.AlignRight
        }
        Slider {
            id: visibilitySlider
            value: mpv.visibility
            from: 0
            to: 100
            onValueChanged: mpv.visibility = value.toFixed(0)

            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        LabelWithTooltip {
            text: {
                if(mpv.visibility < 10)
                    qsTr("__%1\%").arg(Number(mpv.visibility))
                else if(mpv.visibility < 100)
                    qsTr("_%1\%").arg(Number(mpv.visibility))
                else
                    qsTr("%1\%").arg(Number(mpv.visibility))
            }
            elide: Text.ElideRight
        }

        ToolButton {
            text: qsTr("Fade Out")
            focusPolicy: Qt.NoFocus
            enabled: mpv.visibility != 0
            onClicked: PropertyAnimation {
                target: visibilitySlider;
                property: "value";
                to: 0;
                duration: PlaybackSettings.fadeDuration;
            }
        }
        ToolButton {
            text: qsTr("Fade In")
            focusPolicy: Qt.NoFocus
            enabled: mpv.visibility != 100
            onClicked: PropertyAnimation {
                target: visibilitySlider;
                property: "value";
                to: 100;
                duration: PlaybackSettings.fadeDuration;
            }
        }

        ToolButton {
            id: spinMenuButton

            text: {
                qsTr("Constant Spin")
            }
            focusPolicy: Qt.NoFocus

            onClicked: {
                spinMenu.visible = !spinMenu.visible
            }

            Menu {
                id: spinMenu

                y: parent.height

                MenuSeparator {}

                ButtonGroup {
                    buttons: columnSpin.children
                }

                Column {
                    id: columnSpin

                    RowLayout {
                        CheckBox {
                            id: spinX
                            checked: false
                            text: qsTr("Spin X:")
                            onClicked: {
                                if(startSpin.checked){
                                    if(spinX.checked){
                                        if(!spinTimerX.running)
                                            spinTimerX.start()
                                    }
                                    else{
                                        if(spinTimerX.running)
                                            spinTimerX.stop()
                                    }
                                }
                            }
                        }
                        SpinBox {
                            id: spinXSpeed
                            from: -1000
                            value: 10
                            to: 200 * 100
                            stepSize: 1

                            property int decimals: 2
                            property real realValue: value / 100

                            validator: DoubleValidator {
                                bottom: Math.min(spinXSpeed.from, spinXSpeed.to)
                                top:  Math.max(spinXSpeed.from, spinXSpeed.to)
                            }

                            textFromValue: function(value, locale) {
                                return Number(value / 100).toLocaleString(locale, 'f', spinXSpeed.decimals)
                            }

                            valueFromText: function(text, locale) {
                                return Number.fromLocaleString(locale, text) * 100
                            }
                        }
                    }

                    RowLayout {
                        CheckBox {
                            id: spinY
                            checked: false
                            text: qsTr("Spin Y:")
                            onClicked: {
                                if(startSpin.checked){
                                    if(spinY.checked){
                                        if(!spinTimerY.running)
                                            spinTimerY.start()
                                    }
                                    else{
                                        if(spinTimerY.running)
                                            spinTimerY.stop()
                                    }
                                }
                            }
                        }
                        SpinBox {
                            id: spinYSpeed
                            from: -1000
                            value: 10
                            to: 200 * 100
                            stepSize: 1

                            property int decimals: 2
                            property real realValue: value / 100

                            validator: DoubleValidator {
                                bottom: Math.min(spinYSpeed.from, spinYSpeed.to)
                                top:  Math.max(spinYSpeed.from, spinYSpeed.to)
                            }

                            textFromValue: function(value, locale) {
                                return Number(value / 100).toLocaleString(locale, 'f', spinYSpeed.decimals)
                            }

                            valueFromText: function(text, locale) {
                                return Number.fromLocaleString(locale, text) * 100
                            }
                        }
                    }

                    RowLayout {
                        CheckBox {
                            id: spinZ
                            checked: false
                            text: qsTr("Spin Z:")
                            onClicked: {
                                if(startSpin.checked){
                                    if(spinZ.checked){
                                        if(!spinTimerZ.running)
                                            spinTimerZ.start()
                                    }
                                    else{
                                        if(spinTimerZ.running)
                                            spinTimerZ.stop()
                                    }
                                }
                            }
                        }
                        SpinBox {
                            id: spinZSpeed
                            from: -1000
                            value: 10
                            to: 200 * 100
                            stepSize: 1

                            property int decimals: 2
                            property real realValue: value / 100

                            validator: DoubleValidator {
                                bottom: Math.min(spinZSpeed.from, spinZSpeed.to)
                                top:  Math.max(spinZSpeed.from, spinZSpeed.to)
                            }

                            textFromValue: function(value, locale) {
                                return Number(value / 100).toLocaleString(locale, 'f', spinZSpeed.decimals)
                            }

                            valueFromText: function(text, locale) {
                                return Number.fromLocaleString(locale, text) * 100
                            }
                        }
                    }

                    SpinBox {
                        id: rotateXValue
                        from: -20000
                        to: 20000
                        stepSize: 1
                        value: 0
                        visible: false

                        onValueChanged: {
                            mpv.rotateX = value / 100.0;
                        }
                    }
                    SpinBox {
                        id: rotateYValue
                        from: -20000
                        to: 20000
                        stepSize: 1
                        value: 0
                        visible: false

                        onValueChanged: {
                            mpv.rotateY = value / 100.0;
                        }
                    }
                    SpinBox {
                        id: rotateZValue
                        from: -20000
                        to: 20000
                        stepSize: 1
                        value: 0
                        visible: false

                        onValueChanged: {
                            mpv.rotateZ = value / 100.0;
                        }
                    }

                    Timer {
                        id: spinTimerX
                        interval: 1000/60;
                        running: false;
                        repeat: true
                        onTriggered: mpv.rotateX += spinXSpeed.realValue;
                    }

                    Timer {
                        id: spinTimerY
                        interval: 1000/60;
                        running: false;
                        repeat: true
                        onTriggered: mpv.rotateY += spinYSpeed.realValue;
                    }

                    Timer {
                        id: spinTimerZ
                        interval: 1000/60;
                        running: false;
                        repeat: true
                        onTriggered: mpv.rotateZ += spinZSpeed.realValue;
                    }

                    ToggleButton {
                        id: startSpin
                        text: qsTr("Start Spin")
                        checked: false
                        onClicked: {
                            if(checked){
                                startSpin.text = qsTr("Stop Spin")

                                if(spinX.checked)
                                    spinTimerX.start()

                                if(spinY.checked)
                                    spinTimerY.start()

                                if(spinZ.checked)
                                    spinTimerZ.start()
                            }
                            else{
                                startSpin.text = qsTr("Start Spin")
                                spinTimerX.stop()
                                spinTimerY.stop()
                                spinTimerZ.stop()
                            }
                        }
                        Layout.fillWidth: true
                    }
                }
            }
        }

        ToolButton {
            action: actions.configureAction
            focusPolicy: Qt.NoFocus
        }

        ToolButton {
            // using `action: actions.quitApplicationAction` breaks the action
            // doens't work on the first try in certain circumstances
            text: actions.quitApplicationAction.text
            icon: actions.quitApplicationAction.icon
            focusPolicy: Qt.NoFocus
            onClicked: actions.quitApplicationAction.trigger()
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
