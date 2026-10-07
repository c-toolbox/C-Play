/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "capturepresets.h"
#include <utils/nodeidentityconfig.h>
#include <sgct/sgct.h>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <nlohmann/json.hpp>

/*static*/ const std::string CapturePresets::kDefaultFilePath = "./data/predefined-captures.json";

namespace {

constexpr std::chrono::seconds kRefreshInterval{5};

// Applies the source fields present in o on top of base.
CaptureSource parseSource(const nlohmann::json& o, CaptureSource base) {
    if (o.contains("backend") && o["backend"].is_string())
        base.backend = o["backend"].get<std::string>();
    if (o.contains("input") && o["input"].is_number_integer())
        base.input = o["input"].get<int>();
    if (o.contains("ganging") && o["ganging"].is_string())
        base.ganging = o["ganging"].get<std::string>();
    if (o.contains("directGpu") && o["directGpu"].is_boolean())
        base.directGpu = o["directGpu"].get<bool>();
    if (o.contains("audio") && o["audio"].is_boolean())
        base.audio = o["audio"].get<bool>();
    return base;
}

std::string roleFor(bool isMaster, const NodeIdentityConfig& identity) {
    if (isMaster)
        return "master";
    std::string nodeId;
    try {
        nodeId = identity.thisNodeId();
    } catch (...) {}
    if (!nodeId.empty())
        return nodeId;
    try {
        return sgct::Engine::instance().thisNode().address();
    } catch (...) {}
    return "";
}

struct Resolver {
    std::mutex mutex;
    CapturePresets presets;
    NodeIdentityConfig identity;
    std::chrono::steady_clock::time_point lastRefresh{};
    std::set<std::string> reportedNoCapture;

    void refreshIfNeeded() {
        const auto now = std::chrono::steady_clock::now();
        if (lastRefresh.time_since_epoch().count() != 0 && now - lastRefresh < kRefreshInterval)
            return;
        lastRefresh = now;

        const std::string presetsPath = CapturePresets::findDefaultFilePath();
        if (!presetsPath.empty())
            presets.loadFromFile(presetsPath);
        const std::string nodesPath = NodeIdentityConfig::findDefaultFilePath();
        if (!nodesPath.empty())
            identity.loadFromFile(nodesPath);
    }
};

Resolver& resolver() {
    static Resolver r;
    return r;
}

} // namespace

/*static*/
std::string CapturePresets::findDefaultFilePath() {
    std::vector<std::string> candidates{kDefaultFilePath};
    try {
        namespace fs = std::filesystem;
        auto cur = fs::current_path();
        for (int i = 0; i < 6 && !cur.empty(); ++i) {
            candidates.push_back((cur / "data" / "predefined-captures.json").string());
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

bool CapturePresets::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        sgct::Log::Warning(std::format("CapturePresets: cannot open file '{}'", filePath));
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();

    try {
        const nlohmann::json doc = nlohmann::json::parse(ss.str(), nullptr, true, true);
        if (!doc.contains("captures") || !doc["captures"].is_array()) {
            sgct::Log::Error("CapturePresets: JSON must contain a 'captures' array");
            return false;
        }

        std::vector<Entry> entries;
        std::set<std::string> titles;
        for (const auto& c : doc["captures"]) {
            if (!c.is_object() || !c.contains("title") || !c["title"].is_string())
                continue;
            Entry e;
            e.title = c["title"].get<std::string>();
            if (!titles.insert(e.title).second)
                continue; // first entry wins on duplicate titles
            if (c.contains("enabled") && c["enabled"].is_boolean())
                e.enabled = c["enabled"].get<bool>();
            e.source = parseSource(c, CaptureSource{});
            if (c.contains("sources") && c["sources"].is_object()) {
                for (auto it = c["sources"].begin(); it != c["sources"].end(); ++it) {
                    CaptureSource roleSource = e.source;
                    if (it.value().is_object())
                        roleSource = parseSource(it.value(), e.source);
                    else
                        roleSource.input = 0; // null: no capture on that machine
                    e.perRole[it.key()] = roleSource;
                }
            }
            entries.push_back(std::move(e));
        }
        m_entries = std::move(entries);
        return true;
    } catch (const std::exception& ex) {
        sgct::Log::Error(std::format("CapturePresets: JSON parse error: {}", ex.what()));
        return false;
    }
}

bool CapturePresets::resolveForRole(const std::string& title, const std::string& role, CaptureSource& out) const {
    for (const Entry& e : m_entries) {
        if (e.title != title)
            continue;
        const auto it = e.perRole.find(role);
        out = it != e.perRole.end() ? it->second : e.source;
        return true;
    }
    return false;
}

/*static*/
bool CapturePresets::resolve(const std::string& title, bool isMaster, CaptureSource& out) {
    Resolver& r = resolver();
    std::lock_guard<std::mutex> lock(r.mutex);
    r.refreshIfNeeded();

    const std::string role = roleFor(isMaster, r.identity);
    if (!r.presets.resolveForRole(title, role, out))
        return false;

    if (!out.valid() && r.reportedNoCapture.insert(title).second) {
        sgct::Log::Info(std::format("CapturePresets: setup '{}' has no capture source for role '{}' - nothing is captured on this machine",
                                    title, role.empty() ? std::string("<unknown>") : role));
    }
    return true;
}
