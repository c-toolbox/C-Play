/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQml.Models

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

import "Menus"

ToolBar {
    id: root

    property var audioTracks

    position: ToolBar.Header
    visible: UserInterfaceSettings.showHeader && !window.hideUI

    RowLayout {
        id: headerRow

        width: parent.width

        ToolButton {
            action: actions.openAction
            focusPolicy: Qt.NoFocus
            display: Window.width < 1600 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon

            ToolTip {
                text: "Open a any media file, as well as *.cplayfile or *.cplaylist."
            }
        }
        ToolButton {
            id: saveAsCPlayFileActionButton

            action: actions.saveAsCPlayFileAction
            enabled: false
            focusPolicy: Qt.NoFocus
            display: Window.width < 1600 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon

            ToolTip {
                text: "Save current media and settings as a *.cplayfile"
            }
        }
        Connections {
            function onCurrentEditItemChanged() {
                saveAsCPlayFileActionButton.enabled = !mpv.playSectionsModel.isEmpty();
                actions.saveAsCPlayFileAction.enabled = !mpv.playSectionsModel.isEmpty();
            }

            target: mpv.playSectionsModel
        }
        ToolButton {
            focusPolicy: Qt.NoFocus
            icon.color: mpv.audioTracksModel.countTracks() > 0 ? "lime" : "crimson"
            icon.name: "new-audio-alarm"
            text: qsTr("Audio File")
            display: Window.width < 1600 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon

            onClicked: {
                if (audioMenuInstantiator.model === 0) {
                    audioMenuInstantiator.model = mpv.audioTracksModel;
                }
                audioMenu.visible = !audioMenu.visible;
            }

            ToolTip {
                text: "Choose the audio track/file that was loaded with the media."
            }
            Menu {
                id: audioMenu

                y: parent.height

                Instantiator {
                    id: audioMenuInstantiator

                    model: 0

                    delegate: MenuItem {
                        id: audioMenuItem

                        checkable: true
                        checked: model.id === mpv.audioId
                        text: model.text

                        onTriggered: mpv.audioId = model.id
                    }

                    onObjectAdded: audioMenu.insertItem(index, object)
                    onObjectRemoved: audioMenu.removeItem(object)
                }
            }
        }
        ToolButton {
            id: mute

            action: actions.muteAction
            focusPolicy: Qt.NoFocus
            text: ""

            ToolTip {
                text: actions.muteAction.text
            }
        }
        PropertyAnimation {
            id: volume_fade_down_animation

            duration: PlaybackSettings.fadeDuration
            property: "value"
            target: volumeSlider
            to: 0
        }
        ToolButton {
            id: fade_volume_down

            enabled: mpv.volume !== 0
            focusPolicy: Qt.NoFocus
            icon.name: "audio-volume-low"

            onClicked: {
                if (!volume_fade_down_animation.running && mpv.volume > 0) {
                    volume_fade_up_animation.to = mpv.volume;
                    volume_fade_down_animation.start();
                    if (mpv.syncVolumeVisibilityFading && !visibility_fade_out_animation.running) {
                        visibility_fade_out_animation.start();
                    }
                }
            }

            ToolTip {
                text: "Fade volume down to 0"
            }
        }
        VolumeSlider {
            id: volumeSlider

            onValueChanged: {
                if (value.toFixed(0) !== mpv.volume) {
                    mpv.volume = value.toFixed(0);
                }
            }
            Component.onCompleted: {
                volumeSlider.value = mpv.volume
            }
        }
        PropertyAnimation {
            id: volume_fade_up_animation

            duration: PlaybackSettings.fadeDuration
            property: "value"
            target: volumeSlider
            to: 100

            onFinished: {
                volume_fade_up_animation.to = 100;
            }
        }
        ToolButton {
            id: fade_volume_up

            enabled: mpv.volume !== 100
            focusPolicy: Qt.NoFocus
            icon.name: "audio-volume-high"

            onClicked: {
                if (!volume_fade_up_animation.running && mpv.volume < 100) {
                    volume_fade_up_animation.start();
                    if (mpv.syncVolumeVisibilityFading && !visibility_fade_in_animation.running) {
                        visibility_fade_in_animation.start();
                    }
                }
            }

            ToolTip {
                text: "Fade volume up to previous highest level"
            }
        }
        ToolButton {
            id: faiMenuButton

            focusPolicy: Qt.NoFocus
            icon.name: "media-repeat-none"

            onClicked: {
                faiMenu.visible = !faiMenu.visible;
            }

            ToolTip {
                id: faiToolTip

                text: "Sync audio+image fading: No"
            }
            Menu {
                id: faiMenu

                y: parent.height

                MenuSeparator {
                }
                ButtonGroup {
                    buttons: columnFAI.children
                }
                Column {
                    id: columnFAI

                    RadioButton {
                        id: fai_no_fade_sync

                        Component.onCompleted: checked = !mpv.syncVolumeVisibilityFading
                        text: qsTr("Do not sync volume and visibility fading")

                        onCheckedChanged: {
                            if (checked) {
                                faiMenuButton.icon.name = "media-repeat-none";
                                faiToolTip.text = "Sync volume+visibility fading: No";
                                if(mpv.syncVolumeVisibilityFading)
                                    mpv.syncVolumeVisibilityFading = false;
                            }
                        }
                        onClicked: {}
                    }
                    RadioButton {
                        id: fai_fade_sync

                        Component.onCompleted: checked = mpv.syncVolumeVisibilityFading
                        text: qsTr("Sync volume+visibility fading")

                        onCheckedChanged: {
                            if (checked) {
                                faiMenuButton.icon.name = "media-playlist-repeat";
                                faiToolTip.text = "Sync audio+image fading: Yes";
                                if(!mpv.syncVolumeVisibilityFading)
                                    mpv.syncVolumeVisibilityFading = true;
                            }
                        }
                        onClicked: {}
                    }
                }
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

            enabled: mpv.visibility !== 0
            focusPolicy: Qt.NoFocus
            icon.name: "view-hidden"

            onClicked: {
                if (!visibility_fade_out_animation.running) {
                    visibility_fade_out_animation.start();
                    if (mpv.syncVolumeVisibilityFading && !volume_fade_down_animation.running && mpv.volume > 0) {
                        volume_fade_up_animation.to = mpv.volume;
                        volume_fade_down_animation.start();
                    }
                }
            }

            ToolTip {
                text: "Fade media visibility down to 0."
            }
        }
        VisibilitySlider {
            id: visibilitySlider

            onValueChanged: {
                if (value.toFixed(0) !== mpv.visibility) {
                    mpv.visibility = value.toFixed(0);
                }
            }
            Component.onCompleted: {
                visibilitySlider.value = mpv.visibility
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

            enabled: mpv.visibility !== 100
            focusPolicy: Qt.NoFocus
            icon.name: "view-visible"

            onClicked: {
                if (!visibility_fade_in_animation.running) {
                    visibility_fade_in_animation.start();
                    if (mpv.syncVolumeVisibilityFading && !volume_fade_up_animation.running && mpv.volume < 100) {
                        volume_fade_up_animation.start();
                    }
                }
            }

            ToolTip {
                text: "Fade media visibility up to 100."
            }
        }
        Connections {
            function onVisibilityChanged() {
                if (visibilitySlider.value.toFixed(0) !== mpv.visibility) {
                    visibilitySlider.value = mpv.visibility;
                }
            }
            function onVolumeChanged() {
                if (volumeSlider.value.toFixed(0) !== mpv.volume) {
                    volumeSlider.value = mpv.volume;
                }
            }
            function onSyncVolumeVisibilityFadingChanged() {  
                if (mpv.syncVolumeVisibilityFading && !fai_fade_sync.checked) {
                    fai_fade_sync.checked = true;
                } 
                else if (!mpv.syncVolumeVisibilityFading && !fai_no_fade_sync.checked) {
                    fai_no_fade_sync.checked = true;
                }
            }
            function onFadeImageDown() {
                fade_image_out.clicked();
            }
            function onFadeImageUp() {
                fade_image_in.clicked();
            }
            function onFadeVolumeDown() {
                fade_volume_down.clicked();
            }
            function onFadeVolumeUp() {
                fade_volume_up.clicked();
            }

            target: mpv
        }
        ToolButton {
            id: imageMenuButton

            focusPolicy: Qt.NoFocus
            icon.name: "layer-visible-off"
            text: qsTr("Bg/Fg images")
            display: Window.width < 1600 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon

            onClicked: {
                imageMenu.visible = !imageMenu.visible;
            }

            ToolTip {
                id: imageToolTip

                text: "Background image OFF and Foreground image OFF"
            }
            Menu {
                id: imageMenu

                y: parent.height

                MenuSeparator {
                }
                ButtonGroup {
                    buttons: columnForeground.children
                }
                Column {
                    id: columnForeground

                    RadioButton {
                        id: foreground_visible

                        checked: false
                        text: qsTr("Show foreground image")

                        onCheckedChanged: {
                            if (checked) {
                                if (background_visible.checked) {
                                    imageMenuButton.icon.name = "layer-visible-on";
                                    imageToolTip.text = "Background image ON and Foreground image ON";
                                } else {
                                    imageMenuButton.icon.name = "layer-top-icon";
                                    imageToolTip.text = "Background image OFF and Foreground image ON";
                                }
                                playerController.setForegroundVisibility(1);
                            }
                        }
                        onClicked: {}

                        Connections {
                            function onForegroundVisibilityChanged() {
                                foreground_visible.checked = (playerController.foregroundVisibility() === 1);
                            }

                            target: playerController
                        }
                    }
                    RadioButton {
                        id: foreground_not_visible

                        checked: true
                        text: qsTr("Hide foreground image")

                        onCheckedChanged: {
                            if (checked) {
                                if (background_visible.checked) {
                                    imageMenuButton.icon.name = "layer-bottom-icon";
                                    imageToolTip.text = "Background image ON and Foreground image OFF";
                                } else {
                                    imageMenuButton.icon.name = "layer-visible-off";
                                    imageToolTip.text = "Background image OFF and Foreground image OFF";
                                }
                                playerController.setForegroundVisibility(0);
                            }
                        }
                        onClicked: {}

                        Connections {
                            function onForegroundVisibilityChanged() {
                                foreground_not_visible.checked = (playerController.foregroundVisibility() === 0);
                            }

                            target: playerController
                        }
                    }
                }
                MenuSeparator {
                }
                ButtonGroup {
                    buttons: columnBackground.children
                }
                Column {
                    id: columnBackground

                    RadioButton {
                        id: background_visible

                        checked: false
                        text: qsTr("Show background when video is not visible")

                        onCheckedChanged: {
                            if (checked) {
                                if (foreground_visible.checked) {
                                    imageMenuButton.icon.name = "layer-visible-on";
                                    imageToolTip.text = "Background image ON and Foreground image ON";
                                } else {
                                    imageMenuButton.icon.name = "layer-bottom-icon";
                                    imageToolTip.text = "Background image ON and Foreground image OFF";
                                }
                                playerController.setBackgroundVisibility(1);
                            }
                        }
                        onClicked: {}

                        Connections {
                            function onBackgroundVisibilityChanged() {
                                background_visible.checked = (playerController.backgroundVisibility() === 1);
                            }

                            target: playerController
                        }
                    }
                    RadioButton {
                        id: background_not_visible

                        checked: true
                        text: qsTr("Do NOT show background when video is not visible")

                        onCheckedChanged: {
                            if (checked) {
                                if (foreground_visible.checked) {
                                    imageMenuButton.icon.name = "layer-top-icon";
                                    imageToolTip.text = "Background image OFF and Foreground image ON";
                                } else {
                                    imageMenuButton.icon.name = "layer-visible-off";
                                    imageToolTip.text = "Background image OFF and Foreground image OFF";
                                }
                                playerController.setBackgroundVisibility(0);
                            }
                        }
                        onClicked: {}

                        Connections {
                            function onBackgroundVisibilityChanged() {
                                background_not_visible.checked = (playerController.backgroundVisibility() === 0);
                            }

                            target: playerController
                        }
                    }
                }
                MenuSeparator {
                }
                Column {
                    id: columnViewOnMaster

                    RadioButton {
                        id: show_same_as_nodes

                        checked: true
                        text: qsTr("On master: Same view as nodes")

                        onCheckedChanged: {
                            if (checked) {
                                playerController.setViewModeOnMaster(0);
                            }
                        }
                        onClicked: {}
                    }
                    RadioButton {
                        id: media_only

                        checked: false
                        text: qsTr("On master: Show media and Hide background")

                        onCheckedChanged: {
                            if (checked) {
                                playerController.setViewModeOnMaster(1);
                            }
                        }
                        onClicked: {}
                    }
                    RadioButton {
                        id: background_only

                        checked: false
                        text: qsTr("On master: Show background and Hide media")

                        onCheckedChanged: {
                            if (checked) {
                                playerController.setViewModeOnMaster(2);
                            }
                        }
                        onClicked: {}
                    }
                }
            }
        }
        ToolSeparator {
            bottomPadding: vertical ? 2 : 10
            padding: vertical ? 10 : 2
            topPadding: vertical ? 2 : 10

            contentItem: Rectangle {
                color: Kirigami.Theme.textColor
                implicitHeight: parent.vertical ? 24 : 1
                implicitWidth: parent.vertical ? 1 : 24
            }
        }
        ToolButton {
            id: eofMenuButton

            focusPolicy: Qt.NoFocus
            icon.name: playerController.rewindMediaOnEOF() ? "media-playback-stop" : "media-playback-pause"
            implicitWidth: Window.width < 1600 ? 40 : 100
            text: playerController.rewindMediaOnEOF() ? qsTr("EOF: Stop") : qsTr("EOF: Pause")
            display: Window.width < 1600 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon

            onClicked: {
                eofMenu.visible = !eofMenu.visible;
            }

            ToolTip {
                text: "\"End of file\" mode for current media."
            }
            Connections {
                function onFileLoaded() {
                    const eofMode = mpv.playlistModel.eofMode(mpv.playlistModel.getPlayingVideo());
                    if (eofMode === 1 && playList.playlistView.count > 1) {
                        //Continue
                        mpv.eofMode = 1;
                        eofMenuButton.text = qsTr("EOF: Next ");
                        eofMenuButton.icon.name = "go-next";
                    } else if (eofMode === 2) {
                        //Loop
                        mpv.eofMode = 2;
                        eofMenuButton.text = qsTr("EOF: Loop ");
                        eofMenuButton.icon.name = "media-playlist-repeat";
                    } else {
                        //Pause
                        mpv.eofMode = 0;
                        if (playerController.rewindMediaOnEOF()) {
                            eofMenuButton.text = qsTr("EOF: Stop");
                            eofMenuButton.icon.name = "media-playback-stop";
                        } else {
                            eofMenuButton.text = qsTr("EOF: Pause");
                            eofMenuButton.icon.name = "media-playback-pause";
                        }
                    }
                }

                target: mpv
            }
            Menu {
                id: eofMenu

                y: parent.height

                MenuSeparator {
                }
                ButtonGroup {
                    buttons: columnEOF.children
                }
                Column {
                    id: columnEOF

                    RadioButton {
                        id: eof_pause

                        checked: true
                        text: qsTr("EOF: Pause (Or Stop, see below)")

                        onClicked: {
                            mpv.eofMode = 0;
                            if (playerController.rewindMediaOnEOF()) {
                                eofMenuButton.text = qsTr("EOF: Stop");
                                eofMenuButton.icon.name = "media-playback-stop";
                            } else {
                                eofMenuButton.text = qsTr("EOF: Pause");
                                eofMenuButton.icon.name = "media-playback-pause";
                            }
                        }

                        Connections {
                            function onFileLoaded() {
                                eof_pause.checked = (mpv.eofMode === 0);
                            }

                            target: mpv
                        }
                    }
                    RadioButton {
                        id: eof_next

                        checked: false
                        enabled: (playList.playlistView.count > 1)
                        text: qsTr("EOF: Next ")

                        onClicked: {
                            mpv.eofMode = 1;
                            eofMenuButton.text = qsTr("EOF: Next");
                            eofMenuButton.icon.name = "go-next";
                        }

                        Connections {
                            function onFileLoaded() {
                                eof_next.checked = (mpv.eofMode === 1);
                            }

                            target: mpv
                        }
                    }
                    RadioButton {
                        id: eof_loop

                        checked: false
                        text: qsTr("EOF: Loop ")

                        onClicked: {
                            mpv.eofMode = 2;
                            eofMenuButton.text = qsTr("EOF: Loop");
                            eofMenuButton.icon.name = "media-playlist-repeat";
                        }

                        Connections {
                            function onFileLoaded() {
                                eof_loop.checked = (mpv.eofMode === 2);
                            }

                            target: mpv
                        }
                    }
                }
                MenuSeparator {
                }
                Column {
                    id: columeFadeOnPauseEOF

                    RadioButton {
                        id: do_not_rewind_on_eof

                        checked: !playerController.rewindMediaOnEOF()
                        text: qsTr("EOF-Pause: Only pause media.")

                        onClicked: {
                            if (checked) {
                                playerController.setRewindMediaOnEOF(false);
                                if (mpv.eofMode === 0) {
                                    eofMenuButton.text = qsTr("EOF: Pause");
                                    eofMenuButton.icon.name = "media-playback-pause";
                                }
                            }
                        }

                        Connections {
                            function onRewindMediaOnEOFChanged() {
                                do_not_rewind_on_eof.checked = !playerController.rewindMediaOnEOF();
                            }

                            target: playerController
                        }
                    }
                    RadioButton {
                        id: do_rewind_on_eof

                        checked: playerController.rewindMediaOnEOF()
                        text: qsTr("EOF-Pause: Stop/rewind instead of pause.")

                        onClicked: {
                            if (checked) {
                                playerController.setRewindMediaOnEOF(true);
                                if (mpv.eofMode === 0) {
                                    eofMenuButton.text = qsTr("EOF: Stop");
                                    eofMenuButton.icon.name = "media-playback-stop";
                                }
                            }
                        }

                        Connections {
                            function onRewindMediaOnEOFChanged() {
                                do_rewind_on_eof.checked = playerController.rewindMediaOnEOF();
                            }

                            target: playerController
                        }
                    }
                }
            }
        }
        ToolButton {
            id: stereoscopicMenuButton

            focusPolicy: Qt.NoFocus
            implicitWidth: Window.width < 1600 ? 40 : 100 
            text: {
                if (mpv.stereoscopicMode === 0) {
                    stereoscopicMenuButton.text = qsTr("2D (Mono)");
                    stereoscopicMenuButton.icon.name = "redeyes";
                } else if (mpv.stereoscopicMode === 1) {
                    stereoscopicMenuButton.text = qsTr("3D (SBS)");
                    stereoscopicMenuButton.icon.name = "visibility";
                } else if (mpv.stereoscopicMode === 2) {
                    stereoscopicMenuButton.text = qsTr("3D (TB)");
                    stereoscopicMenuButton.icon.name = "visibility";
                } else if (mpv.stereoscopicMode === 3) {
                    stereoscopicMenuButton.text = qsTr("3D (TB+F)");
                    stereoscopicMenuButton.icon.name = "visibility";
                }
            }
            display: Window.width < 1600 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon

            onClicked: {
                stereoscopicMenu.visible = !stereoscopicMenu.visible;
            }

            ToolTip {
                text: "Stereoscopic mode of the current media."
            }
            Connections {
                function onStereoscopicModeChanged() {
                    if (mpv.stereoscopicMode === 0) {
                        stereoscopicMenuButton.text = qsTr("2D (Mono)");
                        stereoscopicMenuButton.icon.name = "redeyes";
                    } else if (mpv.stereoscopicMode === 1) {
                        stereoscopicMenuButton.text = qsTr("3D (SBS)");
                        stereoscopicMenuButton.icon.name = "visibility";
                    } else if (mpv.stereoscopicMode === 2) {
                        stereoscopicMenuButton.text = qsTr("3D (TB)");
                        stereoscopicMenuButton.icon.name = "visibility";
                    } else if (mpv.stereoscopicMode === 3) {
                        stereoscopicMenuButton.text = qsTr("3D (TB+F)");
                        stereoscopicMenuButton.icon.name = "visibility";
                    }
                    if (mpv.stereoscopicMode > 0) {
                        if (playerController.getViewModeOnClients() > 0) {
                            stereoscopicMenuButton.text += qsTr("->2D");
                            stereoscopicMenuButton.icon.name = "redeyes";
                        }
                    }
                }

                target: mpv
            }
            Menu {
                id: stereoscopicMenu

                y: parent.height

                MenuSeparator {
                }
                ButtonGroup {
                    buttons: stereoscopicMenuGrid.children
                }
                Column {
                    id: stereoscopicMenuGrid

                    RadioButton {
                        id: stereoscopic_2D

                        checked: (mpv.stereoscopicMode === 0)
                        text: qsTr("2D Mono")

                        onClicked: {
                            mpv.stereoscopicMode = 0;
                            mpv.playSectionsModel.currentEditItemIsEdited = true;
                        }
                    }
                    RadioButton {
                        id: stereoscopic_3D_sbs

                        checked: (mpv.stereoscopicMode === 1)
                        text: qsTr("3D Side-by-side")

                        onClicked: {
                            mpv.stereoscopicMode = 1;
                            mpv.playSectionsModel.currentEditItemIsEdited = true;
                        }
                    }
                    RadioButton {
                        id: stereoscopic_3D_tp

                        checked: (mpv.stereoscopicMode === 2)
                        text: qsTr("3D Top/Bottom")

                        onClicked: {
                            mpv.stereoscopicMode = 2;
                            mpv.playSectionsModel.currentEditItemIsEdited = true;
                        }
                    }
                    RadioButton {
                        id: stereoscopic_3D_tbf

                        checked: (mpv.stereoscopicMode === 3)
                        text: qsTr("3D Top/Bottom+Flip")

                        onClicked: {
                            mpv.stereoscopicMode = 3;
                            mpv.playSectionsModel.currentEditItemIsEdited = true;
                        }
                    }
                }
                MenuSeparator {
                }
                ButtonGroup {
                    buttons: columnViewOnClients.children
                }
                Column {
                    id: columnViewOnClients

                    RadioButton {
                        id: clients_auto_2D_3D_switch

                        checked: true
                        text: qsTr("On clients: Auto 2D / 3D switch")

                        onCheckedChanged: {
                            if (checked) {
                                playerController.setViewModeOnClients(0);
                                mpv.stereoscopicModeChanged();
                            }
                        }
                        onClicked: {}

                        Connections {
                            function onViewModeOnClientsChanged() {
                                clients_auto_2D_3D_switch.checked = (playerController.getViewModeOnClients() === 0);
                                mpv.stereoscopicModeChanged();
                            }

                            target: playerController
                        }
                    }
                    RadioButton {
                        id: clients_force_2D

                        checked: false
                        text: qsTr("On clients: Force all to 2D")

                        onCheckedChanged: {
                            if (checked) {
                                playerController.setViewModeOnClients(1);
                                mpv.stereoscopicModeChanged();
                            }
                        }
                        onClicked: {}

                        Connections {
                            function onViewModeOnClientsChanged() {
                                clients_force_2D.checked = (playerController.getViewModeOnClients() === 1);
                                mpv.stereoscopicModeChanged();
                            }

                            target: playerController
                        }
                    }
                }
            }
        }
        ToolButton {
            id: gridMenuButton

            focusPolicy: Qt.NoFocus
            icon.name: "kstars_hgrid"
            implicitWidth: Window.width < 1600 ? 40 : 130
            text: {
                if (mpv.gridToMapOn === 0)
                    gridMenuButton.text = qsTr("Grid (None)");
                else if (mpv.gridToMapOn === 1)
                    gridMenuButton.text = qsTr("Grid (Plane)");
                else if (mpv.gridToMapOn === 2)
                    gridMenuButton.text = qsTr("Grid (Dome)");
                else if (mpv.gridToMapOn === 3)
                    gridMenuButton.text = qsTr("Grid (Sphere EQR)");
                else if (mpv.gridToMapOn === 4)
                    gridMenuButton.text = qsTr("Grid (Sphere EAC)");
            }
            display: Window.width < 1600 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon

            onClicked: {
                gridMenu.visible = !gridMenu.visible;
            }

            ToolTip {
                text: "Mapping/grid mode of the current media."
            }
            Connections {
                function onGridToMapOnChanged() {
                    if (mpv.gridToMapOn === 0)
                        gridMenuButton.text = qsTr("Grid (None)");
                    else if (mpv.gridToMapOn === 1)
                        gridMenuButton.text = qsTr("Grid (Plane)");
                    else if (mpv.gridToMapOn === 2)
                        gridMenuButton.text = qsTr("Grid (Dome)");
                    else if (mpv.gridToMapOn === 3)
                        gridMenuButton.text = qsTr("Grid (Sphere EQR)");
                    else if (mpv.gridToMapOn === 4)
                        gridMenuButton.text = qsTr("Grid (Sphere EAC)");
                }

                target: mpv
            }
            Menu {
                id: gridMenu

                y: parent.height

                MenuSeparator {
                }
                ButtonGroup {
                    buttons: columnGrid.children
                }
                Column {
                    id: columnGrid

                    RadioButton {
                        id: presplit_grid

                        checked: false
                        text: qsTr("None")

                        onClicked: {
                            mpv.gridToMapOn = 0;
                            mpv.playSectionsModel.currentEditItemIsEdited = true;
                        }

                        Connections {
                            function onGridToMapOnChanged() {
                                presplit_grid.checked = (mpv.gridToMapOn === 0);
                            }

                            target: mpv
                        }
                    }
                    RadioButton {
                        id: plane_grid

                        checked: false
                        text: qsTr("Plane")

                        onClicked: {
                            mpv.gridToMapOn = 1;
                            mpv.playSectionsModel.currentEditItemIsEdited = true;
                        }

                        Connections {
                            function onGridToMapOnChanged() {
                                plane_grid.checked = (mpv.gridToMapOn === 1);
                            }

                            target: mpv
                        }
                    }
                    RadioButton {
                        id: dome_grid

                        checked: false
                        text: qsTr("Dome")

                        onClicked: {
                            mpv.gridToMapOn = 2;
                            mpv.playSectionsModel.currentEditItemIsEdited = true;
                        }

                        Connections {
                            function onGridToMapOnChanged() {
                                dome_grid.checked = (mpv.gridToMapOn === 2);
                            }

                            target: mpv
                        }
                    }
                    RadioButton {
                        id: sphere_eqr_grid

                        checked: false
                        text: qsTr("Sphere EQR")

                        onClicked: {
                            mpv.gridToMapOn = 3;
                            mpv.playSectionsModel.currentEditItemIsEdited = true;
                        }

                        Connections {
                            function onGridToMapOnChanged() {
                                sphere_eqr_grid.checked = (mpv.gridToMapOn === 3);
                            }

                            target: mpv
                        }
                    }
                    RadioButton {
                        id: sphere_eac_grid

                        checked: false
                        text: qsTr("Sphere EAC")

                        onClicked: {
                            mpv.gridToMapOn = 4;
                            mpv.playSectionsModel.currentEditItemIsEdited = true;
                        }

                        Connections {
                            function onGridToMapOnChanged() {
                                sphere_eac_grid.checked = (mpv.gridToMapOn === 4);
                            }

                            target: mpv
                        }
                    }
                }
            }
        }
        ToolButton {
            id: spinMenuButton

            enabled: mpv.gridToMapOn >= 2
            focusPolicy: Qt.NoFocus
            icon.color: (!spinPitchUp.checked & !spinPitchDown.checked & !spinYawLeft.checked & !spinYawRight.checked & !spinRollClockwise.checked & !spinRollCounterClockwise.checked) ? "crimson" : "lime"
            icon.name: "hand"
            text: {
                qsTr("Spin && Move");
            }
            display: Window.width < 1600 ? AbstractButton.IconOnly : AbstractButton.TextBesideIcon

            onClicked: {
                spinMenu.visible = !spinMenu.visible;
            }

            ToolTip {
                text: "Spin and translate the mapping grid."
            }
            Menu {
                id: spinMenu

                y: parent.height

                MenuSeparator {
                }
                GridLayout {
                    columns: 2

                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Pitch:")
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            id: spinPitchUp

                            checkable: true
                            checked: false
                            enabled: mpv.gridToMapOn >= 3
                            icon.color: spinPitchUp.checked ? "lime" : "crimson"
                            icon.name: "go-up"

                            onCheckedChanged: {
                                if (spinPitchUp.checked) {
                                    if (spinPitchDown.checked) {
                                        spinPitchDown.checked = false;
                                    } else if (!spinTimerPitch.running)
                                        spinTimerPitch.start();
                                } else if (!spinPitchDown.checked) {
                                    if (spinTimerPitch.running)
                                        spinTimerPitch.stop();
                                }
                            }

                            ToolTip {
                                text: qsTr("Pitch Down")
                            }
                        }
                        ToolSeparator {
                            padding: 2

                            contentItem: Rectangle {
                                color: Kirigami.Theme.textColor
                                implicitHeight: 24
                                implicitWidth: 1
                            }
                        }
                        Button {
                            id: spinPitchDown

                            checkable: true
                            checked: false
                            enabled: mpv.gridToMapOn >= 3
                            icon.color: spinPitchDown.checked ? "lime" : "crimson"
                            icon.name: "go-down"

                            onCheckedChanged: {
                                if (spinPitchDown.checked) {
                                    if (spinPitchUp.checked) {
                                        spinPitchUp.checked = false;
                                    } else if (!spinTimerPitch.running)
                                        spinTimerPitch.start();
                                } else if (!spinPitchUp.checked) {
                                    if (spinTimerPitch.running)
                                        spinTimerPitch.stop();
                                }
                            }

                            ToolTip {
                                text: qsTr("Pitch Up")
                            }
                        }
                        Item {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                        }
                    }
                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Yaw:")
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            id: spinYawLeft

                            checkable: true
                            checked: false
                            enabled: mpv.gridToMapOn >= 2
                            icon.color: spinYawLeft.checked ? "lime" : "crimson"
                            icon.name: "go-previous"

                            onCheckedChanged: {
                                if (spinYawLeft.checked) {
                                    if (spinYawRight.checked) {
                                        spinYawRight.checked = false;
                                    } else if (!spinTimerYaw.running)
                                        spinTimerYaw.start();
                                } else if (!spinYawRight.checked) {
                                    if (spinTimerYaw.running)
                                        spinTimerYaw.stop();
                                }
                            }

                            ToolTip {
                                text: qsTr("Yaw Left")
                            }
                        }
                        ToolSeparator {
                            padding: 2

                            contentItem: Rectangle {
                                color: Kirigami.Theme.textColor
                                implicitHeight: 24
                                implicitWidth: 1
                            }
                        }
                        Button {
                            id: spinYawRight

                            checkable: true
                            checked: false
                            enabled: mpv.gridToMapOn >= 2
                            icon.color: spinYawRight.checked ? "lime" : "crimson"
                            icon.name: "go-next"

                            onCheckedChanged: {
                                if (spinYawRight.checked) {
                                    if (spinYawLeft.checked) {
                                        spinYawLeft.checked = false;
                                    } else if (!spinTimerYaw.running)
                                        spinTimerYaw.start();
                                } else if (!spinYawLeft.checked) {
                                    if (spinTimerYaw.running)
                                        spinTimerYaw.stop();
                                }
                            }

                            ToolTip {
                                text: qsTr("Yaw Right")
                            }
                        }
                        Item {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                        }
                    }
                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Roll:")
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            id: spinRollCounterClockwise

                            checkable: true
                            checked: false
                            enabled: mpv.gridToMapOn >= 3
                            icon.color: spinRollCounterClockwise.checked ? "lime" : "crimson"
                            icon.name: "object-rotate-left"

                            onCheckedChanged: {
                                if (spinRollCounterClockwise.checked) {
                                    if (spinRollClockwise.checked) {
                                        spinRollClockwise.checked = false;
                                    } else if (!spinTimerYaw.running)
                                        spinTimerRoll.start();
                                } else if (!spinRollClockwise.checked) {
                                    if (spinTimerRoll.running)
                                        spinTimerRoll.stop();
                                }
                            }

                            ToolTip {
                                text: qsTr("Roll counter-clockwise")
                            }
                        }
                        ToolSeparator {
                            padding: 2

                            contentItem: Rectangle {
                                color: Kirigami.Theme.textColor
                                implicitHeight: 24
                                implicitWidth: 1
                            }
                        }
                        Button {
                            id: spinRollClockwise

                            checkable: true
                            checked: false
                            enabled: mpv.gridToMapOn >= 3
                            icon.color: spinRollClockwise.checked ? "lime" : "crimson"
                            icon.name: "object-rotate-right"

                            onCheckedChanged: {
                                if (spinRollClockwise.checked) {
                                    if (spinRollCounterClockwise.checked) {
                                        spinRollCounterClockwise.checked = false;
                                    } else if (!spinTimerYaw.running)
                                        spinTimerRoll.start();
                                } else if (!spinRollCounterClockwise.checked) {
                                    if (spinTimerRoll.running)
                                        spinTimerRoll.stop();
                                }
                            }

                            ToolTip {
                                text: qsTr("Roll clockwise")
                            }
                        }
                        Item {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                        }
                    }
                    Label {
                        id: spinSpeedLabel

                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Speed:")
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        SpinBox {
                            id: spinSpeed

                            property int decimals: 0
                            property real realValue: {
                                if (location.checked)
                                    return value / 1000;
                                else
                                    return -value / 1000;
                            }

                            from: -1000
                            stepSize: 1
                            textFromValue: function (value, locale) {
                                return Number(value).toLocaleString(locale, 'f', spinSpeed.decimals);
                            }
                            to: 200 * 100
                            value: 10
                            valueFromText: function (text, locale) {
                                return Number.fromLocaleString(locale, text);
                            }

                            validator: DoubleValidator {
                                bottom: Math.min(spinSpeed.from, spinSpeed.to)
                                top: Math.max(spinSpeed.from, spinSpeed.to)
                            }
                        }
                    }
                    Label {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Location:")
                    }
                    RowLayout {
                        Layout.fillWidth: true

                        Button {
                            id: location

                            checkable: true
                            checked: false
                            text: location.checked ? "Outside" : "Inside"

                            onClicked: {}

                            ToolTip {
                                text: qsTr("Inside or outside sphere/mesh")
                            }
                        }
                        Item {
                            Layout.fillHeight: true
                            Layout.fillWidth: true
                        }
                    }
                    SpinBox {
                        id: rotateXValue

                        from: -20000
                        stepSize: 1
                        to: 20000
                        value: 0
                        visible: false

                        onValueModified: mpv.rotate.x = value / 100.0
                    }
                    SpinBox {
                        id: rotateYValue

                        from: -20000
                        stepSize: 1
                        to: 20000
                        value: 0
                        visible: false

                        onValueModified: mpv.rotate.y = value / 100.0
                    }
                    SpinBox {
                        id: rotateZValue

                        from: -20000
                        stepSize: 1
                        to: 20000
                        value: 0
                        visible: false

                        onValueModified: mpv.rotate.z = value / 100.0
                    }
                    Timer {
                        id: spinTimerPitch

                        interval: 1000 / 60
                        repeat: true
                        running: false

                        onTriggered: {
                            if (spinPitchUp.checked)
                                mpv.rotate.x += spinSpeed.realValue;
                            else
                                mpv.rotate.x -= spinSpeed.realValue;
                        }
                    }
                    Timer {
                        id: spinTimerYaw

                        interval: 1000 / 60
                        repeat: true
                        running: false

                        onTriggered: {
                            if (spinYawLeft.checked)
                                mpv.rotate.y += spinSpeed.realValue;
                            else
                                mpv.rotate.y -= spinSpeed.realValue;
                        }
                    }
                    Timer {
                        id: spinTimerRoll

                        interval: 1000 / 60
                        repeat: true
                        running: false

                        onTriggered: {
                            if (spinRollClockwise.checked)
                                mpv.rotate.z += spinSpeed.realValue;
                            else
                                mpv.rotate.z -= spinSpeed.realValue;
                        }
                    }
                    ToolButton {
                        id: transitionHeaderButton

                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        Layout.columnSpan: 2
                        enabled: !mpv.surfaceTransitionOnGoing
                        focusPolicy: Qt.NoFocus
                        text: {
                            qsTr("Move between scenarios");
                        }

                        onClicked: {
                            spinPitchUp.checked = false;
                            spinPitchDown.checked = false;
                            spinYawLeft.checked = false;
                            spinYawRight.checked = false;
                            spinRollClockwise.checked = false;
                            spinRollCounterClockwise.checked = false;
                            spinTimerPitch.stop();
                            spinTimerYaw.stop();
                            spinTimerRoll.stop();
                            mpv.performSurfaceTransition();
                        }
                        onEnabledChanged: {
                            if (enabled) {
                                transitionHeaderButton.text = qsTr("Move between scenarios");
                            } else {
                                transitionHeaderButton.text = qsTr("Move/transition ongoing");
                            }
                        }
                    }
                    ToolButton {
                        id: resetOrientation

                        Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                        Layout.columnSpan: 2
                        focusPolicy: Qt.NoFocus
                        text: {
                            qsTr("Reset Orientation");
                        }

                        onClicked: {
                            spinPitchUp.checked = false;
                            spinPitchDown.checked = false;
                            spinYawLeft.checked = false;
                            spinYawRight.checked = false;
                            spinRollClockwise.checked = false;
                            spinRollCounterClockwise.checked = false;
                            spinTimerPitch.stop();
                            spinTimerYaw.stop();
                            spinTimerRoll.stop();
                            mpv.resetOrientation();
                        }
                    }
                }
                Connections {
                    function onOrientationAndSpinReset() {
                        resetOrientation.clicked();
                    }
                    function onRunSurfaceTransition() {
                        if (!mpv.surfaceTransitionOnGoing) {
                            transitionHeaderButton.clicked();
                        }
                    }
                    function onSpinPitchDown(run) {
                        if (mpv.gridToMapOn >= 3) {
                            spinPitchDown.checked = run;
                        }
                    }
                    function onSpinPitchUp(run) {
                        if (mpv.gridToMapOn >= 3) {
                            spinPitchUp.checked = run;
                        }
                    }
                    function onSpinRollCCW(run) {
                        if (mpv.gridToMapOn >= 3) {
                            spinRollCounterClockwise.checked = run;
                        }
                    }
                    function onSpinRollCW(run) {
                        if (mpv.gridToMapOn >= 3) {
                            spinRollClockwise.checked = run;
                        }
                    }
                    function onSpinYawLeft(run) {
                        if (mpv.gridToMapOn >= 2) {
                            spinYawLeft.checked = run;
                        }
                    }
                    function onSpinYawRight(run) {
                        if (mpv.gridToMapOn >= 2) {
                            spinYawRight.checked = run;
                        }
                    }

                    target: playerController
                }
            }
        }
        ToolSeparator {
            bottomPadding: vertical ? 2 : 10
            padding: vertical ? 10 : 2
            topPadding: vertical ? 2 : 10

            contentItem: Rectangle {
                color: Kirigami.Theme.textColor
                implicitHeight: parent.vertical ? 24 : 1
                implicitWidth: parent.vertical ? 1 : 24
            }
        }
        ToolButton {
            id: windowOpacity

            action: actions.windowOpacityAction
            focusPolicy: Qt.NoFocus
            display: AbstractButton.IconOnly
            ToolTip {
                text: "ON/OFF to have node windows visible."
            }
        }
        ToolButton {
            id: sync

            action: actions.syncAction
            focusPolicy: Qt.NoFocus
            display: AbstractButton.IconOnly
            ToolTip {
                text: "ON/OFF to sync state from master to clients."
            }
        }
        Item {
            Layout.fillWidth: true
        }
    }
}
