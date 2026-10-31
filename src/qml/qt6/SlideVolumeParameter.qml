/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

// Edits the parameter of the slide volume control operations: "slide:level" for
// SetSlideVolume, or only the slide for the fades, which have no level (showLevel false).
RowLayout {
    id: root

    property string parameter: ""
    property bool showLevel: true

    signal edited(string parameter)

    function composed() {
        return showLevel ? slideComboBox.currentText + ":" + levelSpinBox.value : slideComboBox.currentText;
    }
    function refresh() {
        const text = parameter.trim();
        const names = Array.from(app.slides.slideNames());
        const sep = text.lastIndexOf(":");
        const tail = text.slice(sep + 1).trim();
        // Also tells the slide of "slide:level" when the operation was just changed to a fade.
        const hasLevel = names.indexOf(text) < 0 && /^[0-9]+$/.test(tail) && (sep >= 0 || showLevel);
        const slide = hasLevel ? text.slice(0, Math.max(sep, 0)).trim() : text;
        // Keeps a typed index or a name no longer in the list instead of dropping it.
        if (slide !== "" && names.indexOf(slide) < 0)
            names.push(slide);
        slideComboBox.model = names;
        slideComboBox.currentIndex = names.indexOf(slide !== "" ? slide : app.slides.selected.layersName);
        levelSpinBox.value = hasLevel ? parseInt(tail) : 100;
        if (parameter === "" && visible)
            edited(composed());
    }
    // Rewrites a parameter left over from the previous operation, after the user changed it.
    function adoptOperation() {
        if (visible && composed() !== parameter)
            edited(composed());
    }

    onParameterChanged: refresh()
    onShowLevelChanged: refresh()
    onVisibleChanged: refresh()
    Component.onCompleted: refresh()

    ComboBox {
        id: slideComboBox

        Layout.fillWidth: true

        ToolTip.text: qsTr("Slide to change the volume of. It is stored by name, so it stays the same slide when the slides are reordered.")
        ToolTip.visible: hovered

        onActivated: root.edited(root.composed())
        // Picks up slides added or renamed since the list was filled.
        onPressedChanged: {
            if (pressed)
                root.refresh();
        }
    }
    SpinBox {
        id: levelSpinBox

        editable: true
        from: 0
        to: 100
        visible: root.showLevel

        ToolTip.text: qsTr("Slide volume (0-100)")
        ToolTip.visible: hovered

        onValueModified: root.edited(root.composed())
    }
}
