/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STREAMPATHSCONFIG_H
#define STREAMPATHSCONFIG_H

#include <map>
#include <string>

// Parses the predefined stream list (default path: ./data/predefined-streams.json) and resolves, for each entry, which local media path a given machine should open.
//
// Each entry is identified by its "title", which acts as a stable key shared across all machines in a cluster. Besides the plain default "path", an entry may define:
//   * "paths" - an object mapping role/node id ("master", or node ids from data/multivideo/nodes.json) to a path. When present it wins for that machine, even when the value is empty ("" or null), which means the stream is intentionally not opened on that machine.
//   * "pathTemplate" - a template string where {nodeId} is replaced with the machine's role/node id. Used when there is no explicit "paths" entry for the machine.
//
// Resolution order per machine: paths[role] (if present) -> pathTemplate -> plain path.
//
// JSON format:
// {
//   "streams": [
//     {
//       "title": "HDMI Capture 1",
//       "path": "av://dshow:video=Capture Card (RX0)",
//       "enabled": true,
//       "paths": { "master": "av://dshow:video=Capture Card (RX0)", "node-A": "" },
//       "pathTemplate": "av://dshow:video=Capture {nodeId}"
//     }
//   ]
// }

class StreamPathsConfig {
public:
    static const std::string kDefaultFilePath;

    struct Entry {
        std::string path;                              // Plain default path (may be empty)
        std::map<std::string, std::string> paths;      // Role/node id -> path (empty = no stream on that machine)
        bool hasPaths = false;                         // Whether the "paths" key was present at all
        std::string pathTemplate;                      // Optional template with {nodeId} placeholder
    };

    StreamPathsConfig();

    // Locate predefined-streams.json. Tries the default CWD-relative path first, then walks up from the current directory looking for <dir>/data/predefined-streams.json (covers e.g. running from a build/ subfolder). Returns "" if not found.
    static std::string findDefaultFilePath();

    // Load and parse configuration from a JSON file. Returns true on success.
    bool loadFromFile(const std::string& filePath = kDefaultFilePath);

    // Whether any entries were loaded.
    bool isLoaded() const;

    // Number of parsed stream entries.
    int entryCount() const;

    // Resolve the local path for a stream entry (by title) on the machine identified by role ("master" or node id).
    // Returns false if no entry with that title exists (caller should fall back to the synced file path).
    // When true, outPath may be empty - meaning "no stream on this machine".
    bool resolvePathForRole(const std::string& title, const std::string& role, std::string& outPath) const;

private:
    // title -> entry
    std::map<std::string, Entry> m_entries;
    bool m_loaded = false;
};

#endif // STREAMPATHSCONFIG_H
