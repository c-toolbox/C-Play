/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick 2.12
import QtQuick.Controls 2.12

Menu {
    id: root

    title: qsTr("&Playback")

    MenuItem { action: actions["playPauseAction"] }
    MenuItem { action: actions["playNextAction"] }
    MenuItem { action: actions["playPreviousAction"] }

    MenuSeparator {}

    MenuSeparator {}

    Menu {
        title: "Seek"
        MenuItem { action: actions["seekForwardSmallAction"] }
        MenuItem { action: actions["seekBackwardSmallAction"] }

        MenuSeparator {}

        MenuItem { action: actions["seekForwardMediumAction"] }
        MenuItem { action: actions["seekBackwardMediumAction"] }

        MenuSeparator {}

        MenuItem { action: actions["seekForwardBigAction"] }
        MenuItem { action: actions["seekBackwardBigAction"] }
    }
}
