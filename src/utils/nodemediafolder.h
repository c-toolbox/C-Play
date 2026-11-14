/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODEMEDIAFOLDER_H
#define NODEMEDIAFOLDER_H

#include <optional>
#include <string>
#include <vector>

#include <QString>

// Per-node media for the main video, given as a folder next to the media file:
//
//   Media/Show.mp4                    played by the master
//   Media/Show_NODES/<nodeId>/...     played by the node with that id (data/nodes.json)
//   Media/Show_NODES/default/...      played by nodes without a folder of their own
//
// A node folder holds:
//   * a video file with the same base name as the master file (any extension), and/or
//   * audio files (wav, mp3, flac, ...), which become that node's audio tracks.
//
// Video + audio files: the node plays the video and its audio files (not the video's own audio).
// Only a video file:   the node plays the video without audio.
// Only audio files:    the node plays audio only.
//
// The master scans the folder on its own disk and sends the result (as JSON) to the nodes, so,
// as for _NODE files, the files must exist at the same paths on the master and on the nodes.
// Nodes without a matching folder (and without default/) play the master's file or its _NODE file.
struct NodeMediaEntry {
    std::string video;              // "" = no video on this node
    std::vector<std::string> audio; // in alphabetical order; empty = no audio on this node

    bool audioOnly() const { return video.empty() && !audio.empty(); }
};

class NodeMediaFolder {
public:
    // Folder name that holds the files of nodes without a folder of their own.
    static const QString kDefaultFolder;
    // Suffix of the per-node folder: <base name of the media file><suffix>.
    static const QString kFolderSuffix;

    // Master: the per-node folder of mediaFile (<dir>/<base name>_NODES), or "" when it does not exist.
    static QString folderFor(const QString& mediaFile);

    // Master: scans the per-node folder of mediaFile and returns the map to send to the nodes:
    //   { "folder": "...", "nodes": { "<nodeId>": { "video": "...", "audio": [ "...", ... ] } } }
    // Returns "" when there is no per-node folder or it holds no usable node folders.
    static std::string buildMapJson(const QString& mediaFile);

    // Node: the entry for nodeId in a map from buildMapJson(). Matches the folder name exactly,
    // then ignoring case, then falls back to default/. Empty when the node has no entry.
    static std::optional<NodeMediaEntry> entryForNode(const std::string& mapJson, const std::string& nodeId);

    // True when the file extension is one of the audio file extensions.
    static bool isAudioFile(const QString& filePath);
};

#endif // NODEMEDIAFOLDER_H
