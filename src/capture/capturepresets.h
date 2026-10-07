/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CAPTUREPRESETS_H
#define CAPTUREPRESETS_H

#include <capture/capturebackend.h>
#include <map>
#include <string>
#include <vector>

// Predefined capture setups from data/predefined-captures.json. Each entry is identified by its
// "title", a stable key shared by all machines in the cluster: every machine resolves the entry
// against its own local copy of the file, so the master and the nodes can capture from different
// inputs - or not at all.
//
// Resolution per machine: sources[role] (when present) -> the entry's default source. A role object
// only overrides the fields it sets, so input, ganging, directGpu and audio can all differ per machine.
// role is "master", or the node id from data/multivideo/nodes.json (falling back to the node IP).
// A role mapped to null, or to an input <= 0, means "no capture on that machine".
//
// JSON format:
// {
//   "captures": [
//     {
//       "title": "Datapath Input 1",
//       "backend": "datapath",
//       "input": 1,                 // 1-based, as in the Datapath Vision utility
//       "ganging": "",              // optional: 2x1, 1x2, 2x2, 3x1, 1x3, 4x1, 1x4 or off
//       "directGpu": true,          // optional: GPU direct transfer when a professional GPU is present
//       "audio": false,             // optional: capture the input's audio and play it on that machine
//       "enabled": true,
//       "sources": {
//         "master": { "audio": true, "directGpu": false },  // master plays the audio, CPU copy
//         "node-A": { "input": 2 },
//         "node-B": null                                     // no capture on node-B
//       }
//     }
//   ]
// }
class CapturePresets {
public:
    static const std::string kDefaultFilePath;

    struct Entry {
        std::string title;
        bool enabled = true;
        CaptureSource source;                         // default source
        std::map<std::string, CaptureSource> perRole; // role/node id -> source (input <= 0 = no capture)
    };

    // ./data/predefined-captures.json, or the first <dir>/data/predefined-captures.json found walking
    // up from the current directory. Empty when not found.
    static std::string findDefaultFilePath();

    bool loadFromFile(const std::string& filePath);
    const std::vector<Entry>& entries() const { return m_entries; }

    // False when no entry with that title exists. When true, out may be invalid (no capture here).
    bool resolveForRole(const std::string& title, const std::string& role, CaptureSource& out) const;

    // Resolves an entry for this machine using a process-wide copy of the local files, re-read at
    // most every few seconds. Any thread.
    static bool resolve(const std::string& title, bool isMaster, CaptureSource& out);

private:
    std::vector<Entry> m_entries;
};

#endif // CAPTUREPRESETS_H
