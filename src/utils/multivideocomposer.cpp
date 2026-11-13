/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "multivideocomposer.h"
#include "nodeidentityconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QSet>

#include <nlohmann/json.hpp>

using ojson = nlohmann::ordered_json;

namespace {

const QString kTypeVideo = QStringLiteral("video");
const QString kTypeAudio = QStringLiteral("audio");
const QString kAudioDefault = QStringLiteral("default");
const QString kAudioOn = QStringLiteral("on");
const QString kAudioOff = QStringLiteral("off");

QVariant toVariant(const ojson &j) {
    return QJsonDocument::fromJson(QByteArray::fromStdString(j.dump())).toVariant();
}

// QJsonDocument only wraps objects/arrays, so scalars go through a one-element array.
ojson fromVariant(const QVariant &v) {
    const QByteArray bytes = QJsonDocument::fromVariant(QVariantList{v}).toJson(QJsonDocument::Compact);
    ojson arr = ojson::parse(bytes.toStdString(), nullptr, false);
    if (arr.is_array() && !arr.empty())
        return arr[0];
    return ojson();
}

QString str(const ojson &obj, const char *key, const QString &fallback = QString()) {
    if (obj.contains(key) && obj[key].is_string())
        return QString::fromStdString(obj[key].get<std::string>());
    return fallback;
}

bool boolean(const ojson &obj, const char *key, bool fallback) {
    if (obj.contains(key) && obj[key].is_boolean())
        return obj[key].get<bool>();
    return fallback;
}

QString normalizeEyeMode(const QString &s) {
    if (s.compare(QStringLiteral("left"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("left");
    if (s.compare(QStringLiteral("right"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("right");
    return QStringLiteral("Both");
}

// Keys of an entry / the master that the composer edits; anything else goes to "extra".
const QSet<QString> kEntryKeys = {
    QStringLiteral("name"), QStringLiteral("type"), QStringLiteral("eyeMode"),
    QStringLiteral("gridMode"), QStringLiteral("stereoMode"), QStringLiteral("audio"),
    QStringLiteral("audioFile"), QStringLiteral("pathTemplate"), QStringLiteral("paths")
};
const QSet<QString> kMasterKeys = {
    QStringLiteral("file"), QStringLiteral("gridMode"), QStringLiteral("stereoMode"),
    QStringLiteral("audio"), QStringLiteral("audioFile")
};
const QSet<QString> kTopKeys = { QStringLiteral("master"), QStringLiteral("videos") };

QVariantMap extraKeys(const ojson &obj, const QSet<QString> &known) {
    QVariantMap extra;
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        const QString key = QString::fromStdString(it.key());
        if (!known.contains(key))
            extra.insert(key, toVariant(it.value()));
    }
    return extra;
}

void appendExtra(ojson &obj, const QVariantMap &extra, const QSet<QString> &known) {
    for (auto it = extra.cbegin(); it != extra.cend(); ++it) {
        if (!known.contains(it.key()))
            obj[it.key().toStdString()] = fromVariant(it.value());
    }
}

// --- Resolution on the normalized structure (mirrors MultiVideoConfig on the nodes) ---

QVariantMap nodeRecord(const QVariantMap &entry, const QString &nodeId) {
    const QVariantList nodes = entry.value(QStringLiteral("nodes")).toList();
    for (const QVariant &n : nodes) {
        const QVariantMap rec = n.toMap();
        if (rec.value(QStringLiteral("id")).toString() == nodeId)
            return rec;
    }
    return {};
}

QString resolveFileForNode(const QVariantMap &entry, const QString &nodeId) {
    const QString direct = nodeRecord(entry, nodeId).value(QStringLiteral("file")).toString().trimmed();
    if (!direct.isEmpty())
        return direct;
    QString tmpl = entry.value(QStringLiteral("pathTemplate")).toString().trimmed();
    if (tmpl.isEmpty())
        return {};
    const QString placeholder = QStringLiteral("{nodeId}");
    const int pos = tmpl.indexOf(placeholder);
    if (pos >= 0)
        tmpl.replace(pos, placeholder.size(), nodeId);
    return tmpl;
}

bool isAudioEntry(const QVariantMap &entry) {
    return entry.value(QStringLiteral("type")).toString() == kTypeAudio;
}

QString entryLabel(const QVariantMap &entry, int index) {
    const QString name = entry.value(QStringLiteral("name")).toString().trimmed();
    return name.isEmpty() ? QStringLiteral("#%1").arg(index + 1) : name;
}

QStringList allNodeIds(const QVariantMap &composition, const QStringList &nodeIds) {
    QStringList ids = nodeIds;
    const QVariantList videos = composition.value(QStringLiteral("videos")).toList();
    for (const QVariant &v : videos) {
        const QVariantList nodes = v.toMap().value(QStringLiteral("nodes")).toList();
        for (const QVariant &n : nodes) {
            const QString id = n.toMap().value(QStringLiteral("id")).toString();
            if (!id.isEmpty() && !ids.contains(id))
                ids.append(id);
        }
    }
    return ids;
}

} // namespace

MultiVideoComposer::MultiVideoComposer(QObject *parent)
    : QObject(parent) {}

QVariantMap MultiVideoComposer::newEntry(const QString &type, const QString &name) const {
    QVariantMap e;
    e.insert(QStringLiteral("name"), name);
    e.insert(QStringLiteral("type"), type == kTypeAudio ? kTypeAudio : kTypeVideo);
    e.insert(QStringLiteral("eyeMode"), QStringLiteral("Both"));
    e.insert(QStringLiteral("gridMode"), QStringLiteral("None"));
    e.insert(QStringLiteral("stereoMode"), QStringLiteral("No_2D"));
    e.insert(QStringLiteral("audio"), false);
    e.insert(QStringLiteral("audioFile"), QString());
    e.insert(QStringLiteral("pathTemplate"), QString());
    e.insert(QStringLiteral("nodes"), QVariantList());
    e.insert(QStringLiteral("extra"), QVariantMap());
    return e;
}

QVariantMap MultiVideoComposer::newComposition() const {
    QVariantMap master;
    master.insert(QStringLiteral("file"), QString());
    master.insert(QStringLiteral("gridMode"), QStringLiteral("None"));
    master.insert(QStringLiteral("stereoMode"), QStringLiteral("No_2D"));
    master.insert(QStringLiteral("audio"), true);
    master.insert(QStringLiteral("audioFile"), QString());
    master.insert(QStringLiteral("extra"), QVariantMap());

    QVariantMap comp;
    comp.insert(QStringLiteral("master"), master);
    comp.insert(QStringLiteral("videos"), QVariantList{newEntry(kTypeVideo, QStringLiteral("video1"))});
    comp.insert(QStringLiteral("extra"), QVariantMap());
    return comp;
}

QVariantMap MultiVideoComposer::load(const QString &path) const {
    QVariantMap result;
    result.insert(QStringLiteral("ok"), false);

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.insert(QStringLiteral("error"), tr("Cannot open '%1'.").arg(path));
        return result;
    }
    const ojson doc = ojson::parse(f.readAll().toStdString(), nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        result.insert(QStringLiteral("error"), tr("'%1' is not valid JSON.").arg(path));
        return result;
    }

    QVariantMap master;
    const ojson m = (doc.contains("master") && doc["master"].is_object()) ? doc["master"] : ojson::object();
    master.insert(QStringLiteral("file"), str(m, "file"));
    master.insert(QStringLiteral("gridMode"), str(m, "gridMode", QStringLiteral("None")));
    master.insert(QStringLiteral("stereoMode"), str(m, "stereoMode", QStringLiteral("No_2D")));
    // Absent means the app's audio settings decide, which plays audio on the master.
    master.insert(QStringLiteral("audio"), boolean(m, "audio", true));
    master.insert(QStringLiteral("audioFile"), str(m, "audioFile"));
    master.insert(QStringLiteral("extra"), extraKeys(m, kMasterKeys));

    QVariantList videos;
    if (doc.contains("videos") && doc["videos"].is_array()) {
        for (const auto &v : doc["videos"]) {
            if (!v.is_object())
                continue;
            const QString type = str(v, "type").compare(kTypeAudio, Qt::CaseInsensitive) == 0 ? kTypeAudio : kTypeVideo;
            QVariantMap e = newEntry(type, str(v, "name"));
            e.insert(QStringLiteral("eyeMode"), normalizeEyeMode(str(v, "eyeMode")));
            e.insert(QStringLiteral("gridMode"), str(v, "gridMode", QStringLiteral("None")));
            e.insert(QStringLiteral("stereoMode"), str(v, "stereoMode", QStringLiteral("No_2D")));
            e.insert(QStringLiteral("audio"), boolean(v, "audio", false));
            e.insert(QStringLiteral("audioFile"), str(v, "audioFile"));
            e.insert(QStringLiteral("pathTemplate"), str(v, "pathTemplate"));

            QVariantList nodes;
            if (v.contains("paths") && v["paths"].is_object()) {
                for (auto it = v["paths"].begin(); it != v["paths"].end(); ++it) {
                    QVariantMap rec;
                    rec.insert(QStringLiteral("id"), QString::fromStdString(it.key()));
                    QString audio = kAudioDefault;
                    if (it.value().is_string()) {
                        rec.insert(QStringLiteral("file"), QString::fromStdString(it.value().get<std::string>()));
                        rec.insert(QStringLiteral("audioFile"), QString());
                    } else if (it.value().is_object()) {
                        rec.insert(QStringLiteral("file"), str(it.value(), "file"));
                        rec.insert(QStringLiteral("audioFile"), str(it.value(), "audioFile"));
                        if (it.value().contains("audio") && it.value()["audio"].is_boolean())
                            audio = it.value()["audio"].get<bool>() ? kAudioOn : kAudioOff;
                    } else {
                        continue;
                    }
                    rec.insert(QStringLiteral("audio"), audio);
                    nodes.append(rec);
                }
            }
            e.insert(QStringLiteral("nodes"), nodes);
            e.insert(QStringLiteral("extra"), extraKeys(v, kEntryKeys));
            videos.append(e);
        }
    }

    QVariantMap comp;
    comp.insert(QStringLiteral("master"), master);
    comp.insert(QStringLiteral("videos"), videos);
    comp.insert(QStringLiteral("extra"), extraKeys(doc, kTopKeys));

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("composition"), comp);
    return result;
}

QString MultiVideoComposer::toJson(const QVariantMap &composition) const {
    ojson doc = ojson::object();

    // Master
    const QVariantMap m = composition.value(QStringLiteral("master")).toMap();
    ojson master = ojson::object();
    master["file"] = m.value(QStringLiteral("file")).toString().trimmed().toStdString();
    master["gridMode"] = m.value(QStringLiteral("gridMode"), QStringLiteral("None")).toString().toStdString();
    master["stereoMode"] = m.value(QStringLiteral("stereoMode"), QStringLiteral("No_2D")).toString().toStdString();
    master["audio"] = m.value(QStringLiteral("audio"), true).toBool();
    const QString masterAudioFile = m.value(QStringLiteral("audioFile")).toString().trimmed();
    if (!masterAudioFile.isEmpty())
        master["audioFile"] = masterAudioFile.toStdString();
    appendExtra(master, m.value(QStringLiteral("extra")).toMap(), kMasterKeys);
    doc["master"] = master;

    // Entries
    ojson videos = ojson::array();
    const QVariantList entries = composition.value(QStringLiteral("videos")).toList();
    for (const QVariant &ev : entries) {
        const QVariantMap e = ev.toMap();
        const bool audioOnly = isAudioEntry(e);
        ojson v = ojson::object();
        v["name"] = e.value(QStringLiteral("name")).toString().trimmed().toStdString();
        if (audioOnly) {
            v["type"] = "audio";
        } else {
            v["eyeMode"] = normalizeEyeMode(e.value(QStringLiteral("eyeMode")).toString()).toStdString();
            v["gridMode"] = e.value(QStringLiteral("gridMode"), QStringLiteral("None")).toString().toStdString();
            v["stereoMode"] = e.value(QStringLiteral("stereoMode"), QStringLiteral("No_2D")).toString().toStdString();
            v["audio"] = e.value(QStringLiteral("audio")).toBool();
            const QString audioFile = e.value(QStringLiteral("audioFile")).toString().trimmed();
            if (!audioFile.isEmpty())
                v["audioFile"] = audioFile.toStdString();
        }
        const QString tmpl = e.value(QStringLiteral("pathTemplate")).toString().trimmed();
        if (!tmpl.isEmpty())
            v["pathTemplate"] = tmpl.toStdString();

        ojson paths = ojson::object();
        const QVariantList nodes = e.value(QStringLiteral("nodes")).toList();
        for (const QVariant &nv : nodes) {
            const QVariantMap n = nv.toMap();
            const std::string id = n.value(QStringLiteral("id")).toString().trimmed().toStdString();
            const QString file = n.value(QStringLiteral("file")).toString().trimmed();
            if (id.empty())
                continue;
            if (audioOnly) {
                if (!file.isEmpty())
                    paths[id] = file.toStdString();
                continue;
            }
            const QString audio = n.value(QStringLiteral("audio"), kAudioDefault).toString();
            const QString nodeAudioFile = n.value(QStringLiteral("audioFile")).toString().trimmed();
            const bool hasAudioOverride = (audio == kAudioOn || audio == kAudioOff);
            if (!hasAudioOverride && nodeAudioFile.isEmpty()) {
                if (!file.isEmpty())
                    paths[id] = file.toStdString(); // short form
                continue;
            }
            ojson rec = ojson::object();
            if (!file.isEmpty())
                rec["file"] = file.toStdString();
            if (hasAudioOverride)
                rec["audio"] = (audio == kAudioOn);
            if (!nodeAudioFile.isEmpty())
                rec["audioFile"] = nodeAudioFile.toStdString();
            paths[id] = rec;
        }
        v["paths"] = paths;
        appendExtra(v, e.value(QStringLiteral("extra")).toMap(), kEntryKeys);
        videos.push_back(v);
    }
    doc["videos"] = videos;
    appendExtra(doc, composition.value(QStringLiteral("extra")).toMap(), kTopKeys);

    return QString::fromStdString(doc.dump(2));
}

QString MultiVideoComposer::save(const QString &path, const QVariantMap &composition) const {
    if (path.trimmed().isEmpty())
        return tr("No file name given.");
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return tr("Cannot write '%1': %2").arg(path, f.errorString());
    f.write(toJson(composition).toUtf8());
    f.write("\n");
    if (!f.commit())
        return tr("Cannot write '%1': %2").arg(path, f.errorString());
    return {};
}

QString MultiVideoComposer::nodesFilePath() const {
    const std::string p = NodeIdentityConfig::findDefaultFilePath();
    return p.empty() ? QString() : QFileInfo(QString::fromStdString(p)).absoluteFilePath();
}

QString MultiVideoComposer::nodesSavePath() const {
    // Save over the file in use, unless it is in the old data/multivideo folder: then write the new
    // data/nodes.json, which is found first from then on.
    const QString existing = nodesFilePath();
    if (!existing.isEmpty() && !NodeIdentityConfig::isLegacyPath(existing.toStdString()))
        return existing;
    return QDir::cleanPath(QDir::current().absoluteFilePath(QString::fromStdString(NodeIdentityConfig::kDefaultFilePath)));
}

QVariantMap MultiVideoComposer::loadNodes() const {
    QVariantMap result;
    const QString existing = nodesFilePath();
    const QString path = existing.isEmpty() ? nodesSavePath() : existing;
    result.insert(QStringLiteral("path"), path);
    result.insert(QStringLiteral("exists"), QFileInfo::exists(path));
    result.insert(QStringLiteral("legacy"), !existing.isEmpty() && NodeIdentityConfig::isLegacyPath(existing.toStdString()));
    result.insert(QStringLiteral("savePath"), nodesSavePath());
    result.insert(QStringLiteral("nodes"), QVariantList());
    result.insert(QStringLiteral("ok"), true);
    if (!QFileInfo::exists(path))
        return result;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), tr("Cannot open '%1'.").arg(path));
        return result;
    }
    const ojson doc = ojson::parse(f.readAll().toStdString(), nullptr, false);
    if (doc.is_discarded() || !doc.is_object() || !doc.contains("nodes") || !doc["nodes"].is_array()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("error"), tr("'%1' is not valid: it needs a \"nodes\" array.").arg(path));
        return result;
    }
    QVariantList nodes;
    for (const auto &n : doc["nodes"]) {
        if (!n.is_object())
            continue;
        QVariantMap rec;
        rec.insert(QStringLiteral("id"), str(n, "id"));
        rec.insert(QStringLiteral("ip"), str(n, "ip"));
        nodes.append(rec);
    }
    result.insert(QStringLiteral("nodes"), nodes);
    return result;
}

