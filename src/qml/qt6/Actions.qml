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

    property Action aboutCPlayAction: Action {
        id: aboutCPlayAction

        property var qaction: app.action("aboutCPlay")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["aboutCPlayAction"] = aboutCPlayAction
        onTriggered: {
            app.updateAboutOtherText(mpv.getProperty("mpv-version"), mpv.getProperty("ffmpeg-version"));
            qaction.trigger();
        }
    }
    property Action clearRecentMediaFilesAction: Action {
        id: clearRecentMediaFilesAction

        text: qsTr("Clear Recent Media Files")

        onTriggered: {
            mpv.clearRecentMediaFilelist();
        }
    }
    property Action clearRecentPlaylistsAction: Action {
        id: clearRecentPlaylistsAction

        text: qsTr("Clear Recent Playlists")

        onTriggered: {
            mpv.clearRecentPlaylist();
        }
    }
    property Action configureAction: Action {
        id: configureAction

        property var qaction: app.action("configure")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["configureAction"] = configureAction
        onTriggered: {
            settingsEditor.visible = true;
        }
    }
    property Action configureShortcutsAction: Action {
        id: configureShortcutsAction

        property var qaction: app.action("options_configure_keybinding")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["configureShortcutsAction"] = configureShortcutsAction
        onTriggered: qaction.trigger()
    }
    property var list: ({})
    property Action muteAction: Action {
        id: muteAction

        property var qaction: app.action("mute")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["muteAction"] = muteAction
        onTriggered: {
            mpv.setProperty("mute", !mpv.getProperty("mute"));
            if (mpv.getProperty("mute")) {
                text = qsTr("Unmute");
                icon.name = "player-volume-muted";
            } else {
                text = qaction.text;
                icon.name = qaction.iconName();
            }
        }
    }
    property Action openAction: Action {
        id: openAction

        property var qaction: app.action("openFile")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["openAction"] = openAction
        onTriggered: openFileDialog.open()
    }
    property Action playNextAction: Action {
        id: playNextAction

        property var qaction: app.action("playNext")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["playNextAction"] = playNextAction
        onTriggered: {
            const nextFileRow = mpv.playlistModel.getPlayingVideo() + 1;
            const updateLastPlayedFile = !playList.isYouTubePlaylist;
            if (nextFileRow < playList.playlistView.count) {
                mpv.pause = true;
                mpv.position = 0;
                mpv.playlistModel.setPlayingVideo(nextFileRow);
                mpv.loadItem(nextFileRow);
            } else {
                // Last file in playlist
                if (PlaylistSettings.repeat) {
                    mpv.pause = true;
                    mpv.position = 0;
                    mpv.playlistModel.setPlayingVideo(0);
                    mpv.loadItem(0);
                }
            }
        }
    }
    property Action playPauseAction: Action {
        id: playPauseAction

        property var qaction: app.action("play_pause")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["playPauseAction"] = playPauseAction
        onTriggered: mpv.togglePlayPause()
    }
    property Action playPreviousAction: Action {
        id: playPreviousAction

        property var qaction: app.action("playPrevious")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["playPreviousAction"] = playPreviousAction
        onTriggered: {
            if (mpv.playlistModel.getPlayingVideo() !== 0) {
                const previousFileRow = mpv.playlistModel.getPlayingVideo() - 1;
                const updateLastPlayedFile = !playList.isYouTubePlaylist;
                mpv.pause = true;
                mpv.position = 0;
                mpv.playlistModel.setPlayingVideo(previousFileRow);
                mpv.loadItem(previousFileRow);
            }
        }
    }
    property Action quitApplicationAction: Action {
        id: quitApplicationAction

        property var qaction: app.action("file_quit")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: {
            list["quitApplicationAction"] = quitApplicationAction;
        }
        onTriggered: {
            mpv.handleTimePosition();
            qaction.trigger();
        }
    }
    property Action saveAsCPlayFileAction: Action {
        id: saveAsCPlayFileAction

        property var qaction: app.action("saveAsCPlayFile")

        enabled: false
        icon.color: mpv.playSectionsModel.currentEditItemIsEdited ? "orange" : "lime"
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["saveAsCPlayFileAction"] = saveAsCPlayFileAction
        onTriggered: {
            saveCPlayFileDialog.currentFile = mpv.playSectionsModel.getSuggestedFileURL();
            mpv.setLoadedAsCurrentEditItem();
            if (!mpv.playSectionsModel.isEmpty())
                saveAsCPlayFileWindow.visible = true;
        }
    }
    property Action seekBackwardBigAction: Action {
        id: seekBackwardBigAction

        property var qaction: app.action("seekBackwardBig")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["seekBackwardBigAction"] = seekBackwardBigAction
        onTriggered: mpv.seek(-PlaybackSettings.seekBigStep)
    }
    property Action seekBackwardMediumAction: Action {
        id: seekBackwardMediumAction

        property var qaction: app.action("seekBackwardMedium")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["seekBackwardMediumAction"] = seekBackwardMediumAction
        onTriggered: mpv.seek(-PlaybackSettings.seekMediumStep)
    }
    property Action seekBackwardSmallAction: Action {
        id: seekBackwardSmallAction

        property var qaction: app.action("seekBackwardSmall")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["seekBackwardSmallAction"] = seekBackwardSmallAction
        onTriggered: mpv.seek(-PlaybackSettings.seekSmallStep)
    }
    property Action seekForwardBigAction: Action {
        id: seekForwardBigAction

        property var qaction: app.action("seekForwardBig")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["seekForwardBigAction"] = seekForwardBigAction
        onTriggered: mpv.seek(PlaybackSettings.seekBigStep)
    }
    property Action seekForwardMediumAction: Action {
        id: seekForwardMediumAction

        property var qaction: app.action("seekForwardMedium")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["seekForwardMediumAction"] = seekForwardMediumAction
        onTriggered: mpv.seek(PlaybackSettings.seekMediumStep)
    }
    property Action seekForwardSmallAction: Action {
        id: seekForwardSmallAction

        property var qaction: app.action("seekForwardSmall")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["seekForwardSmallAction"] = seekForwardSmallAction
        onTriggered: mpv.seek(PlaybackSettings.seekSmallStep)
    }
    property Action stopRewindAction: Action {
        id: stopRewindAction

        property var qaction: app.action("stop_rewind")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["stopRewindAction"] = stopRewindAction
        onTriggered: mpv.performRewind()
    }
    property Action syncAction: Action {
        id: syncAction

        icon.name: "im-user-online"
        text: qsTr("Sync On")

        onTriggered: {
            mpv.syncVideo = !mpv.syncVideo;
            if (mpv.syncVideo) {
                text = qsTr("Sync On");
                icon.name = "im-user-online";
            } else {
                text = qsTr("Sync Off");
                icon.name = "im-user-offline";
            }
        }
    }
    property Action toggleHeaderAction: Action {
        id: toggleHeaderAction

        property var qaction: app.action("toggleHeader")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["toggleHeaderAction"] = toggleHeaderAction
        onTriggered: UserInterfaceSettings.showHeader = !header.visible
    }
    property Action toggleLayersAction: Action {
        id: toggleLayersAction

        property var qaction: app.action("toggleLayers")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["toggleLayersAction"] = toggleLayersAction
        onTriggered: {
            if (layers.state === "hidden" && slides.state === "hidden") {
                layers.state = "visible-without-partner";
            } else if (layers.state === "hidden" && slides.state === "visible-without-partner") {
                slides.state = "visible-with-partner";
                layers.state = "visible-with-partner";
            } else if (slides.state === "visible-with-partner") {
                slides.state = "visible-without-partner";
                layers.state = "hidden";
            } else {
                layers.state = "hidden";
            }
        }
    }
    property Action toggleMenuBarAction: Action {
        id: toggleMenuBarAction

        property var qaction: app.action("toggleMenuBar")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["toggleMenuBarAction"] = toggleMenuBarAction
        onTriggered: UserInterfaceSettings.showMenuBar = !menuBar.visible
    }
    property Action togglePlaylistAction: Action {
        id: togglePlaylistAction

        property var qaction: app.action("togglePlaylist")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["togglePlaylistAction"] = togglePlaylistAction
        onTriggered: {
            if (playList.state === "hidden" && playSections.state === "hidden") {
                playList.state = "visible-without-partner";
            } else if (playList.state === "hidden" && playSections.state === "visible-without-partner") {
                playList.state = "visible-with-partner";
                playSections.state = "visible-with-partner";
            } else if (playSections.state === "visible-with-partner") {
                playSections.state = "visible-without-partner";
                playList.state = "hidden";
            } else {
                playList.state = "hidden";
            }
        }
    }
    property Action toggleSectionsAction: Action {
        id: toggleSectionsAction

        property var qaction: app.action("toggleSections")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["toggleSectionsAction"] = toggleSectionsAction
        onTriggered: {
            if (playSections.state === "hidden" && playList.state === "hidden") {
                playSections.state = "visible-without-partner";
            } else if (playSections.state === "hidden" && playList.state === "visible-without-partner") {
                playList.state = "visible-with-partner";
                playSections.state = "visible-with-partner";
            } else if (playList.state === "visible-with-partner") {
                playList.state = "visible-without-partner";
                playSections.state = "hidden";
            } else {
                playSections.state = "hidden";
            }
        }
    }
    property Action toggleSlidesAction: Action {
        id: toggleSlidesAction

        property var qaction: app.action("toggleSlides")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["toggleSlidesAction"] = toggleSlidesAction
        onTriggered: {
            if (slides.state === "hidden" && layers.state === "hidden") {
                slides.state = "visible-without-partner";
            } else if (slides.state === "hidden" && layers.state === "visible-without-partner") {
                slides.state = "visible-with-partner";
                layers.state = "visible-with-partner";
            } else if (layers.state === "visible-with-partner") {
                layers.state = "visible-without-partner";
                slides.state = "hidden";
            } else {
                slides.state = "hidden";
            }
        }
    }
    property Action layerCopyAction: Action {
        id: layerCopyAction

        property var qaction: app.action("layerCopy")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["layerCopyAction"] = layerCopyAction
        onTriggered: {
            if(enabled){
                app.slides.copyLayer();
                app.action("layerPaste").enabled = true;
                app.action("layerPasteProperties").enabled = true;
            }
        }
    }
    property Action layerPasteAction: Action {
        id: layerPasteAction

        property var qaction: app.action("layerPaste")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["layerPasteAction"] = layerPasteAction
        onTriggered: {
            if(enabled){
                app.slides.pasteLayer();
            }
        }
    }
    property Action layerPastePropertiesAction: Action {
        id: layerPastePropertiesAction

        property var qaction: app.action("layerPasteProperties")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["layerPastePropertiesAction"] = layerPastePropertiesAction
        onTriggered: {
            if(enabled){
                app.slides.pasteLayerAsProperties(app.slides.selected.layerToCopy);
            }
        }
    }
    property Action slidePreviousAction: Action {
        id: slidePreviousAction

        property var qaction: app.action("slidePrevious")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["slidePreviousAction"] = slidePreviousAction
        onTriggered: {
            if(enabled){
                if(app.slides.selectedSlideIdx === app.slides.triggeredSlideIdx){
                    app.slides.selectedSlideIdx = app.slides.selectedSlideIdx - 1;
                    app.slides.triggeredSlideIdx = app.slides.triggeredSlideIdx - 1;
                }
                else {
                    app.slides.triggeredSlideIdx = app.slides.selectedSlideIdx;
                }   
            }
        }
    }
    property Action slideNextAction: Action {
        id: slideNextAction

        property var qaction: app.action("slideNext")

        enabled: qaction.enabled
        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["slideNextAction"] = slideNextAction
        onTriggered: {
            if(enabled){
                if(app.slides.selectedSlideIdx === app.slides.triggeredSlideIdx){
                    app.slides.selectedSlideIdx = app.slides.selectedSlideIdx + 1;
                    app.slides.triggeredSlideIdx = app.slides.triggeredSlideIdx + 1;
                }
                else {
                    app.slides.triggeredSlideIdx = app.slides.selectedSlideIdx;
                }
            }
        }
    }
    property Action visibilityFadeDownAction: Action {
        id: visibilityFadeDownAction

        property var qaction: app.action("visibilityFadeDown")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["visibilityFadeDownAction"] = visibilityFadeDownAction
        onTriggered: {
            mpv.fadeImageDown();
        }
    }
    property Action visibilityFadeUpAction: Action {
        id: visibilityFadeUpAction

        property var qaction: app.action("visibilityFadeUp")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["visibilityFadeUpAction"] = visibilityFadeUpAction
        onTriggered: {
            mpv.fadeImageUp();
        }
    }
    property Action volumeDownAction: Action {
        id: volumeDownAction

        property var qaction: app.action("volumeDown")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["volumeDownAction"] = volumeDownAction
        onTriggered: {
            mpv.command(["add", "volume", -AudioSettings.volumeStep]);
            osd.message(`Volume: ${parseInt(mpv.getProperty("volume"))}`);
        }
    }
    property Action volumeFadeDownAction: Action {
        id: volumeFadeDownAction

        property var qaction: app.action("volumeFadeDown")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["volumeFadeDownAction"] = volumeFadeDownAction
        onTriggered: {
            mpv.fadeVolumeDown();
        }
    }
    property Action volumeFadeUpAction: Action {
        id: volumeFadeUpAction

        property var qaction: app.action("volumeFadeUp")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["volumeFadeUpAction"] = volumeFadeUpAction
        onTriggered: {
            mpv.fadeVolumeUp();
        }
    }
    property Action volumeUpAction: Action {
        id: volumeUpAction

        property var qaction: app.action("volumeUp")

        icon.name: qaction.iconName()
        shortcut: qaction.shortcutName()
        text: qaction.text

        Component.onCompleted: list["volumeUpAction"] = volumeUpAction
        onTriggered: {
            mpv.command(["add", "volume", AudioSettings.volumeStep]);
            osd.message(`Volume: ${parseInt(mpv.getProperty("volume"))}`);
        }
    }
}
