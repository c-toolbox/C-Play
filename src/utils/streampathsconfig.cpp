/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "streampathsconfig.h"
#include <sgct/sgct.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>

/*static*/ const std::string StreamPathsConfig::kDefaultFilePath = "./data/predefined-streams.json";

StreamPathsConfig::StreamPathsConfig() {}

/*static*/
std::string StreamPathsConfig::findDefaultFilePath() {
    // The default path is CWD-relative ("./data/predefined-streams.json"), which only works when the app is
    // launched from its install directory. To be robust against other working directories, also walk up from
    // the current directory looking for <dir>/data/predefined-streams.json (covers e.g. running from a build/ subfolder).
    std::vector<std::string> candidates;
    candidates.push_back(kDefaultFilePath);

    try {
        namespace fs = std::filesystem;
        auto cur = fs::current_path();
        for (int i = 0; i < 6 && !cur.empty(); ++i) {
            candidates.push_back((cur / "data" / "predefined-streams.json").string());
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

bool StreamPathsConfig::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        sgct::Log::Warning(std::format("StreamPathsConfig: cannot open file '{}'", filePath));
        m_loaded = false;
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();

    try {
        nlohmann::json doc = nlohmann::json::parse(ss.str());

        if (!doc.contains("streams") || !doc["streams"].is_array()) {
            sgct::Log::Error("StreamPathsConfig: JSON must contain a 'streams' array");
            m_loaded = false;
            return false;
        }

        std::map<std::string, Entry> entries;
        for (const auto& s : doc["streams"]) {
            if (!s.contains("title") || !s["title"].is_string()) continue;
            const std::string title = s["title"].get<std::string>();
            if (entries.count(title)) continue; // First entry wins on duplicate titles

            Entry e;
            if (s.contains("path")) {
                e.path = s["path"].is_null() ? "" : s["path"].get<std::string>();
            }
            if (s.contains("paths") && s["paths"].is_object()) {
                e.hasPaths = true;
                for (auto it = s["paths"].begin(); it != s["paths"].end(); ++it) {
                    const auto& v = it.value();
                    // Empty string and null both mean "intentionally no stream on that machine"
                    e.paths[it.key()] = v.is_string() ? v.get<std::string>() : "";
                }
            }
            if (s.contains("pathTemplate") && s["pathTemplate"].is_string()) {
                e.pathTemplate = s["pathTemplate"].get<std::string>();
            }

            entries[title] = std::move(e);
        }

        m_entries = std::move(entries);
        sgct::Log::Info(std::format("StreamPathsConfig: loaded {} stream entries", m_entries.size()));
        m_loaded = true;
        return true;
    }
    catch (const std::exception& e) {
        sgct::Log::Error(std::format("StreamPathsConfig: JSON parse error: {}", e.what()));
        m_loaded = false;
        return false;
    }
}

bool StreamPathsConfig::isLoaded() const {
    return m_loaded;
}

int StreamPathsConfig::entryCount() const {
    return static_cast<int>(m_entries.size());
}

bool StreamPathsConfig::resolvePathForRole(const std::string& title, const std::string& role, std::string& outPath) const {
    auto it = m_entries.find(title);
    if (it == m_entries.end())
        return false;

    const Entry& e = it->second;

    // 1. Explicit per-machine override wins, even when empty (intentional no-stream).
    if (e.hasPaths) {
        auto pit = e.paths.find(role);
        if (pit != e.paths.end()) {
            outPath = pit->second;
            return true;
        }
    }

    // 2. Template substitution ({nodeId} -> role). Skipped when the machine has no resolvable identity,
    // so an unresolved node falls through to the plain path instead of a broken template result.
    if (!e.pathTemplate.empty() && !role.empty()) {
        const std::string placeholder = "{nodeId}";
        auto pos = e.pathTemplate.find(placeholder);
        if (pos != std::string::npos) {
            outPath = e.pathTemplate;
            outPath.replace(pos, placeholder.size(), role);
            return true;
        }
        outPath = e.pathTemplate;
        return true;
    }

    // 3. Plain default path (may be empty).
    outPath = e.path;
    return true;
}