QString MultiVideoComposer::saveNodes(const QVariantList &nodes) const {
    const QString path = nodesSavePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return tr("Cannot create the folder for '%1'.").arg(path);

    // Same layout as the shipped file: one node per line.
    QStringList lines;
    for (const QVariant &v : nodes) {
        const QVariantMap n = v.toMap();
        const QString id = n.value(QStringLiteral("id")).toString().trimmed();
        const QString ip = n.value(QStringLiteral("ip")).toString().trimmed();
        if (id.isEmpty() && ip.isEmpty())
            continue;
        // ojson(...).dump() gives a correctly escaped JSON string literal.
        lines.append(QStringLiteral("    { \"id\": %1, \"ip\": %2 }")
                         .arg(QString::fromStdString(ojson(id.toStdString()).dump()),
                              QString::fromStdString(ojson(ip.toStdString()).dump())));
    }
    const QString text = QStringLiteral("{\n  \"nodes\": [\n") + lines.join(QStringLiteral(",\n"))
        + (lines.isEmpty() ? QString() : QStringLiteral("\n")) + QStringLiteral("  ]\n}\n");

    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return tr("Cannot write '%1': %2").arg(path, f.errorString());
    f.write(text.toUtf8());
    if (!f.commit())
        return tr("Cannot write '%1': %2").arg(path, f.errorString());
    return {};
}

