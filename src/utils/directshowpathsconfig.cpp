/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "directshowpathsconfig.h"
#include <sgct/sgct.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>

/*static*/ const std::string DirectShowPathsConfig::kDefaultFilePath = "./data/predefined-directshows.json";

DirectShowPathsConfig::DirectShowPathsConfig() {}

namespace {
// Read one optional string field of a device pair object. Missing, null and non-string values all mean "no device".
std::string deviceField(const nlohmann::json& o, const char* name) {
    if (o.contains(name)) {
        const auto& v = o[name];
        return v.is_string() ? v.get<std::string>() : "";
    }
    return "";
}
} // namespace

/*static*/
std::string DirectShowPathsConfig::findDefaultFilePath() {
    // The default path is CWD-relative ("./data/predefined-directshows.json"), which only works when the app is
    // launched from its install directory. To be robust against other working directories, also walk up from
    // the current directory looking for <dir>/data/predefined-directshows.json (covers e.g. running from a build/ subfolder).
    std::vector<std::string> candidates;
    candidates.push_back(kDefaultFilePath);

    try {
        namespace fs = std::filesystem;
        auto cur = fs::current_path();
        for (int i = 0; i < 6 && !cur.empty(); ++i) {
            candidates.push_back((cur / "data" / "predefined-directshows.json").string());
            if (!cur.has_parent_path() || cur.parent_path() == cur)
                break;
            cur = cur.parent_path();
        }
    } catch (...) {}

    for (const auto& c : candidates) {
        try {
            if (std::filesystem::exists(c))
                return c;
        } catch (...) {}
    }
    return "";
}

bool DirectShowPathsConfig::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        sgct::Log::Warning(std::format("DirectShowPathsConfig: cannot open file '{}'", filePath));
        m_loaded = false;
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();

    try {
        nlohmann::json doc = nlohmann::json::parse(ss.str());

        if (!doc.contains("directshows") || !doc["directshows"].is_array()) {
            sgct::Log::Error("DirectShowPathsConfig: JSON must contain a 'directshows' array");
            m_loaded = false;
            return false;
        }

        std::map<std::string, Entry> entries;
        for (const auto& s : doc["directshows"]) {
            if (!s.contains("title") || !s["title"].is_string()) continue;
            const std::string title = s["title"].get<std::string>();
            if (entries.count(title)) continue; // First entry wins on duplicate titles

            Entry e;
            e.devices.videoDevice = deviceField(s, "videoDevice");
            e.devices.audioDevice = deviceField(s, "audioDevice");
            if (s.contains("devices") && s["devices"].is_object()) {
                e.hasPerRole = true;
                for (auto it = s["devices"].begin(); it != s["devices"].end(); ++it) {
                    const auto& v = it.value();
                    // Empty strings and nulls both mean "intentionally no capture on that machine"
                    DevicePair pair;
                    if (v.is_object()) {
                        pair.videoDevice = deviceField(v, "videoDevice");
                        pair.audioDevice = deviceField(v, "audioDevice");
                    }
                    e.perRole[it.key()] = pair;
                }
            }

            entries[title] = std::move(e);
        }

        m_entries = std::move(entries);
        sgct::Log::Info(std::format("DirectShowPathsConfig: loaded {} DirectShow setup entries", m_entries.size()));
        m_loaded = true;
        return true;
    }
    catch (const std::exception& e) {
        sgct::Log::Error(std::format("DirectShowPathsConfig: JSON parse error: {}", e.what()));
        m_loaded = false;
        return false;
    }
}

bool DirectShowPathsConfig::isLoaded() const {
    return m_loaded;
}

int DirectShowPathsConfig::entryCount() const {
    return static_cast<int>(m_entries.size());
}

bool DirectShowPathsConfig::resolveDevicesForRole(const std::string& title, const std::string& role, std::string& outVideoDevice, std::string& outAudioDevice) const {
    auto it = m_entries.find(title);
    if (it == m_entries.end())
        return false;

    const Entry& e = it->second;

    // 1. Explicit per-machine override wins, even when both devices are empty (intentional no-capture).
    if (e.hasPerRole) {
        auto pit = e.perRole.find(role);
        if (pit != e.perRole.end()) {
            outVideoDevice = pit->second.videoDevice;
            outAudioDevice = pit->second.audioDevice;
            return true;
        }
    }

    // 2. Plain default device pair (may be empty).
    outVideoDevice = e.devices.videoDevice;
    outAudioDevice = e.devices.audioDevice;
    return true;
}