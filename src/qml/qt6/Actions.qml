/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls

import org.ctoolbox.cplay

QtObject {
    id: root

    property var list: ({})

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
            mpv.command(["add", "volume", AudioSettings.volumeStep])
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
            mpv.command(["add", "volume", -AudioSettings.volumeStep])
            osd.message(`Volume: ${parseInt(mpv.getProperty("volume"))}`)
        }
    }

    property Action volumeFadeUpAction: Action {
        id: volumeFadeUpAction
        property var qaction: app.action("volumeFadeUp")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["volumeFadeUpAction"] = volumeFadeUpAction

        onTriggered: {
            mpv.fadeVolumeUp()
        }
    }

    property Action volumeFadeDownAction: Action {
        id: volumeFadeDownAction
        property var qaction: app.action("volumeFadeDown")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["volumeFadeDownAction"] = volumeFadeDownAction

        onTriggered: {
            mpv.fadeVolumeDown()
        }
    }

    property Action syncAction: Action {
        id: syncAction
        text: qsTr("Sync On")
        icon.name: "im-user-online"
        onTriggered: {
            mpv.syncVideo = !mpv.syncVideo
            if (mpv.syncVideo) {
                text = qsTr("Sync On")
                icon.name = "im-user-online"
            } else {
                text = qsTr("Sync Off")
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

    property Action visibilityFadeUpAction: Action {
        id: visibilityFadeUpAction
        property var qaction: app.action("visibilityFadeUp")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["visibilityFadeUpAction"] = visibilityFadeUpAction

        onTriggered: {
            mpv.fadeImageUp()
        }
    }

    property Action visibilityFadeDownAction: Action {
        id: visibilityFadeDownAction
        property var qaction: app.action("visibilityFadeDown")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["visibilityFadeDownAction"] = visibilityFadeDownAction

        onTriggered: {
            mpv.fadeImageDown()
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

        onTriggered: {
            app.updateAboutOtherText(mpv.getProperty("mpv-version"), mpv.getProperty("ffmpeg-version"));
            qaction.trigger();
        }
    }

    property Action seekForwardSmallAction: Action {
        id: seekForwardSmallAction
        property var qaction: app.action("seekForwardSmall")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekForwardSmallAction"] = seekForwardSmallAction

        onTriggered: mpv.seek(PlaybackSettings.seekSmallStep)
    }

    property Action seekBackwardSmallAction: Action {
        id: seekBackwardSmallAction
        property var qaction: app.action("seekBackwardSmall")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekBackwardSmallAction"] = seekBackwardSmallAction

        onTriggered: mpv.seek(-PlaybackSettings.seekSmallStep)
    }

    property Action seekForwardMediumAction: Action {
        id: seekForwardMediumAction
        property var qaction: app.action("seekForwardMedium")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekForwardMediumAction"] = seekForwardMediumAction

        onTriggered: mpv.seek(PlaybackSettings.seekMediumStep)
    }

    property Action seekBackwardMediumAction: Action {
        id: seekBackwardMediumAction
        property var qaction: app.action("seekBackwardMedium")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekBackwardMediumAction"] = seekBackwardMediumAction

        onTriggered: mpv.seek(-PlaybackSettings.seekMediumStep)
    }

    property Action seekForwardBigAction: Action {
        id: seekForwardBigAction
        property var qaction: app.action("seekForwardBig")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekForwardBigAction"] = seekForwardBigAction

        onTriggered: mpv.seek(PlaybackSettings.seekBigStep)
    }

    property Action seekBackwardBigAction: Action {
        id: seekBackwardBigAction
        property var qaction: app.action("seekBackwardBig")
        text: qaction.text
        shortcut: qaction.shortcutName()
        icon.name: qaction.iconName()

        Component.onCompleted: list["seekBackwardBigAction"] = seekBackwardBigAction

        onTriggered: mpv.seek(-PlaybackSettings.seekBigStep)
    }

    property Action playPauseAction: Action {
        id: playPauseAction
        property var qaction: app.action("play_pause")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["playPauseAction"] = playPauseAction

        onTriggered: mpv.togglePlayPause()
    }

    property Action stopRewindAction: Action {
        id: stopRewindAction
        property var qaction: app.action("stop_rewind")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["stopRewindAction"] = stopRewindAction

        onTriggered: mpv.performRewind()
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

    property Action toggleMenuBarAction: Action {
        id: toggleMenuBarAction
        property var qaction: app.action("toggleMenuBar")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["toggleMenuBarAction"] = toggleMenuBarAction

        onTriggered: UserInterfaceSettings.showMenuBar = !menuBar.visible
    }

    property Action toggleHeaderAction: Action {
        id: toggleHeaderAction
        property var qaction: app.action("toggleHeader")
        text: qaction.text
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()

        Component.onCompleted: list["toggleHeaderAction"] = toggleHeaderAction

        onTriggered: UserInterfaceSettings.showHeader = !header.visible
    }

    property Action clearRecentMediaFilesAction: Action {
        id: clearRecentMediaFilesAction
        text: qsTr("Clear Recent Media Files")
        onTriggered: {
            mpv.clearRecentMediaFilelist()
        }
    }

    property Action clearRecentPlaylistsAction: Action {
        id: clearRecentPlaylistsAction
        text: qsTr("Clear Recent Playlists")
        onTriggered: {
            mpv.clearRecentPlaylist()
        }
    }
}
