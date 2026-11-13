/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODEIDENTITYCONFIG_H
#define NODEIDENTITYCONFIG_H

#include <map>
#include <string>

// Resolves the logical node identifier for this render node from a local JSON file.
// Each node in the cluster has a unique identifier (e.g. "node-A") that is used
// to look up per-node file paths (multi-video compositions, stream/DirectShow/capture
// per-node resolution, node capture inventory).
//
// JSON format (default path: ./data/nodes.json; older installs used ./data/multivideo/nodes.json,
// which is still read when ./data/nodes.json does not exist):
// {
//   "nodes": [
//     { "id": "node-A", "ip": "10.0.0.11" },
//     { "id": "node-B", "ip": "10.0.0.12" }
//   ]
// }
//
class NodeIdentityConfig {
public:
    static const std::string kDefaultFilePath;
    // Location used before nodes.json moved up to data/. Read only as a fallback.
    static const std::string kLegacyFilePath;

    NodeIdentityConfig();

    // Locate nodes.json. Tries the default CWD-relative path first, then walks up from the current directory looking for <dir>/data/nodes.json (covers e.g. running from a build/ subfolder). If none is found, the same search is repeated for the legacy data/multivideo/nodes.json. Returns "" if not found.
    static std::string findDefaultFilePath();

    // True when path is a nodes.json in the legacy data/multivideo folder.
    static bool isLegacyPath(const std::string& path);

    // Load once per process, using the first supplied path. All instances reuse
    // that result (including failures); changes require a restart. Returns true on success.
    bool loadFromFile(const std::string& filePath = kDefaultFilePath);

    // Return the node identifier for a given IP address.
    // Returns empty string if not found.
    std::string nodeIdForAddress(const std::string& address) const;

    // Convenience: determine this node's identifier using sgct engine's own address.
    // Returns empty string if not found or if SGCT is not initialised.
    std::string thisNodeId() const;

    // Whether any entries were loaded.
    bool isLoaded() const;

    // All entries, ip -> id.
    const std::map<std::string, std::string>& entries() const { return m_ipToId; }

private:
    bool readFromFile(const std::string& filePath);

    // ip -> id
    std::map<std::string, std::string> m_ipToId;
    bool m_loaded = false;
};

#endif // NODEIDENTITYCONFIG_H
