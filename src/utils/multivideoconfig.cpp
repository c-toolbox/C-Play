/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "multivideoconfig.h"
#include <sgct/sgct.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

MultiVideoConfig::MultiVideoConfig() {}

bool MultiVideoConfig::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        sgct::Log::Warning(std::format("MultiVideoConfig: cannot open file '{}'", filePath));
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return loadFromString(ss.str());
}

bool MultiVideoConfig::loadFromString(const std::string& jsonStr) {
    if (jsonStr.empty()) {
        return false;
    }
    try {
        nlohmann::json doc = nlohmann::json::parse(jsonStr);

        // Parse master file and optional parameters
        m_masterParams.file.clear();
        m_masterParams.gridMode   = 0;
        m_masterParams.stereoMode = 0;
        m_masterParams.audio      = false;
        m_masterParams.audioFile.clear();

        if (doc.contains("master") && doc["master"].is_object()) {
            const auto& master = doc["master"];
            if (master.contains("file") && master["file"].is_string()) {
                m_masterParams.file = master["file"].get<std::string>();
            }
            // Optional master parameters (parsed as strings to match existing helpers)
            if (master.contains("gridMode") && master["gridMode"].is_string())
                m_masterParams.gridMode = parseGridMode(master["gridMode"].get<std::string>());
            if (master.contains("stereoMode") && master["stereoMode"].is_string())
                m_masterParams.stereoMode = parseStereoMode(master["stereoMode"].get<std::string>());
            if (master.contains("audio") && master["audio"].is_boolean())
                m_masterParams.audio = master["audio"].get<bool>();
            // Optional separate audio file for the master reference video
            if (master.contains("audioFile") && master["audioFile"].is_string())
                m_masterParams.audioFile = master["audioFile"].get<std::string>();
        }

        // Parse video entries
        m_entries.clear();
        if (!doc.contains("videos") || !doc["videos"].is_array()) {
            sgct::Log::Warning("MultiVideoConfig: JSON must contain a 'videos' array");
            return false;
        }

        for (const auto& v : doc["videos"]) {
            MultiVideoEntry entry;

            if (v.contains("name") && v["name"].is_string())
                entry.name = v["name"].get<std::string>();

            if (v.contains("eyeMode") && v["eyeMode"].is_string())
                entry.eyeMode = parseEyeMode(v["eyeMode"].get<std::string>());

            if (v.contains("stereoMode") && v["stereoMode"].is_string())
                entry.stereoMode = parseStereoMode(v["stereoMode"].get<std::string>());

            if (v.contains("gridMode") && v["gridMode"].is_string())
                entry.gridMode = parseGridMode(v["gridMode"].get<std::string>());

            if (v.contains("audio") && v["audio"].is_boolean())
                entry.audio = v["audio"].get<bool>();

            // Optional separate audio file for this entry
            if (v.contains("audioFile") && v["audioFile"].is_string())
                entry.audioFile = v["audioFile"].get<std::string>();

            // Plane params
            if (v.contains("plane") && v["plane"].is_object()) {
                const auto& p = v["plane"];
                if (p.contains("azimuth")   && p["azimuth"].is_number())   entry.planeAzimuth   = p["azimuth"].get<double>();
                if (p.contains("elevation") && p["elevation"].is_number()) entry.planeElevation = p["elevation"].get<double>();
                if (p.contains("roll")      && p["roll"].is_number())      entry.planeRoll      = p["roll"].get<double>();
                if (p.contains("distance")  && p["distance"].is_number())  entry.planeDistance  = p["distance"].get<double>();
                if (p.contains("width")     && p["width"].is_number())     entry.planeWidth     = p["width"].get<double>();
                if (p.contains("height")    && p["height"].is_number())    entry.planeHeight    = p["height"].get<double>();
                if (p.contains("aspect")    && p["aspect"].is_number_integer())
                    entry.planeAspectRatioConsideration = static_cast<uint8_t>(p["aspect"].get<int>());
            }

            // Rotate
            if (v.contains("rotate") && v["rotate"].is_array() && v["rotate"].size() >= 3) {
                entry.rotate = glm::vec3(
                    v["rotate"][0].get<float>(),
                    v["rotate"][1].get<float>(),
                    v["rotate"][2].get<float>());
            }

            // Translate
            if (v.contains("translate") && v["translate"].is_array() && v["translate"].size() >= 3) {
                entry.translate = glm::vec3(
                    v["translate"][0].get<float>(),
                    v["translate"][1].get<float>(),
                    v["translate"][2].get<float>());
            }

            // ROI
            if (v.contains("roi") && v["roi"].is_object()) {
                const auto& r = v["roi"];
                entry.roiEnabled = r.value("enabled", false);
                entry.roi.x = static_cast<float>(r.value("x", 0.0));
                entry.roi.y = static_cast<float>(r.value("y", 0.0));
                entry.roi.z = static_cast<float>(r.value("w", 1.0));
                entry.roi.w = static_cast<float>(r.value("h", 1.0));
            }

            // Per-node paths
            if (v.contains("paths") && v["paths"].is_object()) {
                for (auto it = v["paths"].begin(); it != v["paths"].end(); ++it) {
                    if (it.value().is_string())
                        entry.paths[it.key()] = it.value().get<std::string>();
                }
            }

            // Path template
            if (v.contains("pathTemplate") && v["pathTemplate"].is_string())
                entry.pathTemplate = v["pathTemplate"].get<std::string>();

            m_entries.push_back(std::move(entry));
        }

        sgct::Log::Info(std::format("MultiVideoConfig: loaded {} video entries, masterFile='{}', gridMode={}, stereoMode={}, audio={}",
                                    m_entries.size(), m_masterParams.file,
                                    static_cast<int>(m_masterParams.gridMode),
                                    static_cast<int>(m_masterParams.stereoMode),
                                    m_masterParams.audio));
        return !m_entries.empty();
    }
    catch (const std::exception& e) {
        sgct::Log::Error(std::format("MultiVideoConfig: JSON parse error: {}", e.what()));
        return false;
    }
}

