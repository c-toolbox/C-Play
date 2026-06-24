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
// to look up per-node file paths in a MultiVideoConfig.
//
// JSON format (default path: ./data/multivideo/nodes.json):
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

    NodeIdentityConfig();

    // Load configuration from a JSON file. Returns true on success.
    bool loadFromFile(const std::string& filePath = kDefaultFilePath);

    // Return the node identifier for a given IP address.
    // Returns empty string if not found.
    std::string nodeIdForAddress(const std::string& address) const;

    // Convenience: determine this node's identifier using sgct engine's own address.
    // Returns empty string if not found or if SGCT is not initialised.
    std::string thisNodeId() const;

    // Whether any entries were loaded.
    bool isLoaded() const;

private:
    // ip -> id
    std::map<std::string, std::string> m_ipToId;
    bool m_loaded = false;
};

#endif // NODEIDENTITYCONFIG_H
