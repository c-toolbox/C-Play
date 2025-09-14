/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls

Menu {
    id: root

    title: qsTr("&Settings")

    Menu {
        title: "States"
        MenuItem {
            action: actions.windowOpacityAction
            ToolTip {
                text: "ON/OFF to have node windows visible."
            }
        }
        MenuItem {
            action: actions.windowOnTopAction
            ToolTip {
                text: "ON/OFF to sync state from master to clients."
            }
        }
        MenuItem {
            action: actions.syncAction
            ToolTip {
                text: "ON/OFF to sync state from master to clients."
            }
        }
    }
    MenuSeparator {
    }
    MenuItem {
        action: actions["configureShortcutsAction"]
    }
    MenuSeparator {
    }
    MenuItem {
        action: actions["configureAction"]
    }
}
