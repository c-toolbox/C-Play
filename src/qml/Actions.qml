/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Controls 2.12

import org.ctoolbox.cplay 1.0

QtObject {
    id: root

    property var list: ({})

    property Action openContextMenuAction: Action {
        id: openContextMenuAction
        property var qaction: app.action("openContextMenu")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["openContextMenuAction"] = openContextMenuAction

        onTriggered: mpvContextMenu.popup()
    }

    property Action toggleSectionsAction: Action {
        id: toggleSectionsAction
        property var qaction: app.action("toggleSections")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["toggleSectionsAction"] = toggleSectionsAction

        onTriggered: {
            if(playSections.state === "hidden" && playList.state === "hidden"){
                playSections.state = "visible-without-partner"
            }
            else if(playSections.state === "hidden" && playList.state === "visible-without-partner"){
                playList.state = "visible-with-partner"
                playSections.state = "visible-with-partner"
            }
            else if(playList.state === "visible-with-partner") {
                playList.state = "visible-without-partner"
                playSections.state = "hidden"
            }
            else {
                playSections.state = "hidden"
            }
        }
    }

    property Action togglePlaylistAction: Action {
        id: togglePlaylistAction
        property var qaction: app.action("togglePlaylist")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["togglePlaylistAction"] = togglePlaylistAction

        onTriggered: {
            if(playList.state === "hidden" && playSections.state === "hidden"){
                playList.state = "visible-without-partner"
            }
            else if(playList.state === "hidden" && playSections.state === "visible-without-partner"){
                playList.state = "visible-with-partner"
                playSections.state = "visible-with-partner"
            }
            else if(playSections.state === "visible-with-partner") {
                playSections.state = "visible-without-partner"
                playList.state = "hidden"
            }
            else {
                playList.state = "hidden"
            }
        }
    }

    property Action volumeUpAction: Action {
        id: volumeUpAction
        property var qaction: app.action("volumeUp")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["volumeUpAction"] = volumeUpAction

        onTriggered: {
            mpv.command(["add", "volume", GeneralSettings.volumeStep])
            osd.message(`Volume: ${parseInt(mpv.getProperty("volume"))}`)
        }
    }

    property Action volumeDownAction: Action {
        id: volumeDownAction
        property var qaction: app.action("volumeDown")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["volumeDownAction"] = volumeDownAction

        onTriggered: {
            mpv.command(["add", "volume", -GeneralSettings.volumeStep])
            osd.message(`Volume: ${parseInt(mpv.getProperty("volume"))}`)
        }
    }

    property Action syncAction: Action {
        id: syncAction
        property var qaction: app.action("sync-video")
        text: qaction.text
        shortcut: qaction.shortcutName()
        Component.onCompleted: list["syncAction"] = syncAction
        icon.name: "im-user-online"
        onTriggered: {
            mpv.syncVideo = !mpv.syncVideo
            if (mpv.syncVideo) {
                text = qsTr("Sync is On")
                icon.name = "im-user-online"
            } else {
                text = qsTr("Sync is Off")
                icon.name = "im-user-offline"
            }
        }
    }

    property Action muteAction: Action {
        id: muteAction
        property var qaction: app.action("mute")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["muteAction"] = muteAction

        onTriggered: {
            mpv.setProperty("mute", !mpv.getProperty("mute"))
            if (mpv.getProperty("mute")) {
                text = qsTr("Unmute")
                icon.name = "player-volume-muted"
            } else {
                text = qaction.text
                icon.name = qaction.iconName()
            }
        }
    }

    property Action playNextAction: Action {
        id: playNextAction
        property var qaction: app.action("playNext")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["playNextAction"] = playNextAction

        onTriggered: {
            const nextFileRow = mpv.playlistModel.getPlayingVideo() + 1
            const updateLastPlayedFile = !playList.isYouTubePlaylist
            if (nextFileRow < playList.playlistView.count) {
                mpv.pause = true
                mpv.position = 0
                mpv.playlistModel.setPlayingVideo(nextFileRow)
                mpv.loadItem(nextFileRow)
            } else {
                // Last file in playlist
                if (PlaylistSettings.repeat) {
                    mpv.pause = true
                    mpv.position = 0
                    mpv.playlistModel.setPlayingVideo(0)
                    mpv.loadItem(0)
                }
            }
        }
    }

    property Action playPreviousAction: Action {
        id: playPreviousAction
        property var qaction: app.action("playPrevious")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["playPreviousAction"] = playPreviousAction

        onTriggered: {
            if (mpv.playlistModel.getPlayingVideo() !== 0) {
                const previousFileRow = mpv.playlistModel.getPlayingVideo() - 1
                const updateLastPlayedFile = !playList.isYouTubePlaylist
                mpv.pause = true
                mpv.position = 0
                mpv.playlistModel.setPlayingVideo(previousFileRow)
                mpv.loadItem(previousFileRow)
            }
        }
    }

    property Action openAction: Action {
        id: openAction
        property var qaction: app.action("openFile")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["openAction"] = openAction

        onTriggered: openFileDialog.open()
    }

    property Action openUrlAction: Action {
        id: openUrlAction
        property var qaction: app.action("openUrl")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["openUrlAction"] = openUrlAction

        onTriggered: {
            if (openUrlPopup.visible) {
                openUrlPopup.close()
            } else {
                openUrlPopup.open()
            }
        }
    }

    property Action saveAsCPlayFileAction: Action {
        id: saveAsCPlayFileAction
        property var qaction: app.action("saveAsCPlayFile")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()
        icon.color: mpv.playSectionsModel.currentEditItemIsEdited ? "orange" : "lime"
        Component.onCompleted: list["saveAsCPlayFileAction"] = saveAsCPlayFileAction
        enabled: false
        onTriggered: {
            saveCPlayFileDialog.currentFile = mpv.playSectionsModel.getSuggestedFileURL()
            mpv.setLoadedAsCurrentEditItem()
            if(!mpv.playSectionsModel.isEmpty())
                saveAsCPlayFileWindow.visible = true
        }
    }

    property Action aboutCPlayAction: Action {
        id: aboutCPlayAction
        property var qaction: app.action("aboutCPlay")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["aboutCPlayAction"] = aboutCPlayAction

        onTriggered: qaction.trigger()
    }

    property Action seekForwardSmallAction: Action {
        id: seekForwardSmallAction
        property var qaction: app.action("seekForwardSmall")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekForwardSmallAction"] = seekForwardSmallAction

        onTriggered: mpv.seek(GeneralSettings.seekSmallStep)
    }

    property Action seekBackwardSmallAction: Action {
        id: seekBackwardSmallAction
        property var qaction: app.action("seekBackwardSmall")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekBackwardSmallAction"] = seekBackwardSmallAction

        onTriggered: mpv.seek(-GeneralSettings.seekSmallStep)
    }

    property Action seekForwardMediumAction: Action {
        id: seekForwardMediumAction
        property var qaction: app.action("seekForwardMedium")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekForwardMediumAction"] = seekForwardMediumAction

        onTriggered: mpv.seek(GeneralSettings.seekMediumStep)
    }

    property Action seekBackwardMediumAction: Action {
        id: seekBackwardMediumAction
        property var qaction: app.action("seekBackwardMedium")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekBackwardMediumAction"] = seekBackwardMediumAction

        onTriggered: mpv.seek(-GeneralSettings.seekMediumStep)
    }

    property Action seekForwardBigAction: Action {
        id: seekForwardBigAction
        property var qaction: app.action("seekForwardBig")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekForwardBigAction"] = seekForwardBigAction

        onTriggered: mpv.seek(GeneralSettings.seekBigStep)
    }

    property Action seekBackwardBigAction: Action {
        id: seekBackwardBigAction
        property var qaction: app.action("seekBackwardBig")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekBackwardBigAction"] = seekBackwardBigAction

        onTriggered: mpv.seek(-GeneralSettings.seekBigStep)
    }

    property Action playPauseAction: Action {
        id: playPauseAction
        text: qsTr("Play/Pause")
        icon.name: "media-playback-pause"
        shortcut: "Space"

        Component.onCompleted: list["playPauseAction"] = playPauseAction

        onTriggered: mpv.togglePlayPause()
    }

    property Action configureShortcutsAction: Action {
        id: configureShortcutsAction
        property var qaction: app.action("options_configure_keybinding")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["configureShortcutsAction"] = configureShortcutsAction

        onTriggered: qaction.trigger()
    }

    property Action quitApplicationAction: Action {
        id: quitApplicationAction
        property var qaction: app.action("file_quit")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: {
            list["quitApplicationAction"] = quitApplicationAction
        }

        onTriggered: {
            mpv.handleTimePosition()
            qaction.trigger()
        }
    }

    property Action configureAction: Action {
        id: configureAction
        property var qaction: app.action("configure")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["configureAction"] = configureAction

        onTriggered: {
            settingsEditor.visible = true
        }
    }

    property Action audioCycleUpAction: Action {
        id: audioCycleUpAction
        property var qaction: app.action("audioCycleUp")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["audioCycleUpAction"] = audioCycleUpAction

        onTriggered: {
            const tracks = mpv.getProperty("track-list")
            let audioTracksCount = 0
            tracks.forEach(t => { if(t.type === "audio") ++audioTracksCount })

            if (audioTracksCount > 1) {
                mpv.command(["cycle", "aid", "up"])
                const currentTrackId = mpv.getProperty("aid")

                if (currentTrackId === false) {
                    audioCycleUpAction.trigger()
                    return
                }
                const track = tracks.find(t => t.type === "audio" && t.id === currentTrackId)
                const message = `Audio: ${currentTrackId} (${track.lang})`
                osd.message(message)
            }
        }
    }

    property Action audioCycleDownAction: Action {
        id: audioCycleDownAction
        property var qaction: app.action("audioCycleDown")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["audioCycleDownAction"] = audioCycleDownAction

        onTriggered: {
            const tracks = mpv.getProperty("track-list")
            let audioTracksCount = 0
            tracks.forEach(t => { if(t.type === "audio") ++audioTracksCount })

            if (audioTracksCount > 1) {
                mpv.command(["cycle", "aid", "down"])
                const currentTrackId = mpv.getProperty("aid")

                if (currentTrackId === false) {
                    audioCycleDownAction.trigger()
                    return
                }
                const track = tracks.find(t => t.type === "audio" && t.id === currentTrackId)
                const message = `Audio: ${currentTrackId} (${track.lang})`
                osd.message(message)
            }
        }
    }



    property Action contrastUpAction: Action {
        id: contrastUpAction
        property var qaction: app.action("contrastUp")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["contrastUpAction"] = contrastUpAction

        onTriggered: {
            const contrast = parseInt(mpv.getProperty("contrast")) + 1
            mpv.setProperty("contrast", `${contrast}`)
            osd.message(`Contrast: ${contrast}`)
        }
    }
    property Action contrastDownAction: Action {
        id: contrastDownAction
        property var qaction: app.action("contrastDown")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["contrastDownAction"] = contrastDownAction

        onTriggered: {
            const contrast = parseInt(mpv.getProperty("contrast")) - 1
            mpv.setProperty("contrast", `${contrast}`)
            osd.message(`Contrast: ${contrast}`)
        }
    }

    property Action contrastResetAction: Action {
        id: contrastResetAction
        property var qaction: app.action("contrastReset")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["contrastResetAction"] = contrastResetAction

        onTriggered: {
            mpv.setProperty("contrast", `0`)
            osd.message(`Contrast: 0`)
        }
    }

    property Action brightnessUpAction: Action {
        id: brightnessUpAction
        property var qaction: app.action("brightnessUp")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["brightnessUpAction"] = brightnessUpAction

        onTriggered: {
            const brightness = parseInt(mpv.getProperty("brightness")) + 1
            mpv.setProperty("brightness", `${brightness}`)
            osd.message(`Brightness: ${brightness}`)
        }
    }
    property Action brightnessDownAction: Action {
        id: brightnessDownAction
        property var qaction: app.action("brightnessDown")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["brightnessDownAction"] = brightnessDownAction

        onTriggered: {
            const brightness = parseInt(mpv.getProperty("brightness")) - 1
            mpv.setProperty("brightness", `${brightness}`)
            osd.message(`Brightness: ${brightness}`)
        }
    }
    property Action brightnessResetAction: Action {
        id: brightnessResetAction
        property var qaction: app.action("brightnessReset")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["brightnessResetAction"] = brightnessResetAction

        onTriggered: {
            mpv.setProperty("brightness", `0`)
            osd.message(`Brightness: 0`)
        }
    }
    property Action gammaUpAction: Action {
        id: gammaUpAction
        property var qaction: app.action("gammaUp")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["gammaUpAction"] = gammaUpAction

        onTriggered: {
            const gamma = parseInt(mpv.getProperty("gamma")) + 1
            mpv.setProperty("gamma", `${gamma}`)
            osd.message(`Gamma: ${gamma}`)
        }
    }
    property Action gammaDownAction: Action {
        id: gammaDownAction
        property var qaction: app.action("gammaDown")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["gammaDownAction"] = gammaDownAction

        onTriggered: {
            const gamma = parseInt(mpv.getProperty("gamma")) - 1
            mpv.setProperty("gamma", `${gamma}`)
            osd.message(`Gamma: ${gamma}`)
        }
    }
    property Action gammaResetAction: Action {
        id: gammaResetAction
        property var qaction: app.action("gammaReset")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["gammaResetAction"] = gammaResetAction

        onTriggered: {
            mpv.setProperty("gamma", `0`)
            osd.message(`Gamma: 0`)
        }
    }
    property Action saturationUpAction: Action {
        id: saturationUpAction
        property var qaction: app.action("saturationUp")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["saturationUpAction"] = saturationUpAction

        onTriggered: {
            const saturation = parseInt(mpv.getProperty("saturation")) + 1
            mpv.setProperty("saturation", `${saturation}`)
            osd.message(`Saturation: ${saturation}`)
        }
    }
    property Action saturationDownAction: Action {
        id: saturationDownAction
        property var qaction: app.action("saturationDown")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["saturationDownAction"] = saturationDownAction

        onTriggered: {
            const saturation = parseInt(mpv.getProperty("saturation")) - 1
            mpv.setProperty("saturation", `${saturation}`)
            osd.message(`Saturation: ${saturation}`)
        }
    }
    property Action saturationResetAction: Action {
        id: saturationResetAction
        property var qaction: app.action("saturationReset")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["saturationResetAction"] = saturationResetAction

        onTriggered: {
            mpv.setProperty("saturation", `0`)
            osd.message(`Saturation: 0`)
        }
    }

    property Action zoomInAction: Action {
        id: zoomInAction
        property var qaction: app.action("zoomIn")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["zoomInAction"] = zoomInAction

        onTriggered: {
            const zoom = mpv.getProperty("video-zoom") + 0.1
            mpv.setProperty("video-zoom", zoom)
            osd.message(`Zoom: ${zoom.toFixed(2)}`)
        }
    }

    property Action zoomOutAction: Action {
        id: zoomOutAction
        property var qaction: app.action("zoomOut")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["zoomOutAction"] = zoomOutAction

        onTriggered: {
            const zoom = mpv.getProperty("video-zoom") - 0.1
            mpv.setProperty("video-zoom", zoom)
            osd.message(`Zoom: ${zoom.toFixed(2)}`)
        }
    }
    property Action zoomResetAction: Action {
        id: zoomResetAction
        property var qaction: app.action("zoomReset")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["zoomResetAction"] = zoomResetAction

        onTriggered: {
            mpv.setProperty("video-zoom", 0)
            osd.message(`Zoom: 0`)
        }
    }


    property Action videoPanXLeftAction: Action {
        id: videoPanXLeftAction
        property var qaction: app.action("videoPanXLeft")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["videoPanXLeftAction"] = videoPanXLeftAction

        onTriggered: {
            const pan = mpv.getProperty("video-pan-x") - 0.01
            mpv.setProperty("video-pan-x", pan)
            osd.message(`Video pan x: ${pan.toFixed(2)}`)
        }
    }
    property Action videoPanXRightAction: Action {
        id: videoPanXRightAction
        property var qaction: app.action("videoPanXRight")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["videoPanXRightAction"] = videoPanXRightAction

        onTriggered: {
            const pan = mpv.getProperty("video-pan-x") + 0.01
            mpv.setProperty("video-pan-x", pan)
            osd.message(`Video pan x: ${pan.toFixed(2)}`)
        }
    }
    property Action videoPanYUpAction: Action {
        id: videoPanYUpAction
        property var qaction: app.action("videoPanYUp")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["videoPanYUpAction"] = videoPanYUpAction

        onTriggered: {
            const pan = mpv.getProperty("video-pan-y") - 0.01
            mpv.setProperty("video-pan-y", pan)
            osd.message(`Video pan x: ${pan.toFixed(2)}`)
        }
    }
    property Action videoPanYDownAction: Action {
        id: videoPanYDownAction
        property var qaction: app.action("videoPanYDown")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["videoPanYDownAction"] = videoPanYDownAction

        onTriggered: {
            const pan = mpv.getProperty("video-pan-y") + 0.01
            mpv.setProperty("video-pan-y", pan)
            osd.message(`Video pan x: ${pan.toFixed(2)}`)
        }
    }

    property Action toggleFullscreenAction: Action {
        id: toggleFullscreenAction
        property var qaction: app.action("toggleFullscreen")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["toggleFullscreenAction"] = toggleFullscreenAction

        onTriggered: {
            window.toggleFullScreen()
        }
    }

    property Action toggleMenuBarAction: Action {
        id: toggleMenuBarAction
        property var qaction: app.action("toggleMenuBar")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["toggleMenuBarAction"] = toggleMenuBarAction

        onTriggered: GeneralSettings.showMenuBar = !menuBar.visible
    }

    property Action toggleHeaderAction: Action {
        id: toggleHeaderAction
        property var qaction: app.action("toggleHeader")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["toggleHeaderAction"] = toggleHeaderAction

        onTriggered: GeneralSettings.showHeader = !header.visible
    }

    property Action screenshotAction: Action {
        id: screenshotAction
        property var qaction: app.action("screenshot")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["screenshotAction"] = screenshotAction

        onTriggered: mpv.command(["screenshot"])
    }

    property Action setLoopAction: Action {
        id: setLoopAction
        property var qaction: app.action("setLoop")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["setLoopAction"] = setLoopAction

        onTriggered: {
            var a = mpv.getProperty("ab-loop-a")
            var b = mpv.getProperty("ab-loop-b")

            var aIsSet = a !== "no"
            var bIsSet = b !== "no"

            if (!aIsSet && !bIsSet) {
                mpv.setProperty("ab-loop-a", mpv.position)
                footer.progressBar.loopIndicator.startPosition = mpv.position
                osd.message("Loop start: " + app.formatTime(mpv.position))
            } else if (aIsSet && !bIsSet) {
                mpv.setProperty("ab-loop-b", mpv.position)
                footer.progressBar.loopIndicator.endPosition = mpv.position
                osd.message(`Loop: ${app.formatTime(a)} - ${app.formatTime(mpv.position)}`)
            } else if (aIsSet && bIsSet) {
                mpv.setProperty("ab-loop-a", "no")
                mpv.setProperty("ab-loop-b", "no")
                footer.progressBar.loopIndicator.startPosition = -1
                footer.progressBar.loopIndicator.endPosition = -1
                osd.message("Loop cleared")
            }
        }
    }

    property Action toggleDeinterlacingAction: Action {
        id: toggleDeinterlacingAction
        property var qaction: app.action("toggleDeinterlacing")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["toggleDeinterlacingAction"] = toggleDeinterlacingAction

        onTriggered: {
            mpv.setProperty("deinterlace", !mpv.getProperty("deinterlace"))
            osd.message(`Deinterlace: ${mpv.getProperty("deinterlace")}`)
        }
    }

    property Action clearRecentMediaFilesAction: Action {
        id: clearRecentMediaFilesAction
        property var qaction: app.action("clearRecentMediaFiles")
        text: qaction.text

        Component.onCompleted: list["clearRecentMediaFilesAction"] = clearRecentMediaFilesAction

        onTriggered: {
            mpv.clearRecentMediaFilelist()
        }
    }

    property Action clearRecentPlaylistsAction: Action {
        id: clearRecentPlaylistsAction
        property var qaction: app.action("clearRecentPlaylists")
        text: qaction.text

        Component.onCompleted: list["clearRecentPlaylistsAction"] = clearRecentPlaylistsAction

        onTriggered: {
            mpv.clearRecentPlaylist()
        }
    }
}
