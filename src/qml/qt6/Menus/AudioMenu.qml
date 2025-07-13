/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQml.Models
import QtQuick
import QtQuick.Controls

Menu {
    id: root

    title: qsTr("&Audio")

    Menu {
        id: audioMenu

        title: qsTr("&Primary Track")
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

            onObjectAdded: audioMenu.insertItem(index, object)
            onObjectRemoved: audioMenu.removeItem(object)
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
}