QStringList MultiVideoComposer::nodeIdsFromNodesFile() const {
    QStringList ids;
    const QString path = nodesFilePath();
    if (path.isEmpty())
        return ids;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return ids;
    const ojson doc = ojson::parse(f.readAll().toStdString(), nullptr, false);
    if (doc.is_discarded() || !doc.contains("nodes") || !doc["nodes"].is_array())
        return ids;
    for (const auto &n : doc["nodes"]) {
        const QString id = n.is_object() ? str(n, "id") : QString();
        if (!id.isEmpty() && !ids.contains(id))
            ids.append(id);
    }
    return ids;
}

QVariantList MultiVideoComposer::audioRouting(const QVariantMap &composition, const QStringList &nodeIds) const {
    QVariantList routing;

    // Master: the reference video's own audio, or its separate audio file.
    {
        const QVariantMap m = composition.value(QStringLiteral("master")).toMap();
        QStringList sources;
        if (m.value(QStringLiteral("audio"), true).toBool() && !m.value(QStringLiteral("file")).toString().trimmed().isEmpty()) {
            const QString af = m.value(QStringLiteral("audioFile")).toString().trimmed();
            sources.append(af.isEmpty() ? tr("Reference video") : tr("Reference video (%1)").arg(QFileInfo(af).fileName()));
        }
        QVariantMap row;
        row.insert(QStringLiteral("target"), tr("Master"));
        row.insert(QStringLiteral("isMaster"), true);
        row.insert(QStringLiteral("sources"), sources);
        routing.append(row);
    }

    const QVariantList entries = composition.value(QStringLiteral("videos")).toList();
    for (const QString &nodeId : allNodeIds(composition, nodeIds)) {
        QStringList sources;
        for (int i = 0; i < entries.size(); ++i) {
            const QVariantMap e = entries[i].toMap();
            const QString file = resolveFileForNode(e, nodeId);
            if (file.isEmpty())
                continue;
            const QString label = entryLabel(e, i);
            if (isAudioEntry(e)) {
                sources.append(tr("%1 (audio: %2)").arg(label, QFileInfo(file).fileName()));
                continue;
            }
            const QVariantMap rec = nodeRecord(e, nodeId);
            const QString audio = rec.value(QStringLiteral("audio"), kAudioDefault).toString();
            const bool on = audio == kAudioOn || (audio != kAudioOff && e.value(QStringLiteral("audio")).toBool());
            if (!on)
                continue;
            QString af = rec.value(QStringLiteral("audioFile")).toString().trimmed();
            if (af.isEmpty())
                af = e.value(QStringLiteral("audioFile")).toString().trimmed();
            sources.append(af.isEmpty() ? tr("%1 (video track)").arg(label)
                                        : tr("%1 (%2)").arg(label, QFileInfo(af).fileName()));
        }
        QVariantMap row;
        row.insert(QStringLiteral("target"), nodeId);
        row.insert(QStringLiteral("isMaster"), false);
        row.insert(QStringLiteral("sources"), sources);
        routing.append(row);
    }
    return routing;
}

