/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodeidentityconfig.h"
#include <sgct/sgct.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

/*static*/ const std::string NodeIdentityConfig::kDefaultFilePath = "./data/multivideo/nodes.json";

NodeIdentityConfig::NodeIdentityConfig() {}

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
