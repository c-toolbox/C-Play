/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQml.Models 2.15

import org.kde.kirigami 2.11 as Kirigami
import com.georgefb.haruna 1.0
import Haruna.Components 1.0

import "Menus"

ToolBar {
    id: root

    property var audioTracks
    property var subtitleTracks
    property bool syncImageVideoFading: false

    position: ToolBar.Header
    visible: !window.isFullScreen() && GeneralSettings.showHeader

    RowLayout {
        id: headerRow

        width: parent.width

        ToolButton {
            action: actions.openAction
            focusPolicy: Qt.NoFocus
        }

        ToolButton {
            id: saveAsCPlayFileActionButton
            action: actions.saveAsCPlayFileAction
            focusPolicy: Qt.NoFocus
            enabled: false
        }

        Connections {
            target: mpv.playSectionsModel
            function onCurrentEditItemChanged() {
                saveAsCPlayFileActionButton.enabled = !mpv.playSectionsModel.isEmpty()
                actions.saveAsCPlayFileAction.enabled = !mpv.playSectionsModel.isEmpty()
            }
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

//        ToolButton {
//            id: subtitleMenuButton

//            property var model: 0

//            text: qsTr("Subtitles")
//            icon.name: "media-view-subtitles-symbolic"
//            focusPolicy: Qt.NoFocus

//            onClicked: {
//                if (subtitleMenuButton.model === 0) {
//                    subtitleMenuButton.model = mpv.subtitleTracksModel
//                }

//                subtitleMenu.visible = !subtitleMenu.visible
//            }

//            Menu {
//                id: subtitleMenu

//                y: parent.height

//                Menu {
//                    id: secondarySubtitleMenu

//                    title: qsTr("Secondary Subtitle")

//                    Instantiator {
//                        id: secondarySubtitleMenuInstantiator
//                        model: subtitleMenuButton.model
//                        onObjectAdded: secondarySubtitleMenu.insertItem( index, object )
//                        onObjectRemoved: secondarySubtitleMenu.removeItem( object )
//                        delegate: MenuItem {
//                            enabled: model.id !== mpv.subtitleId || model.id === 0
//                            checkable: true
//                            checked: model.id === mpv.secondarySubtitleId
//                            text: model.text
//                            onTriggered: mpv.secondarySubtitleId = model.id
//                        }
//                    }
//                }

//                MenuSeparator {}

//                MenuItem {
//                    text: qsTr("Primary Subtitle")
//                    hoverEnabled: false
//                }

//                Instantiator {
//                    id: primarySubtitleMenuInstantiator
//                    model: subtitleMenuButton.model
//                    onObjectAdded: subtitleMenu.addItem( object )
//                    onObjectRemoved: subtitleMenu.removeItem( object )
//                    delegate: MenuItem {
//                        enabled: model.id !== mpv.secondarySubtitleId || model.id === 0
//                        checkable: true
//                        checked: model.id === mpv.subtitleId
//                        text: model.text
//                        onTriggered: mpv.subtitleId = model.id
//                    }
//                }
//            }
//        }

        ToolButton {
            text: qsTr("Audio File")
            icon.name: "new-audio-alarm"
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

        ToolButton {
            id: mute
            action: actions.muteAction
            text: ""
            focusPolicy: Qt.NoFocus

            ToolTip {
                text: actions.muteAction.text
            }
        }

        Label {
            text: qsTr("Volume:")
            font.pointSize: 9
            Layout.alignment: Qt.AlignRight
        }

        PropertyAnimation {
            id: volume_fade_down_animation;
            target: volumeSlider;
            property: "value";
            to: 0;
            duration: PlaybackSettings.fadeDuration;
        }
        ToolButton {
            id: fade_volume_down
            icon.name: "audio-volume-low"
            focusPolicy: Qt.NoFocus
            enabled: mpv.volume !== 0
            onClicked: {
                if(!volume_fade_down_animation.running){
                    volume_fade_up_animation.to = mpv.volume;
                    volume_fade_down_animation.start()
                    if(root.syncImageVideoFading){
                        visibility_fade_out_animation.start()
                    }
                }
            }
        }

        VolumeSlider { id: volumeSlider }

        PropertyAnimation {
            id: volume_fade_up_animation;
            target: volumeSlider;
            property: "value";
            to: 100;
            duration: PlaybackSettings.fadeDuration;
            onFinished: {
                volume_fade_up_animation.to = 100;
            }
        }
        ToolButton {
            id: fade_volume_up
            icon.name: "audio-volume-high"
            focusPolicy: Qt.NoFocus
            enabled: mpv.volume !== 100
            onClicked: {
                if(!volume_fade_up_animation.running){
                    volume_fade_up_animation.start()
                    if(root.syncImageVideoFading){
                        visibility_fade_in_animation.start()
                    }
                }
            }
        }

        ToolButton {
            id: faiMenuButton
            icon.name: "media-repeat-none"
            focusPolicy: Qt.NoFocus

            ToolTip {
                id: faiToolTip
                text: "Sync audio+image fading: No"
                y: Math.round(-(parent.height - height))
            }

            onClicked: {
                faiMenu.visible = !faiMenu.visible
            }

            Menu {
                id: faiMenu

                y: parent.height

                MenuSeparator {}

                ButtonGroup {
                    buttons: columnFAI.children
                }

                Column {
                    id: columnFAI

                    RadioButton {
                        id: fai_no_fade_sync
                        checked: true
                        text: qsTr("Do not sync audio and image fading")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                faiMenuButton.icon.name = "media-repeat-none";
                                faiToolTip.text = "Sync audio+image fading: No";
                                root.syncImageVideoFading = false;
                            }
                        }
                    }

                    RadioButton {
                        id: fai_fade_sync
                        checked: false
                        text: qsTr("Sync audio+image fading")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                faiMenuButton.icon.name = "media-playlist-repeat";
                                faiToolTip.text = "Sync audio+image fading: Yes";
                                root.syncImageVideoFading = true;
                            }
                        }
                    }
                }
            }
        }

        Label {
            text: qsTr("Visibility:")
            font.pointSize: 9
            Layout.alignment: Qt.AlignRight
        }


        PropertyAnimation {
            id: visibility_fade_out_animation;
            target: visibilitySlider;
            property: "value";
            to: 0;
            duration: PlaybackSettings.fadeDuration;
        }
        ToolButton {
            id: fade_image_out
            icon.name: "view-hidden"
            focusPolicy: Qt.NoFocus
            enabled: mpv.visibility !== 0
            onClicked: {
                if(!visibility_fade_out_animation.running){
                    visibility_fade_out_animation.start()
                    if(root.syncImageVideoFading){
                        volume_fade_up_animation.to = mpv.volume;
                        volume_fade_down_animation.start()
                    }
                }
            }
        }

        VisibilitySlider { id: visibilitySlider }

        PropertyAnimation {
            id: visibility_fade_in_animation;
            target: visibilitySlider;
            property: "value";
            to: 100;
            duration: PlaybackSettings.fadeDuration;
        }
        ToolButton {
            id: fade_image_in
            icon.name: "view-visible"
            focusPolicy: Qt.NoFocus
            enabled: mpv.visibility !== 100
            onClicked: {
                if(!visibility_fade_in_animation.running){
                    visibility_fade_in_animation.start()
                    if(root.syncImageVideoFading){
                        volume_fade_up_animation.start()
                    }
                }
            }
        }

        Connections {
            target: mpv
            function onFadeVolumeDown() {
                fade_volume_down.clicked()
            }
            function onFadeVolumeUp() {
                fade_volume_up.clicked()
            }
            function onFadeImageDown() {
                fade_image_out.clicked()
            }
            function onFadeImageUp() {
                fade_image_in.clicked()
            }
        }

        ToolButton {
            id: backgroundMenuButton
            icon.name: "preview-render-on"
            focusPolicy: Qt.NoFocus

            ToolTip {
                id: backgroundToolTip
                text: "Show background when video is not visible"
                y: Math.round(-(parent.height - height))
            }

            onClicked: {
                backgroundMenu.visible = !backgroundMenu.visible
            }

            Menu {
                id: backgroundMenu

                y: parent.height

                MenuSeparator {}

                ButtonGroup {
                    buttons: columnBackground.children
                }

                Column {
                    id: columnBackground

                    RadioButton {
                        id: background_visible
                        checked: true
                        text: qsTr("Show background when video is not visible")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                backgroundMenuButton.icon.name = "preview-render-on";
                                backgroundToolTip.text = "Background visible when video is not.";
                                playerController.setBackgroundVisibility(1);
                            }
                        }
                    }

                    RadioButton {
                        id: background_not_visible
                        checked: false
                        text: qsTr("Do NOT show background when video is not visible")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                backgroundMenuButton.icon.name = "preview-render-off";
                                backgroundToolTip.text = "Background NOT visible when video is not.";
                                playerController.setBackgroundVisibility(0);
                            }
                        }
                    }
                }

                MenuSeparator {}

                Column {
                    id: columnViewOnMaster

                    RadioButton {
                        id: show_same_as_nodes
                        checked: true
                        text: qsTr("On master: Same view as nodes")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                playerController.setViewModeOnMaster(0);
                            }
                        }
                    }

                    RadioButton {
                        id: media_only
                        checked: false
                        text: qsTr("On master: Show media and Hide background")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                playerController.setViewModeOnMaster(1);
                            }
                        }
                    }

                    RadioButton {
                        id: background_only
                        checked: false
                        text: qsTr("On master: Show background and Hide media")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                playerController.setViewModeOnMaster(2);
                            }
                        }
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
            id: stereoscopicMenuButton
            implicitWidth: 100

            text: {
                if(mpv.stereoscopicMode === 0){
                    stereoscopicMenuButton.text = qsTr("2D (Mono)")
                    stereoscopicMenuButton.icon.name = "redeyes"
                }
                else if(mpv.stereoscopicMode === 1){
                    stereoscopicMenuButton.text = qsTr("3D (SBS)")
                    stereoscopicMenuButton.icon.name = "visibility"
                }
                else if(mpv.stereoscopicMode === 2){
                    stereoscopicMenuButton.text = qsTr("3D (TB)")
                    stereoscopicMenuButton.icon.name = "visibility"
                }
                else if(mpv.stereoscopicMode === 3){
                    stereoscopicMenuButton.text = qsTr("3D (TB+F)")
                    stereoscopicMenuButton.icon.name = "visibility"
                }
            }
            focusPolicy: Qt.NoFocus

            onClicked: {
                stereoscopicMenu.visible = !stereoscopicMenu.visible
            }

            Connections {
                target: mpv
                function onStereoscopicModeChanged() {
                    if(mpv.stereoscopicMode === 0){
                        stereoscopicMenuButton.text = qsTr("2D (Mono)")
                        stereoscopicMenuButton.icon.name = "redeyes"
                    }
                    else if(mpv.stereoscopicMode === 1){
                        stereoscopicMenuButton.text = qsTr("3D (SBS)")
                        stereoscopicMenuButton.icon.name = "visibility"
                    }
                    else if(mpv.stereoscopicMode === 2){
                        stereoscopicMenuButton.text = qsTr("3D (TB)")
                        stereoscopicMenuButton.icon.name = "visibility"
                    }
                    else if(mpv.stereoscopicMode === 3){
                        stereoscopicMenuButton.text = qsTr("3D (TB+F)")
                        stereoscopicMenuButton.icon.name = "visibility"
                    }
                }
            }

            Menu {
                id: stereoscopicMenu

                y: parent.height

                MenuSeparator {}

                ButtonGroup {
                    buttons: stereoscopicMenuGrid.children
                }

                Column {
                    id: stereoscopicMenuGrid

                    RadioButton {
                        id: stereoscopic_2D
                        checked: PlaybackSettings.stereoModeOnStartup === 0
                        text: qsTr("2D Mono")
                        onClicked: {
                            mpv.stereoscopicMode = 0
                        }
                        Connections {
                            target: mpv
                            function onStereoscopicModeChanged(){
                                stereoscopic_2D.checked = (mpv.stereoscopicMode === 0)
                            }
                        }
                    }

                    RadioButton {
                        id: stereoscopic_3D_sbs
                        checked: PlaybackSettings.stereoModeOnStartup === 1
                        text: qsTr("3D Side-by-side")
                        onClicked: {
                             mpv.stereoscopicMode = 1
                        }
                        Connections {
                            target: mpv
                            function onStereoscopicModeChanged(){
                                stereoscopic_3D_sbs.checked = (mpv.stereoscopicMode === 1)
                            }
                        }
                    }

                    RadioButton {
                        id: stereoscopic_3D_tp
                        checked: PlaybackSettings.stereoModeOnStartup === 2
                        text: qsTr("3D Top/Bottom")
                        onClicked: {
                            mpv.stereoscopicMode = 2
                        }
                        Connections {
                            target: mpv
                            function onStereoscopicModeChanged(){
                                stereoscopic_3D_tp.checked = (mpv.stereoscopicMode === 2)
                            }
                        }
                    }

                    RadioButton {
                        id: stereoscopic_3D_tbf
                        checked: PlaybackSettings.stereoModeOnStartup === 3
                        text: qsTr("3D Top/Bottom+Flip")
                        onClicked: {
                            mpv.stereoscopicMode = 3
                        }
                        Connections {
                            target: mpv
                            function onStereoscopicModeChanged(){
                                stereoscopic_3D_tbf.checked = (mpv.stereoscopicMode === 3)
                            }
                        }
                    }
                }
            }
        }

        ToolButton {
            id: gridMenuButton
            implicitWidth: 130

            text: {
                if(mpv.gridToMapOn === 0)
                    gridMenuButton.text = qsTr("Grid (None)")
                else if(mpv.gridToMapOn === 1)
                    gridMenuButton.text = qsTr("Grid (Plane)")
                else if(mpv.gridToMapOn === 2)
                    gridMenuButton.text = qsTr("Grid (Dome)")
                else if(mpv.gridToMapOn === 3)
                    gridMenuButton.text = qsTr("Grid (Sphere EQR)")
                else if(mpv.gridToMapOn === 4)
                    gridMenuButton.text = qsTr("Grid (Sphere EAC)")
            }
            icon.name: "kstars_hgrid"
            focusPolicy: Qt.NoFocus

            onClicked: {
                gridMenu.visible = !gridMenu.visible
            }

            Connections {
                target: mpv
                function onGridToMapOnChanged() {
                    if(mpv.gridToMapOn === 0)
                        gridMenuButton.text = qsTr("Grid (None)")
                    else if(mpv.gridToMapOn === 1)
                        gridMenuButton.text = qsTr("Grid (Plane)")
                    else if(mpv.gridToMapOn === 2)
                        gridMenuButton.text = qsTr("Grid (Dome)")
                    else if(mpv.gridToMapOn === 3)
                        gridMenuButton.text = qsTr("Grid (Sphere EQR)")
                    else if(mpv.gridToMapOn === 4)
                        gridMenuButton.text = qsTr("Grid (Sphere EAC)")
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
                        checked: PlaybackSettings.gridToMapOn === 0
                        text: qsTr("None")
                        onClicked: {
                            mpv.gridToMapOn = 0
                        }
                        Connections {
                            target: mpv
                            function onGridToMapOnChanged() {
                                presplit_grid.checked = (mpv.gridToMapOn === 0)
                            }
                        }
                    }

                    RadioButton {
                        id: plane_grid
                        checked: PlaybackSettings.gridToMapOn === 1
                        text: qsTr("Plane")
                        onClicked: {
                             mpv.gridToMapOn = 1
                        }
                        Connections {
                            target: mpv
                            function onGridToMapOnChanged() {
                                plane_grid.checked = (mpv.gridToMapOn === 1)
                            }
                        }
                    }

                    RadioButton {
                        id: dome_grid
                        checked: PlaybackSettings.gridToMapOn === 2
                        text: qsTr("Dome")
                        onClicked: {
                             mpv.gridToMapOn = 2
                        }
                        Connections {
                            target: mpv
                            function onGridToMapOnChanged() {
                                dome_grid.checked = (mpv.gridToMapOn === 2)
                            }
                        }
                    }

                    RadioButton {
                        id: sphere_eqr_grid
                        checked: PlaybackSettings.gridToMapOn === 3
                        text: qsTr("Sphere EQR")
                        onClicked: {
                            mpv.gridToMapOn = 3
                        }
                        Connections {
                            target: mpv
                            function onGridToMapOnChanged() {
                                sphere_eqr_grid.checked = (mpv.gridToMapOn === 3)
                            }
                        }
                    }

                    RadioButton {
                        id: sphere_eac_grid
                        checked: PlaybackSettings.gridToMapOn === 4
                        text: qsTr("Sphere EAC")
                        onClicked: {
                            mpv.gridToMapOn = 4
                        }
                        Connections {
                            target: mpv
                            function onGridToMapOnChanged() {
                                sphere_eac_grid.checked = (mpv.gridToMapOn === 4)
                            }
                        }
                    }
                }
            }
        }

        ToolButton {
            id: spinMenuButton

            enabled: mpv.gridToMapOn >= 2

            text: {
                qsTr("Spin + Move")
            }
            icon.name: "hand"
            focusPolicy: Qt.NoFocus

            onClicked: {
                spinMenu.visible = !spinMenu.visible
            }

            Menu {
                id: spinMenu

                y: parent.height

                MenuSeparator {}

                GridLayout {
                    columns: 2

                    Label {
                        text: qsTr("Pitch:")
                        Layout.alignment: Qt.AlignRight
                    }
                    RowLayout {
                        Button {
                            id: spinPitchUp
                            enabled: mpv.gridToMapOn >= 3
                            icon.name: "go-up"
                            icon.color: spinPitchUp.checked ? "lawngreen" : "red"
                            checkable: true
                            checked: false
                            onClicked: {
                                spinPitchDown.checked = false

                                if(spinPitchUp.checked){
                                    if(!spinTimerPitch.running)
                                        spinTimerPitch.start()
                                }
                                else{
                                    if(spinTimerPitch.running)
                                        spinTimerPitch.stop()
                                }
                            }
                            ToolTip {
                                text: qsTr("Pitch Down")
                            }
                        }
                        ToolSeparator {
                            padding: 2
                            contentItem: Rectangle {
                                implicitWidth: 1
                                implicitHeight: 24
                                color: Kirigami.Theme.textColor
                            }
                        }
                        Button {
                            id: spinPitchDown
                            enabled: mpv.gridToMapOn >= 3
                            icon.name: "go-down"
                            icon.color: spinPitchDown.checked ? "lawngreen" : "red"
                            checkable: true
                            checked: false
                            onClicked: {
                                spinPitchUp.checked = false

                                if(spinPitchDown.checked){
                                    if(!spinTimerPitch.running)
                                        spinTimerPitch.start()
                                }
                                else{
                                    if(spinTimerPitch.running)
                                        spinTimerPitch.stop()
                                }
                            }
                            ToolTip {
                                text: qsTr("Pitch Up")
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Yaw:")
                        Layout.alignment: Qt.AlignRight
                    }
                    RowLayout {
                        Button {
                            id: spinYawLeft
                            enabled: mpv.gridToMapOn >= 2
                            icon.name: "go-previous"
                            icon.color: spinYawLeft.checked ? "lawngreen" : "red"
                            checkable: true
                            checked: false
                            onClicked: {
                                spinYawRight.checked = false

                                if(spinYawLeft.checked){
                                    if(!spinTimerYaw.running)
                                        spinTimerYaw.start()
                                }
                                else{
                                    if(spinTimerYaw.running)
                                        spinTimerYaw.stop()
                                }
                            }
                            ToolTip {
                                text: qsTr("Yaw Left")
                            }
                        }
                        ToolSeparator {
                            padding: 2
                            contentItem: Rectangle {
                                implicitWidth: 1
                                implicitHeight: 24
                                color: Kirigami.Theme.textColor
                            }
                        }
                        Button {
                            id: spinYawRight
                            enabled: mpv.gridToMapOn >= 2
                            icon.name: "go-next"
                            icon.color: spinYawRight.checked ? "lawngreen" : "red"
                            checkable: true
                            checked: false
                            onClicked: {
                                spinYawLeft.checked = false

                                if(spinYawRight.checked){
                                    if(!spinTimerYaw.running)
                                        spinTimerYaw.start()
                                }
                                else{
                                    if(spinTimerYaw.running)
                                        spinTimerYaw.stop()
                                }
                            }
                            ToolTip {
                                text: qsTr("Yaw Right")
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Roll:")
                        Layout.alignment: Qt.AlignRight
                    }
                    RowLayout {
                        Button {
                            id: spinRollCounterClockwise
                            enabled: mpv.gridToMapOn >= 3
                            icon.name: "object-rotate-left"
                            icon.color: spinRollCounterClockwise.checked ? "lawngreen" : "red"
                            checkable: true
                            checked: false
                            onClicked: {
                                spinRollClockwise.checked = false

                                if(spinRollCounterClockwise.checked){
                                    if(!spinTimerRoll.running)
                                        spinTimerRoll.start()
                                }
                                else{
                                    if(spinTimerRoll.running)
                                        spinTimerRoll.stop()
                                }
                            }
                            ToolTip {
                                text: qsTr("Roll counter-clockwise")
                            }
                        }
                        ToolSeparator {
                            padding: 2
                            contentItem: Rectangle {
                                implicitWidth: 1
                                implicitHeight: 24
                                color: Kirigami.Theme.textColor
                            }
                        }
                        Button {
                            id: spinRollClockwise
                            enabled: mpv.gridToMapOn >= 3
                            icon.name: "object-rotate-right"
                            icon.color: spinRollClockwise.checked ? "lawngreen" : "red"
                            checkable: true
                            checked: false
                            onClicked: {
                                spinRollCounterClockwise.checked = false

                                if(spinRollClockwise.checked){
                                    if(!spinTimerRoll.running)
                                        spinTimerRoll.start()
                                }
                                else{
                                    if(spinTimerRoll.running)
                                        spinTimerRoll.stop()
                                }
                            }
                            ToolTip {
                                text: qsTr("Roll clockwise")
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Layout.fillWidth: true
                    }


                    Label {
                        id: spinSpeedLabel
                        text: qsTr("Speed:")
                        Layout.alignment: Qt.AlignRight
                    }
                    RowLayout {
                        SpinBox {
                            id: spinSpeed
                            from: -1000
                            value: 10
                            to: 200 * 100
                            stepSize: 1

                            property int decimals: 0
                            property real realValue: {
                                if(location.checked)
                                    return value / 1000
                                else
                                    return -value / 1000
                            }

                            validator: DoubleValidator {
                                bottom: Math.min(spinSpeed.from, spinSpeed.to)
                                top:  Math.max(spinSpeed.from, spinSpeed.to)
                            }

                            textFromValue: function(value, locale) {
                                return Number(value).toLocaleString(locale, 'f', spinSpeed.decimals)
                            }

                            valueFromText: function(text, locale) {
                                return Number.fromLocaleString(locale, text)
                            }
                        }
                        Layout.fillWidth: true
                    }

                    Label {
                        text: qsTr("Location:")
                        Layout.alignment: Qt.AlignRight
                    }
                    RowLayout {
                        Button {
                            id: location
                            text: location.checked ? "Outside" : "Inside"
                            checkable: true
                            checked: false
                            onClicked: {
                            }
                            ToolTip {
                                text: qsTr("Inside or outside sphere/mesh")
                            }
                        }
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                        }
                        Layout.fillWidth: true
                    }

                    SpinBox {
                        id: rotateXValue
                        from: -20000
                        to: 20000
                        stepSize: 1
                        value: 0
                        visible: false
                        onValueModified: mpv.rotate.x = value / 100.0;
                    }
                    SpinBox {
                        id: rotateYValue
                        from: -20000
                        to: 20000
                        stepSize: 1
                        value: 0
                        visible: false
                        onValueModified: mpv.rotate.y = value / 100.0;
                    }
                    SpinBox {
                        id: rotateZValue
                        from: -20000
                        to: 20000
                        stepSize: 1
                        value: 0
                        visible: false
                        onValueModified: mpv.rotate.z = value / 100.0;
                    }

                    Timer {
                        id: spinTimerPitch
                        interval: 1000/60;
                        running: false;
                        repeat: true
                        onTriggered: {
                            if(spinPitchUp.checked)
                                mpv.rotate.x += spinSpeed.realValue;
                            else
                                mpv.rotate.x -= spinSpeed.realValue;
                        }
                    }

                    Timer {
                        id: spinTimerYaw
                        interval: 1000/60;
                        running: false;
                        repeat: true
                        onTriggered: {
                            if(spinYawLeft.checked)
                                mpv.rotate.y += spinSpeed.realValue;
                            else
                                mpv.rotate.y -= spinSpeed.realValue;
                        }
                    }

                    Timer {
                        id: spinTimerRoll
                        interval: 1000/60;
                        running: false;
                        repeat: true
                        onTriggered: {
                            if(spinRollClockwise.checked)
                                mpv.rotate.z += spinSpeed.realValue;
                            else
                                mpv.rotate.z -= spinSpeed.realValue;
                        }
                    }

                    ToolButton {
                        id: transistionHeaderButton

                        text: {
                            qsTr("Move between scenarios")
                        }

                        enabled: !mpv.surfaceTransistionOnGoing
                        onEnabledChanged: {
                            if(enabled){
                                transistionHeaderButton.text = qsTr("Move between scenarios")
                            }
                            else{
                                transistionHeaderButton.text = qsTr("Move/transition ongoing")
                            }
                        }

                        focusPolicy: Qt.NoFocus

                        onClicked: {
                            spinPitchUp.checked = false
                            spinPitchDown.checked = false
                            spinYawLeft.checked = false
                            spinYawRight.checked = false
                            spinRollClockwise.checked = false
                            spinRollCounterClockwise.checked = false

                            spinTimerPitch.stop()
                            spinTimerYaw.stop()
                            spinTimerRoll.stop()

                            mpv.performSurfaceTransistion();
                        }

                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        Layout.columnSpan: 2
                    }
                    ToolButton {
                        id: resetOrientation

                        text: {
                            qsTr("Reset Orientation")
                        }

                        focusPolicy: Qt.NoFocus

                        onClicked: {
                            spinPitchUp.checked = false
                            spinPitchDown.checked = false
                            spinYawLeft.checked = false
                            spinYawRight.checked = false
                            spinRollClockwise.checked = false
                            spinRollCounterClockwise.checked = false

                            spinTimerPitch.stop()
                            spinTimerYaw.stop()
                            spinTimerRoll.stop()

                            mpv.resetOrientation()
                        }

                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        Layout.columnSpan: 2
                    }
                }

                Connections {
                    target: playerController

                    function onSpinPitchUp() {
                        if(mpv.gridToMapOn >= 3) {
                            spinPitchUp.clicked()
                        }
                    }
                    function onSpinPitchDown() {
                        if(mpv.gridToMapOn >= 3) {
                            spinPitchDown.clicked()
                        }
                    }
                    function onSpinYawLeft() {
                        if(mpv.gridToMapOn >= 2) {
                            spinYawLeft.clicked()
                        }
                    }
                    function onSpinYawRight() {
                        if(mpv.gridToMapOn >= 2) {
                            spinYawRight.clicked()
                        }
                    }
                    function onSpinRollCCW() {
                        if(mpv.gridToMapOn >= 3) {
                            spinRollCounterClockwise.clicked()
                        }
                    }
                    function onSpinRollCW() {
                        if(mpv.gridToMapOn >= 3) {
                            spinRollClockwise.clicked()
                        }
                    }
                    function onOrientationAndSpinReset() {
                        resetOrientation.clicked()
                    }
                    function onRunSurfaceTransistion() {
                        if(!mpv.surfaceTransistionOnGoing){
                            transistionHeaderButton.clicked()
                        }
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
            implicitWidth: 100

            text: qsTr("EOF: Pause")
            icon.name: "media-playback-pause"
            focusPolicy: Qt.NoFocus

            onClicked: {
                eofMenu.visible = !eofMenu.visible
            }

            Connections {
                target: mpv
                function onFileLoaded() {
                    const loopMode = mpv.playlistModel.loopMode(mpv.playlistModel.getPlayingVideo())
                    if(loopMode===1 && playList.playlistView.count > 1){ //Continue
                        mpv.loopMode = 1;
                        eofMenuButton.text = qsTr("EOF: Next ")
                        eofMenuButton.icon.name = "go-next"
                    }
                    else if(loopMode===2){ //Loop
                        mpv.loopMode = 2;
                        eofMenuButton.text = qsTr("EOF: Loop ")
                        eofMenuButton.icon.name = "media-playlist-repeat"
                    }
                    else { //Pause
                        mpv.loopMode = 0;
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
                            mpv.loopMode = 0;
                            eofMenuButton.text = qsTr("EOF: Pause")
                            eofMenuButton.icon.name = "media-playback-pause"

                        }
                        Connections {
                            target: mpv
                            function onFileLoaded() {
                                eof_pause.checked = (mpv.loopMode === 0)
                            }
                        }
                    }

                    RadioButton {
                        id: eof_next
                        checked: false
                        text: qsTr("EOF: Next ")
                        enabled: (playList.playlistView.count > 1)
                        onClicked: {
                           mpv.loopMode = 1;
                           eofMenuButton.text = qsTr("EOF: Next")
                           eofMenuButton.icon.name = "go-next"
                        }
                        Connections {
                            target: mpv
                            function onFileLoaded() {
                                eof_next.checked = (mpv.loopMode === 1)
                            }
                        }
                    }

                    RadioButton {
                        id: eof_loop
                        checked: false
                        text: qsTr("EOF: Loop ")
                        onClicked: {
                            mpv.loopMode = 2;
                            eofMenuButton.text = qsTr("EOF: Loop")
                            eofMenuButton.icon.name = "media-playlist-repeat"
                        }
                        Connections {
                            target: mpv
                            function onFileLoaded() {
                                eof_loop.checked = (mpv.loopMode === 2)
                            }
                        }
                    }
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
