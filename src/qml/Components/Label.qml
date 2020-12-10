import QtQuick 2.12
import QtQuick.Controls 2.12 as QQC2
import QtQuick.Layouts 1.12

QQC2.Label {
    id: root

    property string toolTipText
    property int toolTipFontSize
    property bool alwaysShowToolTip

    QQC2.ToolTip {
        id: toolTip

        visible: (root.alwaysShowToolTip && mouseArea.containsMouse) || (mouseArea.containsMouse && root.truncated)
        text: root.toolTipText ? root.toolTipText : root.text
        font.pointSize: root.toolTipFontSize ? root.toolTipFontSize : root.font.pointSize
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true
    }
}