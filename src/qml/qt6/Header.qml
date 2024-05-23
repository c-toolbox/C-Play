/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
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
    visible: !window.isFullScreen() && UserInterfaceSettings.showHeader

    RowLayout {
        id: headerRow

        width: parent.width

        ToolButton {
            action: actions.openAction
            focusPolicy: Qt.NoFocus
            ToolTip {
                text: "Open a any media file, as well as *.cplayfile or *.cplaylist."
            }
        }

        ToolButton {
            id: saveAsCPlayFileActionButton
            action: actions.saveAsCPlayFileAction
            focusPolicy: Qt.NoFocus
            enabled: false
            ToolTip {
                text: "Save current media and settings as a *.cplayfile"
            }
        }

        Connections {
            target: mpv.playSectionsModel
            function onCurrentEditItemChanged() {
                saveAsCPlayFileActionButton.enabled = !mpv.playSectionsModel.isEmpty()
                actions.saveAsCPlayFileAction.enabled = !mpv.playSectionsModel.isEmpty()
            }
        }

        ToolButton {
            text: qsTr("Audio File")
            icon.name: "new-audio-alarm"
            icon.color: mpv.audioTracksModel.countTracks() > 0 ? "lime" : "crimson"
            focusPolicy: Qt.NoFocus

            ToolTip {
                text: "Choose the audio track/file that was loaded with the media."
            }

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
                if(!volume_fade_down_animation.running && mpv.volume > 0){
                    volume_fade_up_animation.to = mpv.volume;
                    volume_fade_down_animation.start()
                    if(mpv.syncVolumeVisibilityFading && !visibility_fade_out_animation.running){
                        visibility_fade_out_animation.start()
                    }
                }
            }
            ToolTip {
                text: "Fade volume down to 0"
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
                if(!volume_fade_up_animation.running && mpv.volume < 100){
                    volume_fade_up_animation.start()
                    if(mpv.syncVolumeVisibilityFading && !visibility_fade_in_animation.running){
                        visibility_fade_in_animation.start()
                    }
                }
            }
            ToolTip {
                text: "Fade volume up to previous highest level"
            }
        }

        ToolButton {
            id: faiMenuButton
            icon.name: "media-repeat-none"
            focusPolicy: Qt.NoFocus

            ToolTip {
                id: faiToolTip
                text: "Sync audio+image fading: No"
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
                        checked: mpv.syncVolumeVisibilityFading === false
                        text: qsTr("Do not sync volume and visibility fading")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                faiMenuButton.icon.name = "media-repeat-none";
                                faiToolTip.text = "Sync volume+visibility fading: No";
                                mpv.syncVolumeVisibilityFading = false;
                            }
                        }
                    }

                    RadioButton {
                        id: fai_fade_sync
                        checked: mpv.syncVolumeVisibilityFading === true
                        text: qsTr("Sync volume+visibility fading")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                faiMenuButton.icon.name = "media-playlist-repeat";
                                faiToolTip.text = "Sync audio+image fading: Yes";
                                mpv.syncVolumeVisibilityFading = true;
                            }
                        }
                    }
                }
            }
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
                    if(mpv.syncVolumeVisibilityFading && !volume_fade_down_animation.running && mpv.volume > 0){
                        volume_fade_up_animation.to = mpv.volume;
                        volume_fade_down_animation.start()
                    }
                }
            }
            ToolTip {
                text: "Fade media visibility down to 0."
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
                    if(mpv.syncVolumeVisibilityFading && !volume_fade_up_animation.running && mpv.volume < 100){
                        volume_fade_up_animation.start()
                    }
                }
            }
            ToolTip {
                text: "Fade media visibility up to 100."
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
            id: imageMenuButton
            text: qsTr("Bg/Fg images")
            icon.name: "layer-visible-off"
            focusPolicy: Qt.NoFocus

            ToolTip {
                id: imageToolTip
                text: "Background image OFF and Foreground image OFF"
            }

            onClicked: {
                imageMenu.visible = !imageMenu.visible
            }

            Menu {
                id: imageMenu

                y: parent.height

                MenuSeparator {}

                ButtonGroup {
                    buttons: columnForeground.children
                }

                Column {
                    id: columnForeground

                    RadioButton {
                        id: foreground_visible
                        checked: false
                        text: qsTr("Show foreground image")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                if(background_visible.checked){
                                    imageMenuButton.icon.name = "layer-visible-on";
                                    imageToolTip.text = "Background image ON and Foreground image ON";
                                }
                                else {
                                    imageMenuButton.icon.name = "layer-top-icon";
                                    imageToolTip.text = "Background image OFF and Foreground image ON";
                                }
                                playerController.setForegroundVisibility(1);
                            }
                        }
                        Connections {
                            target: playerController
                            function onForegroundVisibilityChanged(){
                                foreground_visible.checked = (playerController.foregroundVisibility() === 1)
                            }
                        }
                    }

                    RadioButton {
                        id: foreground_not_visible
                        checked: true
                        text: qsTr("Hide foreground image")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                if(background_visible.checked){
                                    imageMenuButton.icon.name = "layer-bottom-icon";
                                    imageToolTip.text = "Background image ON and Foreground image OFF";
                                }
                                else {
                                    imageMenuButton.icon.name = "layer-visible-off";
                                    imageToolTip.text = "Background image OFF and Foreground image OFF";
                                }
                                playerController.setForegroundVisibility(0);
                            }
                        }
                        Connections {
                            target: playerController
                            function onForegroundVisibilityChanged(){
                                foreground_not_visible.checked = (playerController.foregroundVisibility() === 0)
                            }
                        }
                    }
                }

                MenuSeparator {}

                ButtonGroup {
                    buttons: columnBackground.children
                }

                Column {
                    id: columnBackground

                    RadioButton {
                        id: background_visible
                        checked: false
                        text: qsTr("Show background when video is not visible")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                if(foreground_visible.checked){
                                    imageMenuButton.icon.name = "layer-visible-on";
                                    imageToolTip.text = "Background image ON and Foreground image ON";
                                }
                                else {
                                    imageMenuButton.icon.name = "layer-bottom-icon";
                                    imageToolTip.text = "Background image ON and Foreground image OFF";
                                }
                                playerController.setBackgroundVisibility(1);
                            }
                        }
                        Connections {
                            target: playerController
                            function onBackgroundVisibilityChanged(){
                                background_visible.checked = (playerController.backgroundVisibility() === 1)
                            }
                        }
                    }

                    RadioButton {
                        id: background_not_visible
                        checked: true
                        text: qsTr("Do NOT show background when video is not visible")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                if(foreground_visible.checked){
                                    imageMenuButton.icon.name = "layer-top-icon";
                                    imageToolTip.text = "Background image OFF and Foreground image ON";
                                }
                                else {
                                    imageMenuButton.icon.name = "layer-visible-off";
                                    imageToolTip.text = "Background image OFF and Foreground image OFF";
                                }
                                playerController.setBackgroundVisibility(0);
                            }
                        }
                        Connections {
                            target: playerController
                            function onBackgroundVisibilityChanged(){
                                background_not_visible.checked = (playerController.backgroundVisibility() === 0)
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
            id: eofMenuButton
            implicitWidth: 100

            text: playerController.rewindMediaOnEOF() ? qsTr("EOF: Stop") : qsTr("EOF: Pause")
            icon.name: playerController.rewindMediaOnEOF() ? "media-playback-stop" : "media-playback-pause"
            focusPolicy: Qt.NoFocus

            onClicked: {
                eofMenu.visible = !eofMenu.visible
            }

            ToolTip {
                text: "\"End of file\" mode for current media."
            }

            Connections {
                target: mpv
                function onFileLoaded() {
                    const eofMode = mpv.playlistModel.eofMode(mpv.playlistModel.getPlayingVideo())
                    if(eofMode===1 && playList.playlistView.count > 1){ //Continue
                        mpv.eofMode = 1;
                        eofMenuButton.text = qsTr("EOF: Next ")
                        eofMenuButton.icon.name = "go-next"
                    }
                    else if(eofMode===2){ //Loop
                        mpv.eofMode = 2;
                        eofMenuButton.text = qsTr("EOF: Loop ")
                        eofMenuButton.icon.name = "media-playlist-repeat"
                    }
                    else { //Pause
                        mpv.eofMode = 0;

                        if(playerController.rewindMediaOnEOF()){
                            eofMenuButton.text = qsTr("EOF: Stop")
                            eofMenuButton.icon.name = "media-playback-stop"
                        }
                        else {
                            eofMenuButton.text = qsTr("EOF: Pause")
                            eofMenuButton.icon.name = "media-playback-pause"
                        }
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
                        text: qsTr("EOF: Pause (Or Stop, see below)")
                        onClicked: {
                            mpv.eofMode = 0;
                            if(playerController.rewindMediaOnEOF()){
                                eofMenuButton.text = qsTr("EOF: Stop")
                                eofMenuButton.icon.name = "media-playback-stop"
                            }
                            else {
                                eofMenuButton.text = qsTr("EOF: Pause")
                                eofMenuButton.icon.name = "media-playback-pause"
                            }
                        }
                        Connections {
                            target: mpv
                            function onFileLoaded() {
                                eof_pause.checked = (mpv.eofMode === 0)
                            }
                        }
                    }

                    RadioButton {
                        id: eof_next
                        checked: false
                        text: qsTr("EOF: Next ")
                        enabled: (playList.playlistView.count > 1)
                        onClicked: {
                           mpv.eofMode = 1;
                           eofMenuButton.text = qsTr("EOF: Next")
                           eofMenuButton.icon.name = "go-next"
                        }
                        Connections {
                            target: mpv
                            function onFileLoaded() {
                                eof_next.checked = (mpv.eofMode === 1)
                            }
                        }
                    }

                    RadioButton {
                        id: eof_loop
                        checked: false
                        text: qsTr("EOF: Loop ")
                        onClicked: {
                            mpv.eofMode = 2;
                            eofMenuButton.text = qsTr("EOF: Loop")
                            eofMenuButton.icon.name = "media-playlist-repeat"
                        }
                        Connections {
                            target: mpv
                            function onFileLoaded() {
                                eof_loop.checked = (mpv.eofMode === 2)
                            }
                        }
                    }
                }

                MenuSeparator {}

                Column {
                    id: columeFadeOnPauseEOF

                    RadioButton {
                        id: do_not_rewind_on_eof
                        checked: !playerController.rewindMediaOnEOF()
                        text: qsTr("EOF-Pause: Only pause media.")
                        onClicked: {
                            if(checked){
                                playerController.setRewindMediaOnEOF(false);
                                if(mpv.eofMode === 0){
                                    eofMenuButton.text = qsTr("EOF: Pause")
                                    eofMenuButton.icon.name = "media-playback-pause"
                                }
                            }
                        }
                        Connections {
                            target: playerController
                            function onRewindMediaOnEOFChanged(){
                                do_not_rewind_on_eof.checked = !playerController.rewindMediaOnEOF()
                            }
                        }
                    }

                    RadioButton {
                        id: do_rewind_on_eof
                        checked: playerController.rewindMediaOnEOF()
                        text: qsTr("EOF-Pause: Stop/rewind instead of pause.")
                        onClicked: {
                            if(checked){
                                playerController.setRewindMediaOnEOF(true);
                                if(mpv.eofMode === 0){
                                    eofMenuButton.text = qsTr("EOF: Stop")
                                    eofMenuButton.icon.name = "media-playback-stop"
                                }
                            }
                        }
                        Connections {
                            target: playerController
                            function onRewindMediaOnEOFChanged(){
                                do_rewind_on_eof.checked = playerController.rewindMediaOnEOF()
                            }
                        }
                    }
                }
            }
        }

        ToolButton {
            id: stereoscopicMenuButton
            implicitWidth: 100

            ToolTip {
                text: "Stereoscopic mode of the current media."
            }

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
                    if(mpv.stereoscopicMode > 0){
                        if(playerController.getViewModeOnClients() > 0){
                            stereoscopicMenuButton.text += qsTr("->2D")
                            stereoscopicMenuButton.icon.name = "redeyes"
                        }
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
                        checked: false
                        text: qsTr("2D Mono")
                        onClicked: {
                            mpv.stereoscopicMode = 0
                            mpv.playSectionsModel.currentEditItemIsEdited = true
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
                        checked: false
                        text: qsTr("3D Side-by-side")
                        onClicked: {
                             mpv.stereoscopicMode = 1
                            mpv.playSectionsModel.currentEditItemIsEdited = true
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
                        checked: false
                        text: qsTr("3D Top/Bottom")
                        onClicked: {
                            mpv.stereoscopicMode = 2
                            mpv.playSectionsModel.currentEditItemIsEdited = true
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
                        checked: false
                        text: qsTr("3D Top/Bottom+Flip")
                        onClicked: {
                            mpv.stereoscopicMode = 3
                            mpv.playSectionsModel.currentEditItemIsEdited = true
                        }
                        Connections {
                            target: mpv
                            function onStereoscopicModeChanged(){
                                stereoscopic_3D_tbf.checked = (mpv.stereoscopicMode === 3)
                            }
                        }
                    }
                }

                MenuSeparator {}

                Column {
                    id: columnViewOnClients

                    RadioButton {
                        id: clients_auto_2D_3D_switch
                        checked: true
                        text: qsTr("On clients: Auto 2D / 3D switch")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                playerController.setViewModeOnClients(0);
                                mpv.stereoscopicModeChanged()
                            }
                        }
                        Connections {
                            target: playerController
                            function onViewModeOnClientsChanged(){
                                clients_auto_2D_3D_switch.checked = (playerController.getViewModeOnClients() === 0)
                                mpv.stereoscopicModeChanged()
                            }
                        }
                    }

                    RadioButton {
                        id: clients_force_2D
                        checked: false
                        text: qsTr("On clients: Force all to 2D")
                        onClicked: {
                        }
                        onCheckedChanged: {
                            if(checked){
                                playerController.setViewModeOnClients(1);
                                mpv.stereoscopicModeChanged()
                            }
                        }
                        Connections {
                            target: playerController
                            function onViewModeOnClientsChanged(){
                                clients_force_2D.checked = (playerController.getViewModeOnClients() === 1)
                                mpv.stereoscopicModeChanged()
                            }
                        }
                    }
                }
            }
        }

        ToolButton {
            id: gridMenuButton
            implicitWidth: 130

            ToolTip {
                text: "Mapping/grid mode of the current media."
            }

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
                        checked: false
                        text: qsTr("None")
                        onClicked: {
                            mpv.gridToMapOn = 0
                            mpv.playSectionsModel.currentEditItemIsEdited = true
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
                        checked: false
                        text: qsTr("Plane")
                        onClicked: {
                             mpv.gridToMapOn = 1
                            mpv.playSectionsModel.currentEditItemIsEdited = true
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
                        checked: false
                        text: qsTr("Dome")
                        onClicked: {
                             mpv.gridToMapOn = 2
                            mpv.playSectionsModel.currentEditItemIsEdited = true
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
                        checked: false
                        text: qsTr("Sphere EQR")
                        onClicked: {
                            mpv.gridToMapOn = 3
                            mpv.playSectionsModel.currentEditItemIsEdited = true
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
                        checked: false
                        text: qsTr("Sphere EAC")
                        onClicked: {
                            mpv.gridToMapOn = 4
                            mpv.playSectionsModel.currentEditItemIsEdited = true
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

            ToolTip {
                text: "Spin and translate the mapping grid."
            }

            text: {
                qsTr("Spin && Move")
            }
            icon.name: "hand"
            icon.color: (!spinPitchUp.checked & !spinPitchDown.checked
                         & !spinYawLeft.checked & !spinYawRight.checked
                         & !spinRollClockwise.checked & !spinRollCounterClockwise.checked) ? "crimson" : "lime"
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
                            icon.color: spinPitchUp.checked ? "lime" : "crimson"
                            checkable: true
                            checked: false
                            onCheckedChanged: {
                                if(spinPitchUp.checked){
                                    if(spinPitchDown.checked){
                                        spinPitchDown.checked = false
                                    }
                                    else if(!spinTimerPitch.running)
                                        spinTimerPitch.start()
                                }
                                else if(!spinPitchDown.checked){
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
                            icon.color: spinPitchDown.checked ? "lime" : "crimson"
                            checkable: true
                            checked: false
                            onCheckedChanged: {
                                if(spinPitchDown.checked){
                                    if(spinPitchUp.checked){
                                        spinPitchUp.checked = false
                                    }
                                    else if(!spinTimerPitch.running)
                                        spinTimerPitch.start()
                                }
                                else if(!spinPitchUp.checked){
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
                            icon.color: spinYawLeft.checked ? "lime" : "crimson"
                            checkable: true
                            checked: false
                            onCheckedChanged: {
                                if(spinYawLeft.checked){
                                    if(spinYawRight.checked){
                                        spinYawRight.checked = false
                                    }
                                    else if(!spinTimerYaw.running)
                                        spinTimerYaw.start()
                                }
                                else if(!spinYawRight.checked){
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
                            icon.color: spinYawRight.checked ? "lime" : "crimson"
                            checkable: true
                            checked: false
                            onCheckedChanged: {
                                if(spinYawRight.checked){
                                    if(spinYawLeft.checked){
                                        spinYawLeft.checked = false
                                    }
                                    else if(!spinTimerYaw.running)
                                        spinTimerYaw.start()
                                }
                                else if(!spinYawLeft.checked){
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
                            icon.color: spinRollCounterClockwise.checked ? "lime" : "crimson"
                            checkable: true
                            checked: false
                            onCheckedChanged: {
                                if(spinRollCounterClockwise.checked){
                                    if(spinRollClockwise.checked){
                                        spinRollClockwise.checked = false
                                    }
                                    else if(!spinTimerYaw.running)
                                        spinTimerRoll.start()
                                }
                                else if(!spinRollClockwise.checked){
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
                            icon.color: spinRollClockwise.checked ? "lime" : "crimson"
                            checkable: true
                            checked: false
                            onCheckedChanged: {
                                if(spinRollClockwise.checked){
                                    if(spinRollCounterClockwise.checked){
                                        spinRollCounterClockwise.checked = false
                                    }
                                    else if(!spinTimerYaw.running)
                                        spinTimerRoll.start()
                                }
                                else if(!spinRollCounterClockwise.checked){
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
                        id: transitionHeaderButton

                        text: {
                            qsTr("Move between scenarios")
                        }

                        enabled: !mpv.surfaceTransitionOnGoing
                        onEnabledChanged: {
                            if(enabled){
                                transitionHeaderButton.text = qsTr("Move between scenarios")
                            }
                            else{
                                transitionHeaderButton.text = qsTr("Move/transition ongoing")
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

                            mpv.performSurfaceTransition();
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

                    function onSpinPitchUp(run) {
                        if(mpv.gridToMapOn >= 3) {
                            spinPitchUp.checked = run
                        }
                    }
                    function onSpinPitchDown(run) {
                        if(mpv.gridToMapOn >= 3) {
                            spinPitchDown.checked = run
                        }
                    }
                    function onSpinYawLeft(run) {
                        if(mpv.gridToMapOn >= 2) {
                            spinYawLeft.checked = run
                        }
                    }
                    function onSpinYawRight(run) {
                        if(mpv.gridToMapOn >= 2) {
                            spinYawRight.checked = run
                        }
                    }
                    function onSpinRollCCW(run) {
                        if(mpv.gridToMapOn >= 3) {
                            spinRollCounterClockwise.checked = run
                        }
                    }
                    function onSpinRollCW(run) {
                        if(mpv.gridToMapOn >= 3) {
                            spinRollClockwise.checked = run
                        }
                    }
                    function onOrientationAndSpinReset() {
                        resetOrientation.clicked()
                    }
                    function onRunSurfaceTransition() {
                        if(!mpv.surfaceTransitionOnGoing){
                            transitionHeaderButton.clicked()
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
            id: sync
            action: actions.syncAction
            text: actions.syncAction.text
            focusPolicy: Qt.NoFocus
            icon.color: mpv.autoPlay ? "lime" : "crimson"
            ToolTip {
                text: "ON/OFF to sync state from master to clients."
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
