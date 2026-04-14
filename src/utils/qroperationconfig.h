/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef QROPERATIONCONFIG_H
#define QROPERATIONCONFIG_H

#include <string>
#include <vector>
#include <cstdint>

// Defines the grid/placement properties for a single named plane
// that can be controlled by QR code operations.
// Each field has a corresponding bit in 'specified' — only fields
// explicitly present in the JSON will have their bit set.
// Unset fields should be inherited from the parent layer at runtime.
struct QRPlaneDefinition {
    enum Field : uint8_t {
        Height    = 1 << 0,
        Azimuth   = 1 << 1,
        Elevation = 1 << 2,
        Roll      = 1 << 3,
        Distance  = 1 << 4,
    };

    std::string name;
    double height = 0.0;
    double azimuth = 0.0;
    double elevation = 0.0;
    double roll = 0.0;
    double distance = 0.0;
    uint8_t specified = 0;   // bitmask of Field values

    bool has(Field f) const { return (specified & f) != 0; }
};

// Loadable configuration for QR code operations.
// Defines an arbitrary number of named planes, each with grid values.
// The first plane in the list is considered the default active plane.
//
// JSON format — every field except "name" is optional:
// {
//   "planes": [
//     { "name": "Front",  "height": 3.5, "azimuth": 0.0, "elevation": 35.0 },
//     { "name": "Left",   "azimuth": -75.0, "elevation": 26.5 },
//     { "name": "Right",  "azimuth": 75.0,  "elevation": 26.5 },
//     { "name": "Top",    "elevation": 75.0 }
//   ]
// }
//
class QROperationConfig {
public:
    QROperationConfig();

    // Load configuration from a JSON file. Returns true on success.
    bool loadFromFile(const std::string& filePath);

    // Load configuration from a JSON string. Returns true on success.
    bool loadFromString(const std::string& jsonStr);

    // Populate the config with the default planes (Front, Left, Right, Top).
    void loadDefaults();

    // Access the list of plane definitions.
    const std::vector<QRPlaneDefinition>& planes() const;

    // Find a plane definition by name. Returns nullptr if not found.
    const QRPlaneDefinition* findPlane(const std::string& name) const;

    // Check if a plane with the given name exists.
    bool hasPlane(const std::string& name) const;

private:
    std::vector<QRPlaneDefinition> m_planes;
};

#endif // QROPERATIONCONFIG_H
