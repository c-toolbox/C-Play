/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIRECTSHOWPATHSCONFIG_H
#define DIRECTSHOWPATHSCONFIG_H

#include <map>
#include <string>

// Parses the predefined DirectShow capture setup list (default path: ./data/predefined-directshows.json) and resolves, for each entry, which local video/audio capture devices a given machine should use.
//
// Each entry is identified by its "title", which acts as a stable key shared across all machines in a cluster. Besides the plain default "videoDevice"/"audioDevice", an entry may define:
//   * "devices" - an object mapping role/node id ("master", or node ids from data/multivideo/nodes.json) to a device pair {"videoDevice": ..., "audioDevice": ...}. When present it wins for that machine, even when both values are empty ("" or null), which means the capture setup is intentionally not opened on that machine.
//
// Resolution order per machine: devices[role] (if present) -> plain videoDevice/audioDevice.
//
// JSON format:
// {
//   "directshows": [
//     {
//       "title": "HDMI Capture 1",
//       "videoDevice": "DELTA-hmi Video Source (card0 RX0)",
//       "audioDevice": "",
//       "enabled": true,
//       "devices": {
//         "master": { "videoDevice": "DELTA-hmi Video Source (card0 RX0)", "audioDevice": "" },
//         "node-A": { "videoDevice": "Datapath VisionSC-DP2 Video 01", "audioDevice": "" }
//       }
//     }
//   ]
// }

class DirectShowPathsConfig {
public:
    static const std::string kDefaultFilePath;

    struct DevicePair {
        std::string videoDevice; // May be empty (no video capture on that machine)
        std::string audioDevice; // May be empty (no microphone on that machine)
    };

    struct Entry {
        DevicePair devices;                          // Plain default device pair (may be empty)
        std::map<std::string, DevicePair> perRole;   // Role/node id -> device pair (both empty = no capture on that machine)
        bool hasPerRole = false;                     // Whether the "devices" key was present at all
    };

    DirectShowPathsConfig();

    // Locate predefined-directshows.json. Tries the default CWD-relative path first, then walks up from the current directory looking for <dir>/data/predefined-directshows.json (covers e.g. running from a build/ subfolder). Returns "" if not found.
    static std::string findDefaultFilePath();

    // Load and parse configuration from a JSON file. Returns true on success.
    bool loadFromFile(const std::string& filePath = kDefaultFilePath);

    // Whether any entries were loaded.
    bool isLoaded() const;

    // Number of parsed setup entries.
    int entryCount() const;

    // Resolve the local capture devices for a setup entry (by title) on the machine identified by role ("master" or node id).
    // Returns false if no entry with that title exists (caller should fall back to the synced device pair).
    // When true, both out values may be empty - meaning "no capture on this machine".
    bool resolveDevicesForRole(const std::string& title, const std::string& role, std::string& outVideoDevice, std::string& outAudioDevice) const;

private:
    // title -> entry
    std::map<std::string, Entry> m_entries;
    bool m_loaded = false;
};

#endif // DIRECTSHOWPATHSCONFIG_H