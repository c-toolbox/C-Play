/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "streampathresolver.h"
#include <sgct/sgct.h>

namespace {
// How often (at most) the local JSON files are re-read, so edits take effect without a restart.
constexpr std::chrono::seconds kRefreshInterval{5};
} // namespace

/*static*/ StreamPathResolver& StreamPathResolver::instance() {
    static StreamPathResolver resolver;
    return resolver;
}

void StreamPathResolver::refreshIfNeeded() {
    const auto now = std::chrono::steady_clock::now();
    if (m_lastRefresh.time_since_epoch().count() != 0 && now - m_lastRefresh < kRefreshInterval)
        return;
    m_lastRefresh = now;

    // findDefaultFilePath() only returns a path that exists, so missing files do not produce log spam.
    const std::string streamsPath = StreamPathsConfig::findDefaultFilePath();
    if (!streamsPath.empty()) {
        m_paths.loadFromFile(streamsPath);
    } else if (m_paths.isLoaded()) {
        sgct::Log::Debug("StreamPathResolver: predefined-streams.json not found, keeping last loaded list");
    }

    const std::string nodesPath = NodeIdentityConfig::findDefaultFilePath();
    if (!nodesPath.empty()) {
        m_identity.loadFromFile(nodesPath);
    }
}

std::string StreamPathResolver::roleFor(bool isMaster) const {
    if (isMaster)
        return "master";

    std::string nodeId;
    try {
        nodeId = m_identity.thisNodeId();
    } catch (...) {}
    if (!nodeId.empty())
        return nodeId;

    // Fall back to the raw IP address as key, so paths can be keyed by address when
    // nodes.json has no entry for this machine.
    try {
        return sgct::Engine::instance().thisNode().address();
    } catch (...) {}
    return "";
}

bool StreamPathResolver::resolve(const std::string& streamKey, bool isMaster, std::string& outPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    refreshIfNeeded();

    const std::string role = roleFor(isMaster);
    if (!m_paths.resolvePathForRole(streamKey, role, outPath))
        return false;

    if (outPath.empty() && m_warnedNoPath.insert(streamKey).second) {
        if (role.empty()) {
            sgct::Log::Warning(std::format(
                "StreamPathResolver: stream '{}' has no local path on this machine and the node identity could not be resolved "
                "(add this machine's IP to data/multivideo/nodes.json), so no stream is opened here", streamKey));
        } else {
            sgct::Log::Info(std::format(
                "StreamPathResolver: stream '{}' resolves to an empty path for role '{}', so no stream is opened on this machine",
                streamKey, role));
        }
    }

    return true;
}
