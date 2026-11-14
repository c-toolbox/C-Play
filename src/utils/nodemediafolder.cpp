/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodemediafolder.h"
#include "nodeidentityconfig.h"

#include <sgct/sgct.h>
#include <nlohmann/json.hpp>

#include <QDir>
#include <QFileInfo>
#include <QSet>

/*static*/ const QString NodeMediaFolder::kDefaultFolder = QStringLiteral("default");
/*static*/ const QString NodeMediaFolder::kFolderSuffix = QStringLiteral("_NODES");

namespace {
QString localPath(const QString& file) {
    QString path = file;
    path.replace(QStringLiteral("file:///"), QStringLiteral(""));
    return path;
}

std::string nodeEntryLog(const NodeMediaEntry& e) {
    if (e.video.empty())
        return std::format("audio only ({} file(s))", e.audio.size());
    if (e.audio.empty())
        return "video only";
    return std::format("video + audio ({} file(s))", e.audio.size());
}
} // namespace

/*static*/
bool NodeMediaFolder::isAudioFile(const QString& filePath) {
    static const QSet<QString> audioExtensions = {
        QStringLiteral("wav"), QStringLiteral("w64"), QStringLiteral("mp3"), QStringLiteral("flac"),
        QStringLiteral("aac"), QStringLiteral("ac3"), QStringLiteral("eac3"), QStringLiteral("dts"),
        QStringLiteral("thd"), QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("opus"),
        QStringLiteral("m4a"), QStringLiteral("wma"), QStringLiteral("aif"), QStringLiteral("aiff"),
        QStringLiteral("caf"), QStringLiteral("mka"), QStringLiteral("mp2"), QStringLiteral("ape"),
        QStringLiteral("wv")
    };
    return audioExtensions.contains(QFileInfo(filePath).suffix().toLower());
}

/*static*/
QString NodeMediaFolder::folderFor(const QString& mediaFile) {
    const QString path = localPath(mediaFile);
    if (path.isEmpty() || path.contains(QStringLiteral("://")))
        return {};

    const QFileInfo info(path);
    const QString folder = info.absoluteDir().filePath(info.completeBaseName() + kFolderSuffix);
    if (!QFileInfo(folder).isDir())
        return {};
    return QDir::cleanPath(folder);
}

/*static*/
std::string NodeMediaFolder::buildMapJson(const QString& mediaFile) {
    const QString folder = folderFor(mediaFile);
    if (folder.isEmpty())
        return {};

    const QString baseName = QFileInfo(localPath(mediaFile)).completeBaseName();

    // Node ids known from nodes.json, only used to warn about folders no node will match.
    NodeIdentityConfig identity;
    const std::string nodesPath = NodeIdentityConfig::findDefaultFilePath();
    if (!nodesPath.empty())
        identity.loadFromFile(nodesPath);
    QSet<QString> knownIds;
    for (const auto& [ip, id] : identity.entries())
        knownIds.insert(QString::fromStdString(id).toLower());

    nlohmann::json nodes = nlohmann::json::object();
    const QDir root(folder);
    const auto nodeDirs = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
    for (const QFileInfo& nodeDir : nodeDirs) {
        const QString nodeId = nodeDir.fileName();

        NodeMediaEntry entry;
        QStringList videoCandidates;
        const auto files = QDir(nodeDir.absoluteFilePath()).entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
        for (const QFileInfo& file : files) {
            if (isAudioFile(file.fileName())) {
                entry.audio.push_back(QDir::cleanPath(file.absoluteFilePath()).toStdString());
            } else if (file.completeBaseName().compare(baseName, Qt::CaseInsensitive) == 0) {
                videoCandidates.append(QDir::cleanPath(file.absoluteFilePath()));
            }
        }
        if (!videoCandidates.isEmpty()) {
            entry.video = videoCandidates.first().toStdString();
            if (videoCandidates.size() > 1) {
                sgct::Log::Warning(std::format(
                    "NodeMediaFolder: '{}' has {} files named '{}'; using '{}'",
                    nodeDir.absoluteFilePath().toStdString(), videoCandidates.size(),
                    baseName.toStdString(), entry.video));
            }
        }

        if (entry.video.empty() && entry.audio.empty()) {
            sgct::Log::Warning(std::format(
                "NodeMediaFolder: '{}' has no video named '{}' and no audio files; ignored",
                nodeDir.absoluteFilePath().toStdString(), baseName.toStdString()));
            continue;
        }

        const bool isDefault = nodeId.compare(kDefaultFolder, Qt::CaseInsensitive) == 0;
        if (!isDefault && identity.isLoaded() && !knownIds.contains(nodeId.toLower())) {
            sgct::Log::Warning(std::format(
                "NodeMediaFolder: folder '{}' is not a node id in nodes.json, so no node will use it",
                nodeId.toStdString()));
        }

        sgct::Log::Info(std::format("NodeMediaFolder: '{}' -> {}", nodeId.toStdString(), nodeEntryLog(entry)));

        nodes[nodeId.toStdString()] = {
            { "video", entry.video },
            { "audio", entry.audio }
        };
    }

    if (nodes.empty()) {
        sgct::Log::Warning(std::format("NodeMediaFolder: '{}' has no usable node folders", folder.toStdString()));
        return {};
    }

    nlohmann::json doc;
    doc["folder"] = folder.toStdString();
    doc["nodes"] = nodes;
    return doc.dump();
}

/*static*/
std::optional<NodeMediaEntry> NodeMediaFolder::entryForNode(const std::string& mapJson, const std::string& nodeId) {
    if (mapJson.empty())
        return std::nullopt;

    nlohmann::json doc;
    try {
        doc = nlohmann::json::parse(mapJson);
    } catch (const std::exception& e) {
        sgct::Log::Error(std::format("NodeMediaFolder: cannot parse node media map: {}", e.what()));
        return std::nullopt;
    }
    if (!doc.contains("nodes") || !doc["nodes"].is_object())
        return std::nullopt;
    const auto& nodes = doc["nodes"];

    auto toEntry = [](const nlohmann::json& v) {
        NodeMediaEntry e;
        if (v.contains("video") && v["video"].is_string())
            e.video = v["video"].get<std::string>();
        if (v.contains("audio") && v["audio"].is_array()) {
            for (const auto& a : v["audio"]) {
                if (a.is_string() && !a.get<std::string>().empty())
                    e.audio.push_back(a.get<std::string>());
            }
        }
        return e;
    };

    auto findKey = [&nodes](const QString& key) -> const nlohmann::json* {
        if (key.isEmpty())
            return nullptr;
        const std::string exact = key.toStdString();
        if (nodes.contains(exact))
            return &nodes[exact];
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            if (QString::fromStdString(it.key()).compare(key, Qt::CaseInsensitive) == 0)
                return &it.value();
        }
        return nullptr;
    };

    if (const auto* v = findKey(QString::fromStdString(nodeId)))
        return toEntry(*v);
    if (const auto* v = findKey(kDefaultFolder))
        return toEntry(*v);
    return std::nullopt;
}
