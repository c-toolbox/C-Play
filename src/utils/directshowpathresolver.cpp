/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "directshowpathresolver.h"
#include <sgct/sgct.h>

namespace {
// How often (at most) the local JSON files are re-read, so edits take effect without a restart.
constexpr std::chrono::seconds kRefreshInterval{5};
} // namespace

/*static*/ DirectShowPathResolver& DirectShowPathResolver::instance() {
    static DirectShowPathResolver resolver;
    return resolver;
}

void DirectShowPathResolver::refreshIfNeeded() {
    const auto now = std::chrono::steady_clock::now();
    if (m_lastRefresh.time_since_epoch().count() != 0 && now - m_lastRefresh < kRefreshInterval)
        return;
    m_lastRefresh = now;

    // findDefaultFilePath() only returns a path that exists, so missing files do not produce log spam.
    const std::string setupsPath = DirectShowPathsConfig::findDefaultFilePath();
    if (!setupsPath.empty()) {
        m_paths.loadFromFile(setupsPath);
    } else if (m_paths.isLoaded()) {
        sgct::Log::Debug("DirectShowPathResolver: predefined-directshows.json not found, keeping last loaded list");
    }

    const std::string nodesPath = NodeIdentityConfig::findDefaultFilePath();
    if (!nodesPath.empty()) {
        m_identity.loadFromFile(nodesPath);
    }
}

std::string DirectShowPathResolver::roleFor(bool isMaster) const {
    if (isMaster)
        return "master";

    std::string nodeId;
    try {
        nodeId = m_identity.thisNodeId();
    } catch (...) {}
    if (!nodeId.empty())
        return nodeId;

    // Fall back to the raw IP address as key, so devices can be keyed by address when
    // nodes.json has no entry for this machine.
    try {
        return sgct::Engine::instance().thisNode().address();
    } catch (...) {}
    return "";
}

bool DirectShowPathResolver::resolve(const std::string& setupKey, bool isMaster, std::string& outVideoDevice, std::string& outAudioDevice) {
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshIfNeeded();

    const std::string role = roleFor(isMaster);
    if (!m_paths.resolveDevicesForRole(setupKey, role, outVideoDevice, outAudioDevice))
        return false;

    if (outVideoDevice.empty() && outAudioDevice.empty() && m_warnedNoCapture.insert(setupKey).second) {
        if (role.empty()) {
            sgct::Log::Warning(std::format(
                "DirectShowPathResolver: setup '{}' has no local capture devices on this machine and the node identity could not be resolved "
                "(add this machine's IP to data/multivideo/nodes.json), so no capture is opened here", setupKey));
        } else {
            sgct::Log::Info(std::format(
                "DirectShowPathResolver: setup '{}' resolves to no capture devices for role '{}', so no capture is opened on this machine",
                setupKey, role));
        }
    }

    return true;
}