QVariantList MultiVideoComposer::validate(const QVariantMap &composition, const QStringList &nodeIds) const {
    QVariantList issues;
    auto add = [&issues](const QString &level, const QString &text) {
        QVariantMap i;
        i.insert(QStringLiteral("level"), level);
        i.insert(QStringLiteral("text"), text);
        issues.append(i);
    };

    const QVariantMap m = composition.value(QStringLiteral("master")).toMap();
    const QString masterFile = m.value(QStringLiteral("file")).toString().trimmed();
    if (masterFile.isEmpty())
        add(QStringLiteral("warning"), tr("No master reference video: the master shows nothing and the timeline has no duration."));
    else if (!QFileInfo::exists(masterFile) && !QDir::isRelativePath(masterFile))
        add(QStringLiteral("warning"), tr("Master reference video not found on this computer: %1").arg(masterFile));

    const QVariantList entries = composition.value(QStringLiteral("videos")).toList();
    if (entries.isEmpty())
        add(QStringLiteral("error"), tr("Add at least one entry; a composition without entries cannot be loaded."));

    const QStringList knownNodes = nodeIdsFromNodesFile();
    const QStringList ids = allNodeIds(composition, nodeIds);
    QSet<QString> names;
    for (int i = 0; i < entries.size(); ++i) {
        const QVariantMap e = entries[i].toMap();
        const QString label = entryLabel(e, i);
        const QString name = e.value(QStringLiteral("name")).toString().trimmed();
        if (name.isEmpty())
            add(QStringLiteral("warning"), tr("Entry %1 has no name.").arg(label));
        else if (names.contains(name))
            add(QStringLiteral("warning"), tr("More than one entry is named '%1'.").arg(name));
        names.insert(name);

        int targeted = 0;
        for (const QString &id : ids) {
            if (!resolveFileForNode(e, id).isEmpty())
                ++targeted;
        }
        const bool hasTemplate = !e.value(QStringLiteral("pathTemplate")).toString().trimmed().isEmpty();
        if (targeted == 0 && !hasTemplate)
            add(QStringLiteral("warning"), tr("'%1' has no file on any node, so it plays nowhere.").arg(label));

        const QVariantList nodes = e.value(QStringLiteral("nodes")).toList();
        for (const QVariant &nv : nodes) {
            const QString id = nv.toMap().value(QStringLiteral("id")).toString();
            const QVariantMap rec = nv.toMap();
            const bool used = !rec.value(QStringLiteral("file")).toString().trimmed().isEmpty()
                || rec.value(QStringLiteral("audio"), kAudioDefault).toString() != kAudioDefault
                || !rec.value(QStringLiteral("audioFile")).toString().trimmed().isEmpty();
            if (used && !knownNodes.isEmpty() && !knownNodes.contains(id))
                add(QStringLiteral("warning"), tr("'%1' uses node '%2', which is not in nodes.json, so no node will match it.").arg(label, id));
        }
    }

    if (knownNodes.isEmpty())
        add(QStringLiteral("info"), tr("nodes.json was not found. Nodes find their node id from their IP address in data/nodes.json; create it with Node Configuration."));

    // Audio routing: more than one source on one machine plays overlapping audio.
    const QVariantList routing = audioRouting(composition, nodeIds);
    bool anyAudio = false;
    for (const QVariant &rv : routing) {
        const QVariantMap r = rv.toMap();
        const QStringList sources = r.value(QStringLiteral("sources")).toStringList();
        if (!sources.isEmpty())
            anyAudio = true;
        if (sources.size() > 1)
            add(QStringLiteral("warning"), tr("%1 plays %2 audio sources at once: %3")
                .arg(r.value(QStringLiteral("target")).toString()).arg(sources.size()).arg(sources.join(QStringLiteral(", "))));
    }
    if (!anyAudio)
        add(QStringLiteral("info"), tr("No audio plays anywhere in this composition."));

    return issues;
}

QString MultiVideoComposer::urlToPath(const QUrl &url) const {
    return url.isLocalFile() ? QDir::fromNativeSeparators(url.toLocalFile()) : url.toString();
}

QUrl MultiVideoComposer::folderUrlOf(const QString &path) const {
    if (path.trimmed().isEmpty())
        return {};
    QFileInfo fi(path);
    const QString dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
    return QDir(dir).exists() ? QUrl::fromLocalFile(dir) : QUrl();
}

bool MultiVideoComposer::fileExists(const QString &path) const {
    return !path.trimmed().isEmpty() && QFileInfo::exists(path);
}
