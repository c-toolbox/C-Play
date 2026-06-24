/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MULTIVIDEOCONFIG_H
#define MULTIVIDEOCONFIG_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include <glm/glm.hpp>

// Parameters for the master reference video playback.
struct MultiVideoMasterParams {
    std::string file;
    uint8_t gridMode   = 0;  // BaseLayer::GridMode::None
    uint8_t stereoMode = 0;  // BaseLayer::StereoMode::No_2D
    bool audio         = false;

    // Optional separate audio file for the master reference video (mirrors cplayfile's separateAudioFile)
    std::string audioFile;
};

// Represents a single video entry in the composition (one video per node-role).
struct MultiVideoEntry {
    std::string name;

    // Rendering mapping — mirrors BaseLayer enum values
    uint8_t eyeMode    = 0; // BaseLayer::EyeMode::Both
    uint8_t stereoMode = 0; // BaseLayer::StereoMode::No_2D
    uint8_t gridMode   = 0; // BaseLayer::GridMode::None

    // Audio (eofMode is controlled by BaseLayer::setEOFMode, not per-entry)
    bool audio  = false;

    // Optional separate audio file for this entry (mirrors cplayfile's separateAudioFile)
    std::string audioFile;

    // Plane grid params (used when gridMode == Plane)
    double planeAzimuth   = 0.0;
    double planeElevation = 0.0;
    double planeRoll      = 0.0;
    double planeDistance  = 0.0;
    double planeWidth     = 0.0;
    double planeHeight    = 0.0;
    uint8_t planeAspectRatioConsideration = 1;

    // Rotation / translation (non-plane grid modes)
    glm::vec3 rotate    = glm::vec3(0.f);
    glm::vec3 translate = glm::vec3(0.f);

    // Region of interest
    bool      roiEnabled = false;
    glm::vec4 roi        = glm::vec4(0.f, 0.f, 1.f, 1.f);

    // Per-node file paths: nodeId -> relative file path
    std::map<std::string, std::string> paths;

    // Optional fallback path template; use {nodeId} as a placeholder
    std::string pathTemplate;
};

// Loadable configuration for multi-video playback.
// Contains the master reference file path and one entry per video stream.
//
// JSON format:
// {
//   "master": { "file": "videos/reference.mp4", "audioFile": "/path/to/master_audio.mp3" },
//   "videos": [
//     { "name": "leftEye",  "eyeMode": "left",  "stereoMode": "No_2D", "gridMode": "Dome",
//       "audio": true, "audioFile": "/path/to/audio_left.mp3",
//       "paths": { "node-A": "videos/left_A.mp4", "node-B": "videos/left_B.mp4" } },
//     { "name": "rightEye", "eyeMode": "right", "stereoMode": "No_2D", "gridMode": "Dome",
//       "paths": { "node-A": "videos/right_A.mp4", "node-B": "videos/right_B.mp4" } }
//   ]
// }
//
class MultiVideoConfig {
public:
    MultiVideoConfig();

    // Load configuration from a JSON file. Returns true on success.
    bool loadFromFile(const std::string& filePath);

    // Load configuration from a JSON string. Returns true on success.
    bool loadFromString(const std::string& jsonStr);

    // The reference video path played on the master.
    const std::string& masterFile() const;

    // Master reference video parameters (gridMode, stereoMode, audio).
    const MultiVideoMasterParams& masterParams() const;

    // All video entries.
    const std::vector<MultiVideoEntry>& entries() const;

    // Resolve the file path for a specific entry on a specific node.
    // Looks up entry.paths[nodeId]; falls back to pathTemplate substitution.
    // Returns an empty string if no path can be resolved.
    static std::string resolvePathForNode(const MultiVideoEntry& entry,
                                          const std::string& nodeId);

    // Whether any entries were loaded.
    bool isValid() const;

private:
    static uint8_t parseEyeMode(const std::string& s);
    static uint8_t parseStereoMode(const std::string& s);
    static uint8_t parseGridMode(const std::string& s);

    MultiVideoMasterParams m_masterParams;
    std::vector<MultiVideoEntry> m_entries;
};

#endif // MULTIVIDEOCONFIG_H
