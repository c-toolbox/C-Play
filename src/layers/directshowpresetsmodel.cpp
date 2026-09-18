/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "directshowpresetsmodel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

DirectShowPresetsModel::DirectShowPresetsModel(QObject *parent)
    : QAbstractListModel(parent) {
}

DirectShowPresetsModel::~DirectShowPresetsModel() {
}

int DirectShowPresetsModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_titles.size();
}

QVariant DirectShowPresetsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_titles.empty())
        return QVariant();

    if (!checkIndex(index)) {
        return QVariant();
    }
    if (role == videoDeviceRole) {
        return m_videoDevices.at(index.row());
    }
    if (role == audioDeviceRole) {
        return m_audioDevices.at(index.row());
    }
    if (role == titleRole) {
        return m_titles.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> DirectShowPresetsModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[videoDeviceRole] = "videoDevice";
    roles[audioDeviceRole] = "audioDevice";
    roles[titleRole] = "title";
    return roles;
}

void DirectShowPresetsModel::updatePresetsList() {
    QFile presetsFile(QStringLiteral("./data/predefined-directshows.json"));

    if (!presetsFile.open(QIODevice::ReadOnly)) {
        // No predefined setups on this machine - the UI falls back to the custom device selection.
        return;
    }

    QByteArray presetsArray = presetsFile.readAll();
    QJsonDocument presetsDoc(QJsonDocument::fromJson(presetsArray));
    QJsonObject presetObject = presetsDoc.object();

    if (presetObject.contains(QStringLiteral("directshows"))) {
        beginResetModel();
        m_titles.clear();
        m_videoDevices.clear();
        m_audioDevices.clear();
        QJsonValue presetValues = presetObject.value(QStringLiteral("directshows"));
        QJsonArray arr = presetValues.toArray();
        for (auto v : arr) {
            QJsonObject o = v.toObject();
            bool presetEnabled = true;
            if (o.contains(QStringLiteral("enabled"))) {
                presetEnabled = o.value(QStringLiteral("enabled")).toBool();
            }
            // An entry needs a nickname and at least one of the two plain devices, or per-machine "devices" overrides.
            const bool hasPlainDevices = !o.value(QStringLiteral("videoDevice")).toString().isEmpty()
                    || !o.value(QStringLiteral("audioDevice")).toString().isEmpty();
            const QJsonValue devicesValue = o.value(QStringLiteral("devices"));
            const bool hasPerRoleDevices = devicesValue.isObject() && !devicesValue.toObject().isEmpty();
            if (presetEnabled && o.contains(QStringLiteral("title")) && (hasPlainDevices || hasPerRoleDevices)) {
                m_titles.append(o.value(QStringLiteral("title")).toString());
                m_videoDevices.append(o.value(QStringLiteral("videoDevice")).toString());
                m_audioDevices.append(o.value(QStringLiteral("audioDevice")).toString());
            }
        }
        endResetModel();
        Q_EMIT presetsListChanged();
    }
}

int DirectShowPresetsModel::getNumberOfPresets() {
    return m_titles.size();
}