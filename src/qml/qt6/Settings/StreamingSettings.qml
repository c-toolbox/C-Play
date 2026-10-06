/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

import org.kde.kirigami as Kirigami
import org.ctoolbox.cplay

SettingsBasePage {
    id: root

    CPlayFileDialog {
        id: ytdlpPathDialog

        parentWindow: root.Window.window
        fileMode: CPlayFileDialog.OpenFile
        nameFilters: ["yt-dlp executable (yt-dlp.exe yt-dlp)", "All files (*.*)"]
        title: "Choose yt-dlp executable"

        onAccepted: {
            ytdlpPathText.text = playerController.returnRelativeOrAbsolutePath(ytdlpPathDialog.selectedFile.toString());
            StreamSettings.ytdlpPath = ytdlpPathText.text;
            StreamSettings.save();
        }
        onRejected: {}
    }

    ColumnLayout {
        SettingsHeader {
            Layout.fillWidth: true
            text: qsTr("Streaming settings")
        }

        GridLayout {
            columns: 2

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("yt-dlp path:")
            }
            RowLayout {
                Layout.fillWidth: true

                TextField {
                    id: ytdlpPathText

                    Layout.fillWidth: true
                    placeholderText: "Auto-detect (next to C-Play.exe or PATH)"
                    text: StreamSettings.ytdlpPath

                    onEditingFinished: {
                        StreamSettings.ytdlpPath = text;
                        StreamSettings.save();
                    }

                    ToolTip {
                        text: qsTr("Path to the yt-dlp executable used for YouTube URLs in Stream layers. Empty = auto-detect next to C-Play.exe, then in <working directory>/plugins/, then on PATH.")
                    }
                }
                ToolButton {
                    focusPolicy: Qt.NoFocus
                    icon.height: 16
                    icon.name: "document-open"
                    text: ""

                    onClicked: {
                        ytdlpPathDialog.open();
                    }

                    ToolTip {
                        text: qsTr("Browse for yt-dlp.exe")
                    }
                }
                ToolButton {
                    focusPolicy: Qt.NoFocus
                    icon.height: 16
                    icon.name: "edit-clear-all"
                    text: ""

                    onClicked: {
                        ytdlpPathText.text = "";
                        StreamSettings.ytdlpPath = "";
                        StreamSettings.save();
                    }

                    ToolTip {
                        text: qsTr("Clear - use auto-detection")
                    }
                }
            }
            Item {
                Layout.fillWidth: true
            }
            Label {
                Layout.columnSpan: 2
                Layout.fillWidth: true
                Layout.leftMargin: 4
                font.pointSize: 9
                font.italic: true
                wrapMode: Text.WordWrap
                color: Kirigami.Theme.neutralTextColor
                text: qsTr("YouTube URLs in Stream layers are resolved by mpv via the external yt-dlp tool. "
                         + "Every cluster node playing such a layer needs yt-dlp too. Status on this machine: ")
                      + (app.resolveYtdlpPath() === ""
                         ? qsTr("yt-dlp NOT found - download yt-dlp.exe and place it next to C-Play.exe (or into <working directory>/plugins/), on PATH, or set the path above.")
                         : qsTr("found: %1").arg(app.resolveYtdlpPath()))
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("yt-dlp format:")
            }
            RowLayout {
                Layout.fillWidth: true

                TextField {
                    id: ytdlFormatText

                    Layout.fillWidth: true
                    placeholderText: "Default (e.g. bv*[height<=1080]+ba/b)"
                    text: StreamSettings.ytdlFormat

                    onEditingFinished: {
                        StreamSettings.ytdlFormat = text;
                        StreamSettings.save();
                    }

                    ToolTip {
                        text: qsTr("Optional yt-dlp format selector passed to mpv's ytdl_hook (ytdl-format). Empty = mpv/yt-dlp default.")
                    }
                }
            }
        }

        GridLayout {
            columns: 3

            Item {
                Layout.fillWidth: true
            }
            CheckBox {
                visible: NDI_SUPPORT
                Layout.columnSpan: 2
                text: qsTr("Convert NDI input on the GPU (recreate NDI layers to apply)")
                checked: StreamSettings.ndiReceiveGpuConversion
                onToggled: {
                    StreamSettings.ndiReceiveGpuConversion = checked;
                    StreamSettings.save();
                }
            }

            Item {
                Layout.fillWidth: true
            }
            CheckBox {
                visible: NDI_SUPPORT
                Layout.columnSpan: 2
                text: qsTr("Convert NDI output on the GPU (restart outputs to apply; discards alpha)")
                checked: StreamSettings.ndiOutputGpuConversion
                onToggled: {
                    StreamSettings.ndiOutputGpuConversion = checked;
                    StreamSettings.save();
                }
            }

            Item {
                Layout.fillWidth: true
            }
            Label {
                visible: NDI_SUPPORT
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Input uses the fastest NDI decoder format with GPU conversion and preserved alpha. Opaque UYVY input and output transfer half the RGBA size. QR detection scans native luminance. Unsupported cases use CPU conversion.")
            }

            Label {
                Layout.alignment: Qt.AlignRight
                text: qsTr("NDI output resolution for 3D view:")
            }
            RowLayout {
                ComboBox {
                    id: ndiResolution3DViewComboBox

                    textRole: "key"
                    currentIndex: StreamSettings.ndiResolution3DView

                    model: ListModel {
                        ListElement {
                            key: "2K"
                        }
                        ListElement {
                            key: "4K"
                        }
                        ListElement {
                            key: "6K"
                        }
                        ListElement {
                            key: "8K"
                        }
                    }

                    onActivated: {
                        StreamSettings.ndiResolution3DView = currentIndex;
                        StreamSettings.save();
                    }
                }
                LabelWithTooltip {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: {
                        const perspective = ["1920x1080", "3840x2160", "5760x3240", "7680x4320"];
                        const fisheye = ["2048x2048", "4096x4096", "6144x6144", "8192x8192"];
                        const idx = ndiResolution3DViewComboBox.currentIndex;
                        qsTr("%1 with perspective camera, %2 as fisheye").arg(perspective[idx]).arg(fisheye[idx]);
                    }
                }
            }
            Item {
                Layout.fillWidth: true
            }

            Item {
                visible: NODE_STREAM_SUPPORT && NDI_SUPPORT
                height: 1
                width: 1
            }
            CheckBox {
                visible: NODE_STREAM_SUPPORT && NDI_SUPPORT
                checked: StreamSettings.nodeStreamPreferNdi
                text: qsTr("Prefer NDI for streaming layers to the nodes, instead of UDP multicast.")

                onCheckedChanged: {
                    StreamSettings.nodeStreamPreferNdi = checked;
                    StreamSettings.save();
                }

                ToolTip {
                    text: qsTr("Layers are streamed to the nodes as NDI sources and received by automatically created NDI layers on the nodes. Can be overridden per layer. Requires NDI on the master and the nodes.")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT && NDI_SUPPORT
                Layout.fillWidth: true
            }

            // ------------------------------------
            // LAYER STREAMING TO NODES
            // --

            Item {
                Layout.fillWidth: true
            }
            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.columnSpan: 2
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: qsTr("Enable NodeStream by default for new layers (Add new layer dialog):")
            }

            Item {
                Layout.fillWidth: true
            }
            RowLayout {
                visible: NODE_STREAM_SUPPORT
                Layout.columnSpan: 2
                CheckBox {
                    text: qsTr("DirectShow")
                    checked: StreamSettings.nodeStreamDefaultDirectShow
                    onToggled: {
                        StreamSettings.nodeStreamDefaultDirectShow = checked;
                        StreamSettings.save();
                    }
                }
                CheckBox {
                    text: qsTr("Spout")
                    checked: StreamSettings.nodeStreamDefaultSpout
                    onToggled: {
                        StreamSettings.nodeStreamDefaultSpout = checked;
                        StreamSettings.save();
                    }
                }
                CheckBox {
                    text: qsTr("Stream")
                    checked: StreamSettings.nodeStreamDefaultStream
                    onToggled: {
                        StreamSettings.nodeStreamDefaultStream = checked;
                        StreamSettings.save();
                    }
                }
                CheckBox {
                    text: qsTr("YouTube")
                    checked: StreamSettings.nodeStreamDefaultYouTube
                    onToggled: {
                        StreamSettings.nodeStreamDefaultYouTube = checked;
                        StreamSettings.save();
                    }
                }
            }

            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.alignment: Qt.AlignRight
                text: qsTr("Base multicast group:")
            }
            TextField {
                visible: NODE_STREAM_SUPPORT
                placeholderText: "239.192.77.1"
                text: StreamSettings.nodeStreamBaseGroup

                onEditingFinished: {
                    StreamSettings.nodeStreamBaseGroup = text.trim();
                    StreamSettings.save();
                }

                ToolTip {
                    text: qsTr("Each layer streams to the base group plus its layer id, unless a group is set on the layer.")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.alignment: Qt.AlignRight
                text: qsTr("Base port:")
            }
            SpinBox {
                visible: NODE_STREAM_SUPPORT
                editable: true
                from: 1024
                to: 65535
                value: StreamSettings.nodeStreamBasePort

                onValueChanged: {
                    StreamSettings.nodeStreamBasePort = value.toFixed(0);
                    StreamSettings.save();
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.alignment: Qt.AlignRight
                text: qsTr("Master interface address:")
            }
            TextField {
                visible: NODE_STREAM_SUPPORT
                placeholderText: qsTr("Cluster master address")
                text: StreamSettings.nodeStreamInterface

                onEditingFinished: {
                    StreamSettings.nodeStreamInterface = text.trim();
                    StreamSettings.save();
                }

                ToolTip {
                    text: qsTr("IPv4 address of the network interface to send from. Empty uses the master address of the cluster configuration.")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.alignment: Qt.AlignRight
                text: qsTr("Max datagram size (bytes):")
            }
            RowLayout {
                visible: NODE_STREAM_SUPPORT
                SpinBox {
                    editable: true
                    from: 576
                    to: 65507
                    value: StreamSettings.nodeStreamMaxDatagram

                    onValueChanged: {
                        StreamSettings.nodeStreamMaxDatagram = value.toFixed(0);
                        StreamSettings.save();
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("(1472 for 1500 MTU, 8972 for jumbo frames)")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.alignment: Qt.AlignRight
                text: qsTr("Send rate limit (Mbit/s):")
            }
            RowLayout {
                visible: NODE_STREAM_SUPPORT
                SpinBox {
                    editable: true
                    from: 0
                    to: 100000
                    stepSize: 100
                    value: StreamSettings.nodeStreamRateMbps

                    onValueChanged: {
                        StreamSettings.nodeStreamRateMbps = value.toFixed(0);
                        StreamSettings.save();
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("(All layer streams combined, 0 = 85% of link speed)")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Item {
                visible: NODE_STREAM_SUPPORT
                height: 1
                width: 1
            }
            CheckBox {
                visible: NODE_STREAM_SUPPORT
                checked: StreamSettings.nodeStreamCompressLz4
                text: qsTr("Compress streams with LZ4.")

                onCheckedChanged: {
                    StreamSettings.nodeStreamCompressLz4 = checked;
                    StreamSettings.save();
                }

                ToolTip {
                    text: qsTr("Compresses each packet losslessly. Saves bandwidth on flat graphics and costs some CPU on the master and the nodes.")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Item {
                visible: NODE_STREAM_SUPPORT
                height: 1
                width: 1
            }
            CheckBox {
                visible: NODE_STREAM_SUPPORT
                checked: StreamSettings.nodeStreamDeltaFrames
                text: qsTr("Send only changed regions (delta frames).")

                onCheckedChanged: {
                    StreamSettings.nodeStreamDeltaFrames = checked;
                    StreamSettings.save();
                }

                ToolTip {
                    text: qsTr("Between full keyframes, only the parts of the layer that changed are sent. A node that misses a delta frame waits for the next keyframe, unless frames with lost packets are shown.")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.alignment: Qt.AlignRight
                text: qsTr("Keyframe interval (msec):")
            }
            RowLayout {
                visible: NODE_STREAM_SUPPORT
                SpinBox {
                    editable: true
                    from: 50
                    to: 10000
                    stepSize: 50
                    value: StreamSettings.nodeStreamKeyframeIntervalMs

                    onValueChanged: {
                        StreamSettings.nodeStreamKeyframeIntervalMs = value.toFixed(0);
                        StreamSettings.save();
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("(Full frame for nodes that joined late or lost packets)")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.alignment: Qt.AlignRight
                text: qsTr("Frame-locked wait (msec):")
            }
            RowLayout {
                visible: NODE_STREAM_SUPPORT
                SpinBox {
                    editable: true
                    from: 0
                    to: 50
                    value: StreamSettings.nodeStreamFrameLockedWaitMs

                    onValueChanged: {
                        StreamSettings.nodeStreamFrameLockedWaitMs = value.toFixed(0);
                        StreamSettings.save();
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("(How long a node waits for a late frame)")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.alignment: Qt.AlignRight
                text: qsTr("Frame-locked latency guard (msec):")
            }
            RowLayout {
                visible: NODE_STREAM_SUPPORT
                SpinBox {
                    editable: true
                    from: 0
                    to: 100
                    value: StreamSettings.nodeStreamLatencyGuardMs

                    onValueChanged: {
                        StreamSettings.nodeStreamLatencyGuardMs = value.toFixed(0);
                        StreamSettings.save();
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("(Time a frame has been on the wire before the nodes show it)")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Label {
                visible: NODE_STREAM_SUPPORT
                Layout.alignment: Qt.AlignRight
                text: qsTr("Multicast TTL:")
            }
            SpinBox {
                visible: NODE_STREAM_SUPPORT
                editable: true
                from: 1
                to: 32
                value: StreamSettings.nodeStreamTTL

                onValueChanged: {
                    StreamSettings.nodeStreamTTL = value.toFixed(0);
                    StreamSettings.save();
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Item {
                visible: NODE_STREAM_SUPPORT
                height: 1
                width: 1
            }
            CheckBox {
                visible: NODE_STREAM_SUPPORT
                checked: StreamSettings.nodeStreamLoopback
                text: qsTr("Deliver streams to nodes running on the master computer.")

                onCheckedChanged: {
                    StreamSettings.nodeStreamLoopback = checked;
                    StreamSettings.save();
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }

            Item {
                visible: NODE_STREAM_SUPPORT
                height: 1
                width: 1
            }
            CheckBox {
                visible: NODE_STREAM_SUPPORT
                checked: StreamSettings.nodeStreamAllowPartialFrames
                text: qsTr("Show frames with lost packets on the nodes (may mix two frames).")

                onCheckedChanged: {
                    StreamSettings.nodeStreamAllowPartialFrames = checked;
                    StreamSettings.save();
                }

                ToolTip {
                    text: qsTr("When off, the nodes keep showing the previous complete frame until a complete one arrives.")
                }
            }
            Item {
                visible: NODE_STREAM_SUPPORT
                Layout.fillWidth: true
            }
        }
    }
}
