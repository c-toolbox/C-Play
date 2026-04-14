/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "qroperationconfig.h"
#include <sgct/sgct.h>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

QROperationConfig::QROperationConfig() {
    if (!loadFromFile("./data/impres/config.json")) {
        loadDefaults();
    }
}

bool QROperationConfig::loadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return loadFromString(ss.str());
}

bool QROperationConfig::loadFromString(const std::string& jsonStr) {
    try {
        nlohmann::json doc = nlohmann::json::parse(jsonStr);

        if (!doc.contains("planes") || !doc["planes"].is_array()) {
            sgct::Log::Error("QROperationConfig: JSON must contain a 'planes' array");
            return false;
        }

        std::vector<QRPlaneDefinition> newPlanes;
        for (const auto& planeObj : doc["planes"]) {
            QRPlaneDefinition def;

            if (planeObj.contains("name") && planeObj["name"].is_string()) {
                def.name = planeObj["name"].get<std::string>();
            }
            else {
                sgct::Log::Error("QROperationConfig: Plane definition missing 'name'");
                continue;
            }

            if (planeObj.contains("height") && planeObj["height"].is_number()) {
                def.height = planeObj["height"].get<double>();
                def.specified |= QRPlaneDefinition::Height;
            }
            if (planeObj.contains("azimuth") && planeObj["azimuth"].is_number()) {
                def.azimuth = planeObj["azimuth"].get<double>();
                def.specified |= QRPlaneDefinition::Azimuth;
            }
            if (planeObj.contains("elevation") && planeObj["elevation"].is_number()) {
                def.elevation = planeObj["elevation"].get<double>();
                def.specified |= QRPlaneDefinition::Elevation;
            }
            if (planeObj.contains("roll") && planeObj["roll"].is_number()) {
                def.roll = planeObj["roll"].get<double>();
                def.specified |= QRPlaneDefinition::Roll;
            }
            if (planeObj.contains("distance") && planeObj["distance"].is_number()) {
                def.distance = planeObj["distance"].get<double>();
                def.specified |= QRPlaneDefinition::Distance;
            }

            newPlanes.push_back(def);
        }

        if (newPlanes.empty()) {
            sgct::Log::Error("QROperationConfig: No valid plane definitions found");
            return false;
        }

        m_planes = std::move(newPlanes);
        sgct::Log::Info(std::format("QROperationConfig: Loaded {} plane definitions", m_planes.size()));
        return true;
    }
    catch (const std::exception& e) {
        sgct::Log::Error(std::format("QROperationConfig: JSON parse error: {}", e.what()));
        return false;
    }
}

void QROperationConfig::loadDefaults() {
    m_planes.clear();

    QRPlaneDefinition front;
    front.name = "FrontCapture";
    front.specified = 0;
    m_planes.push_back(front);

    QRPlaneDefinition left;
    left.name = "LeftCapture";
    left.azimuth = -45;
    left.specified = QRPlaneDefinition::Azimuth;
    m_planes.push_back(left);

    QRPlaneDefinition right;
    right.name = "RightCapture";
    right.azimuth = 45;
    right.specified = QRPlaneDefinition::Azimuth;
    m_planes.push_back(right);

    QRPlaneDefinition top;
    top.name = "TopCapture";
    top.elevation = 65;
    top.specified = QRPlaneDefinition::Elevation;
    m_planes.push_back(top);
}

const std::vector<QRPlaneDefinition>& QROperationConfig::planes() const {
    return m_planes;
}

const QRPlaneDefinition* QROperationConfig::findPlane(const std::string& name) const {
    for (const auto& plane : m_planes) {
        if (plane.name == name) {
            return &plane;
        }
    }
    return nullptr;
}

bool QROperationConfig::hasPlane(const std::string& name) const {
    return findPlane(name) != nullptr;
}
