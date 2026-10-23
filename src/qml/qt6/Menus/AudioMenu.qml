/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml.Models
import QtQuick
import QtQuick.Controls
import org.ctoolbox.cplay
import "../Components/PopupHelpers.js" as PopupHelpers

Menu {
    id: root

    title: qsTr("&Audio")

    onOpened: PopupHelpers.handlePopupOpen()
    onClosed: PopupHelpers.handlePopupClose()

    Menu {
        id: audioMenu

        title: qsTr("&Audio Track")
        icon.name: "media-track-show-active"

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

            onObjectAdded: function(index, object) { audioMenu.insertItem(index, object) }
            onObjectRemoved: function(index, object) { audioMenu.removeItem(object) }
        }
        Connections {
            function onFileLoaded() {
                audioMenuInstantiator.model = mpv.audioTracksModel;
            }

            target: mpv
        }
    }
    MenuSeparator {
    }
    MenuItem {
        action: actions["muteAction"]
    }
    MenuItem {
        action: actions["volumeUpAction"]
    }
    MenuItem {
        action: actions["volumeDownAction"]
    }
    MenuItem {
        action: actions["volumeFadeUpAction"]
    }
    MenuItem {
        action: actions["volumeFadeDownAction"]
    }
    MenuSeparator {
        visible: jackItem.visible
        height: visible ? implicitHeight : 0
    }
    MenuItem {
        id: jackItem

        // JACK is used by mpv (the "jack" driver or a JACK audio device) or by the PortAudio layers (the JACK output).
        visible: JACK_SUPPORT
                 && ((AudioSettings.useCustomAudioOutput
                      && ((AudioSettings.useAudioDriver && AudioSettings.preferredAudioOutputDriver === "jack")
                          || (AudioSettings.useAudioDevice && /^jack(\/|$)/.test(AudioSettings.preferredAudioOutputDevice))))
                     || (AudioSettings.portAudioCustomOutput && AudioSettings.portAudioOutputApi === "JACK"))
        height: visible ? implicitHeight : 0
        icon.name: "audio-card"
        text: qsTr("Check JACK server")

        onTriggered: jackStatusDialog.showStatus()
    }
}
