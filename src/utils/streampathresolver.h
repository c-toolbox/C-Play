/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STREAMPATHRESOLVER_H
#define STREAMPATHRESOLVER_H

#include <chrono>
#include <mutex>
#include <set>
#include <string>
#include <utils/streampathsconfig.h>
#include <utils/nodeidentityconfig.h>

// Resolves, on this machine, which local media path a predefined stream entry (identified by its title) should open.
//
// The machine's role is "master" on the master, and otherwise the node id from data/multivideo/nodes.json for this
// machine's IP address (falling back to the raw IP address when no id can be resolved). Both the stream list and the
// node identity are re-read at most once per refresh interval, so edits to the JSON files take effect without a restart.
class StreamPathResolver {
public:
    static StreamPathResolver& instance();

    // Resolve the local path for a predefined stream entry (by title) on this machine.
    // Returns false if no entry with that title exists in the local predefined-streams.json
    // (caller should fall back to the synced file path).
    // When true, outPath may be empty - meaning "no stream on this machine".
    bool resolve(const std::string& streamKey, bool isMaster, std::string& outPath);

private:
    StreamPathResolver() = default;

    void refreshIfNeeded();
    std::string roleFor(bool isMaster) const;

    mutable std::mutex m_mutex;
    StreamPathsConfig m_paths;
    NodeIdentityConfig m_identity;
    std::chrono::steady_clock::time_point m_lastRefresh{};
    // Stream keys already reported as having no local path on this machine (avoids log spam).
    std::set<std::string> m_warnedNoPath;
};

#endif // STREAMPATHRESOLVER_H
