/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodeidentityconfig.h"
#include <sgct/sgct.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <nlohmann/json.hpp>

/*static*/ const std::string NodeIdentityConfig::kDefaultFilePath = "./data/multivideo/nodes.json";

NodeIdentityConfig::NodeIdentityConfig() {}

/*static*/
std::string NodeIdentityConfig::findDefaultFilePath() {
    // The default path is CWD-relative ("./data/multivideo/nodes.json"), which only works when the app is
    // launched from its install directory. To be robust against other working directories, also walk up from
    // the current directory looking for <dir>/data/multivideo/nodes.json (covers e.g. running from a build/ subfolder).
    std::vector<std::string> candidates;
    candidates.push_back(kDefaultFilePath);

    try {
        namespace fs = std::filesystem;
        auto cur = fs::current_path();
        for (int i = 0; i < 6 && !cur.empty(); ++i) {
            candidates.push_back((cur / "data" / "multivideo" / "nodes.json").string());
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

bool NodeIdentityConfig::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        sgct::Log::Warning(std::format("NodeIdentityConfig: cannot open file '{}'", filePath));
        m_loaded = false;
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();

    try {
        nlohmann::json doc = nlohmann::json::parse(ss.str());

        if (!doc.contains("nodes") || !doc["nodes"].is_array()) {
            sgct::Log::Error("NodeIdentityConfig: JSON must contain a 'nodes' array");
            m_loaded = false;
            return false;
        }

        m_ipToId.clear();
        for (const auto& n : doc["nodes"]) {
            if (!n.contains("id") || !n.contains("ip")) continue;
            if (!n["id"].is_string() || !n["ip"].is_string()) continue;
            m_ipToId[n["ip"].get<std::string>()] = n["id"].get<std::string>();
        }

        sgct::Log::Info(std::format("NodeIdentityConfig: loaded {} node entries", m_ipToId.size()));
        m_loaded = true;
        return true;
    }
    catch (const std::exception& e) {
        sgct::Log::Error(std::format("NodeIdentityConfig: JSON parse error: {}", e.what()));
        m_loaded = false;
        return false;
    }
}

std::string NodeIdentityConfig::nodeIdForAddress(const std::string& address) const {
    auto it = m_ipToId.find(address);
    if (it != m_ipToId.end())
        return it->second;
    return "";
}

std::string NodeIdentityConfig::thisNodeId() const {
    try {
        const std::string addr = sgct::Engine::instance().thisNode().address();
        return nodeIdForAddress(addr);
    }
    catch (...) {
        return "";
    }
}

bool NodeIdentityConfig::isLoaded() const {
    return m_loaded;
}
