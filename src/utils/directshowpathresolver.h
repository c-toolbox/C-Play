/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIRECTSHOWPATHRESOLVER_H
#define DIRECTSHOWPATHRESOLVER_H

#include <chrono>
#include <mutex>
#include <set>
#include <string>
#include <utils/directshowpathsconfig.h>
#include <utils/nodeidentityconfig.h>

// Resolves, on this machine, which local video/audio capture devices a predefined DirectShow setup entry (identified by its title) should use.
//
// The machine's role is "master" on the master, and otherwise the node id from data/multivideo/nodes.json for this
// machine's IP address (falling back to the raw IP address when no id can be resolved). Both the setup list and the
// node identity are re-read at most once per refresh interval, so edits to the JSON files take effect without a restart.
class DirectShowPathResolver {
public:
    static DirectShowPathResolver& instance();

    // Resolve the local capture devices for a predefined DirectShow setup entry (by title) on this machine.
    // Returns false if no entry with that title exists in the local predefined-directshows.json
    // (caller should fall back to the synced device pair).
    // When true, both out values may be empty - meaning "no capture on this machine".
    bool resolve(const std::string& setupKey, bool isMaster, std::string& outVideoDevice, std::string& outAudioDevice);

private:
    DirectShowPathResolver() = default;

    void refreshIfNeeded();
    std::string roleFor(bool isMaster) const;

    mutable std::mutex m_mutex;
    DirectShowPathsConfig m_paths;
    NodeIdentityConfig m_identity;
    std::chrono::steady_clock::time_point m_lastRefresh{};
    // Setup keys already reported as having no local capture on this machine (avoids log spam).
    std::set<std::string> m_warnedNoCapture;
};

#endif // DIRECTSHOWPATHRESOLVER_H