/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "multivideohandler.h"
#include <layers/videolayer.h>
#include "locationsettings.h"
#include <sgct/sgct.h>

#include <QDir>
#include <QFileInfo>

// Resolve a potentially relative file path against known search locations.
// Mirrors the same logic as MpvObject::checkAndCorrectPath() (mpvobject.cpp:860-879).
static std::string resolveFilePath(const std::string& filePath) {
    QString qPath = QString::fromStdString(filePath);
    qPath.replace(QStringLiteral("file:///"), QStringLiteral(""));

    QFileInfo fileInfo(qPath);
    if (fileInfo.exists())
        return qPath.toStdString();

    if (fileInfo.isRelative()) {
        // Search common media locations (mirrors MpvObject::loadFile search paths)
        QStringList searchPaths;
        // Add the directory of a hypothetical config file's parent as base
        // (in practice, users should use absolute paths or paths relative to CWD)
        searchPaths.append(QDir::currentPath());
        searchPaths.append(LocationSettings::cPlayFileLocation());
        searchPaths.append(LocationSettings::cPlayMediaLocation());
        searchPaths.append(LocationSettings::univiewVideoLocation());

        for (int i = 0; i < searchPaths.size(); ++i) {
            QString newFilePath = QDir::cleanPath(searchPaths[i] + QDir::separator() + qPath);
            QFileInfo newFileInfo(newFilePath);
            if (newFileInfo.exists())
                return newFilePath.toStdString();
        }

        // Try network share format (Windows UNC path conversion)
        QString sharePath = qPath;
        sharePath.replace(QStringLiteral("file://"), QStringLiteral("\\\\"));
        QFileInfo shareFileInfo(sharePath);
        if (shareFileInfo.exists())
            return sharePath.toStdString();
    }

    // Return original path if not found — let mpv handle the error
    return filePath;
}


MultiVideoHandler::MultiVideoHandler() {}

MultiVideoHandler::~MultiVideoHandler() {
    clearAll();
}

void MultiVideoHandler::setConfig(const MultiVideoConfig& config,
                                   const std::string& nodeId,
                                   BaseLayer::gl_adress_func_v1 opa,
                                   bool allowDirectRendering,
                                   bool loggingOn,
                                   const std::string& logLevel) {
    clearAll();

    if (!config.isValid()) {
        sgct::Log::Warning("MultiVideoHandler: config has no valid entries, no sub-layers created");
        return;
    }

    // Get master parameters to use as defaults for all sub-players
    const auto& masterParams = config.masterParams();

    const auto& entries = config.entries();
    for (const auto& entry : entries) {
        std::string filePath = MultiVideoConfig::resolvePathForNode(entry, nodeId);

        // Resolve relative paths against known locations (mirrors MpvObject::checkAndCorrectPath)
        if (!filePath.empty()) {
            filePath = resolveFilePath(filePath);
        }

        if (filePath.empty()) {
            sgct::Log::Warning(std::format(
                "MultiVideoHandler: no path resolved for entry '{}' on node '{}', skipping",
                entry.name, nodeId));
            continue;
        }

        auto sub = std::make_shared<VideoLayer>(opa, allowDirectRendering, loggingOn, logLevel);
        sub->setIsMaster(false);

        // Apply master parameters as base defaults first (if master file is specified)
        if (!masterParams.file.empty()) {
            sub->setGridMode(masterParams.gridMode);
            sub->setStereoMode(masterParams.stereoMode);
            sub->enableAudio(masterParams.audio);
        }

        // Then override with per-entry values from JSON (per-entry always takes precedence)
        sub->setGridMode(entry.gridMode);
        sub->setStereoMode(entry.stereoMode);
        sub->setEyeMode(entry.eyeMode);

        if (entry.gridMode == static_cast<uint8_t>(BaseLayer::GridMode::Plane)) {
            sub->setPlaneElevation(entry.planeElevation);
            sub->setPlaneAzimuth(entry.planeAzimuth);
            sub->setPlaneRoll(entry.planeRoll);
            sub->setPlaneDistance(entry.planeDistance);
            sub->setPlaneSize(glm::vec2(static_cast<float>(entry.planeWidth),
                                         static_cast<float>(entry.planeHeight)),
                              entry.planeAspectRatioConsideration);
        } else {
            sub->setRotate(entry.rotate);
            sub->setTranslate(entry.translate);
        }

        if (entry.roiEnabled) {
            sub->setRoiEnabled(entry.roiEnabled);
            sub->setRoi(entry.roi);
        }

        sub->enableAudio(entry.audio);
        sub->setAlpha(1.f);
        sub->setShouldUpdate(true);

        // Apply optional separate audio file via setAudioFile() before loading.
        // This passes the audio-file parameter to mpv's loadfile command,
        // consistent with how MpvObject::loadItem handles it for cplayfiles.
        if (!entry.audioFile.empty()) {
            sub->setAudioFile(entry.audioFile);
        }

        // Initialize and load the file
        sub->initializeAndLoad(filePath);

        sgct::Log::Info(std::format(
            "MultiVideoHandler: created sub-layer '{}' -> '{}'", entry.name, filePath));

        m_subLayers.push_back(sub);
    }

    m_active = !m_subLayers.empty();
    sgct::Log::Info(std::format("MultiVideoHandler: {} sub-layers active", m_subLayers.size()));
}

bool MultiVideoHandler::isActive() const {
    return m_active;
}

bool MultiVideoHandler::hasSubLayers() const {
    return m_active && !m_subLayers.empty();
}

std::vector<std::shared_ptr<BaseLayer>>& MultiVideoHandler::subLayers() const {
    return m_subLayers;
}

void MultiVideoHandler::updateSubLayers() {
    for (auto& sub : m_subLayers) {
        if (sub) {
            sub->updateFrame();
        }
    }
}

void MultiVideoHandler::applyTime(double pos, bool dirty) {
    for (auto& sub : m_subLayers) {
        if (sub) {
            sub->setTimePosition(pos, dirty);
        }
    }
}

void MultiVideoHandler::applyPause(bool paused) {
    for (auto& sub : m_subLayers) {
        if (sub) {
            sub->setTimePause(paused, false);
        }
    }
}

void MultiVideoHandler::applyEofMode(int eofMode) {
    // Each sub-player has its own eofMode from the JSON config.
    // We only propagate a "loop" mode from the master if sub-players
    // are in default/loop mode; otherwise keep per-entry eofMode.
    // Currently we forward to all sub-players for simplicity.
    (void)eofMode; // Intentionally not overriding per-entry eofMode
}

void MultiVideoHandler::applyLoopTime(double A, double B, bool enabled) {
    for (auto& sub : m_subLayers) {
        if (sub) {
            sub->setLoopTime(A, B, enabled);
        }
    }
}

void MultiVideoHandler::applyValue(const std::string& param, int val) {
    for (auto& sub : m_subLayers) {
        if (sub) {
            sub->setValue(param, val);
        }
    }
}

void MultiVideoHandler::clearAll() {
    for (auto& sub : m_subLayers) {
        if (sub) {
            sub->stop();
        }
    }
    m_subLayers.clear();
    m_active = false;
}