const std::string& MultiVideoConfig::masterFile() const {
    return m_masterParams.file;
}

const MultiVideoMasterParams& MultiVideoConfig::masterParams() const {
    return m_masterParams;
}

const std::vector<MultiVideoEntry>& MultiVideoConfig::entries() const {
    return m_entries;
}

/*static*/
std::string MultiVideoConfig::resolvePathForNode(const MultiVideoEntry& entry,
                                                   const std::string& nodeId) {
    // Direct lookup
    auto it = entry.paths.find(nodeId);
    if (it != entry.paths.end() && !it->second.empty())
        return it->second;

    // Template substitution
    if (!entry.pathTemplate.empty()) {
        const std::string placeholder = "{nodeId}";
        std::string result = entry.pathTemplate;
        auto pos = result.find(placeholder);
        if (pos != std::string::npos) {
            result.replace(pos, placeholder.size(), nodeId);
            return result;
        }
        return entry.pathTemplate;
    }

    return "";
}

bool MultiVideoConfig::isValid() const {
    return !m_entries.empty();
}

/*static*/
uint8_t MultiVideoConfig::parseEyeMode(const std::string& s) {
    if (s == "left" || s == "Left")   return 1; // BaseLayer::EyeMode::Left
    if (s == "right" || s == "Right") return 2; // BaseLayer::EyeMode::Right
    return 0; // BaseLayer::EyeMode::Both
}

/*static*/
uint8_t MultiVideoConfig::parseStereoMode(const std::string& s) {
    if (s == "SBS_3D")  return 1;
    if (s == "TB_3D")   return 2;
    if (s == "TBF_3D")  return 3;
    return 0; // No_2D
}

/*static*/
uint8_t MultiVideoConfig::parseGridMode(const std::string& s) {
    if (s == "Plane")      return 1;
    if (s == "Dome")       return 2;
    if (s == "Sphere_EQR") return 3;
    if (s == "Sphere_EAC") return 4;
    return 0; // None
}