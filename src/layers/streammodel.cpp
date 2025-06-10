/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "streammodel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

StreamModel::StreamModel(QObject *parent)
    : QAbstractListModel(parent) {
}

StreamModel::~StreamModel() {
}

int StreamModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_streamPaths.size();
}

QVariant StreamModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_streamPaths.empty())
        return QVariant();

    if (!checkIndex(index)) {
        return QVariant();
    }
    if (role == pathRole) {
        return m_streamPaths.at(index.row());
    }
    if (role == titleRole) {
        return m_streamTitles.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> StreamModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[pathRole] = "path";
    roles[titleRole] = "title";
    return roles;
}

void StreamModel::updateStreamsList() {
    QFile streamsFile(QStringLiteral("./data/predefined-streams.json"));

    if (!streamsFile.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open predefined-streams file.");
        return;
    }

    QByteArray streamsArray = streamsFile.readAll();
    QJsonDocument streamsDoc(QJsonDocument::fromJson(streamsArray));
    QJsonObject streamObject = streamsDoc.object();

    if (streamObject.contains(QStringLiteral("streams"))) {
        beginResetModel();
        m_streamTitles.clear();
        m_streamPaths.clear();
        QJsonValue streamValues = streamObject.value(QStringLiteral("streams"));
        QJsonArray arr = streamValues.toArray();
        for (auto v : arr) {
            QJsonObject o = v.toObject();
            bool streamEnabled = true;
            if (o.contains(QStringLiteral("enabled"))) {
                streamEnabled = o.value(QStringLiteral("enabled")).toBool();
            }
            if (streamEnabled && o.contains(QStringLiteral("title")) && o.contains(QStringLiteral("path"))) {
                m_streamTitles.append(o.value(QStringLiteral("title")).toString());
                m_streamPaths.append(o.value(QStringLiteral("path")).toString());
            }
        }
        endResetModel();
        Q_EMIT streamsListChanged();
    }
}

int StreamModel::getNumberOfStreams() {
    return m_streamPaths.size();
}