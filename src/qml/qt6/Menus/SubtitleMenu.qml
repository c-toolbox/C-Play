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

    title: qsTr("&Subtitle")

    Menu {
        id: subtitleMenu

        title: qsTr("&Subtitle Track")
        icon.name: "media-track-show-active"

        Instantiator {
            id: subtitleMenuInstantiator

            model: 0

            delegate: MenuItem {
                id: subtitleMenuItem

                checkable: true
                checked: model.id === mpv.subtitleId
                text: model.text

                onTriggered: mpv.subtitleId = model.id
            }

            onObjectAdded: subtitleMenu.insertItem(index, object)
            onObjectRemoved: subtitleMenu.removeItem(object)
        }
        Connections {
            function onFileLoaded() {
                subtitleMenuInstantiator.model = mpv.subtitleTracksModel;
            }

            target: mpv
        }
    }
}
