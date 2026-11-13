/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MULTIVIDEOCOMPOSER_H
#define MULTIVIDEOCOMPOSER_H

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

// Backend for the Multi-Video Composer window (MultiVideoComposer.qml): reads and writes
// .cplaymulti files and checks them, so the QML side only edits a plain data structure.
//
// The editable structure ("composition") is a normalized form of the .cplaymulti JSON:
//   {
//     master: { file, gridMode, stereoMode, audio: bool, audioFile },
//     videos: [ {
//         name, type: "video"|"audio", eyeMode, gridMode, stereoMode,
//         audio: bool, audioFile, pathTemplate,
//         nodes: [ { id, file, audio: "default"|"on"|"off", audioFile } ],
//         extra: { ...keys the composer does not edit (plane, rotate, translate, roi) }
//     } ],
//     extra: { ...unknown top-level keys }
//   }
// Keys the composer does not edit are kept in "extra" and written back unchanged.
class MultiVideoComposer : public QObject {
    Q_OBJECT
public:
    explicit MultiVideoComposer(QObject *parent = nullptr);

    // An empty composition with one video entry.
    Q_INVOKABLE QVariantMap newComposition() const;
    Q_INVOKABLE QVariantMap newEntry(const QString &type, const QString &name) const;

    // Reads a .cplaymulti file. Returns { ok: bool, error: string, composition: {...} }.
    Q_INVOKABLE QVariantMap load(const QString &path) const;

    // Writes the composition as .cplaymulti JSON. Returns "" on success, else an error message.
    Q_INVOKABLE QString save(const QString &path, const QVariantMap &composition) const;

    // The .cplaymulti JSON text for a composition (for preview).
    Q_INVOKABLE QString toJson(const QVariantMap &composition) const;

    // Node ids listed in nodes.json (empty if the file is not found).
    Q_INVOKABLE QStringList nodeIdsFromNodesFile() const;
    // The nodes.json in use, or "" when none exists yet.
    Q_INVOKABLE QString nodesFilePath() const;
    // Where nodes.json is (or will be) saved: the existing file, else ./data/multivideo/nodes.json.
    Q_INVOKABLE QString nodesSavePath() const;

    // Reads nodes.json. Returns { ok, error, path, exists, nodes: [ { id, ip } ] }.
    Q_INVOKABLE QVariantMap loadNodes() const;
    // Writes nodes.json (creating data/multivideo when needed). Returns "" or an error message.
    Q_INVOKABLE QString saveNodes(const QVariantList &nodes) const;

    // Which audio sources play where, for the master and each node id:
    //   [ { target: "Master"|nodeId, isMaster: bool, sources: [ "entry name (detail)", ... ] } ]
    Q_INVOKABLE QVariantList audioRouting(const QVariantMap &composition, const QStringList &nodeIds) const;

    // Problems in a composition: [ { level: "error"|"warning"|"info", text } ].
    Q_INVOKABLE QVariantList validate(const QVariantMap &composition, const QStringList &nodeIds) const;

    // File dialog helpers.
    Q_INVOKABLE QString urlToPath(const QUrl &url) const;
    Q_INVOKABLE QUrl folderUrlOf(const QString &path) const;
    Q_INVOKABLE bool fileExists(const QString &path) const;
};

#endif // MULTIVIDEOCOMPOSER_H
