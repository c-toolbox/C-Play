/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mpvoptionsmodel.h"
#include "application.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>

namespace {
QString mpvConfRootPath() {
    const QString confAll = QString::fromStdString(SyncHelper::instance().configuration.confAll);
    if (!confAll.isEmpty()) {
        QDir dir = QFileInfo(confAll).absoluteDir();
        if (dir.cdUp() && dir.exists())
            return dir.absolutePath();
    }

    const QString nextToApp = QCoreApplication::applicationDirPath() + QStringLiteral("/data/mpv-conf");
    if (QDir(nextToApp).exists())
        return nextToApp;

    return QStringLiteral("./data/mpv-conf");
}
} // namespace

MpvOptionsModel::MpvOptionsModel(QObject *parent)
    : QAbstractListModel(parent) {
    m_optionTitles.append(QStringLiteral("Use global settings"));
    m_optionNames.append(QStringLiteral(""));
}

MpvOptionsModel::~MpvOptionsModel() {
}

int MpvOptionsModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_optionNames.size();
}

QVariant MpvOptionsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_optionNames.size())
        return QVariant();

    switch (role) {
    case nameRole:
        return QVariant(m_optionNames.at(index.row()));
    case titleRole:
    case Qt::DisplayRole:
        return QVariant(m_optionTitles.at(index.row()));
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MpvOptionsModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[nameRole] = "name";
    roles[titleRole] = "title";
    return roles;
}

void MpvOptionsModel::updateOptionsList(const QString &suffix) {
    beginResetModel();

    m_optionTitles.clear();
    m_optionNames.clear();

    m_optionTitles.append(QStringLiteral("Use global settings"));
    m_optionNames.append(QStringLiteral(""));

    if (!suffix.isEmpty()) {
        const QString fileEnding = suffix + QStringLiteral(".json");
        const QDir rootDir(mpvConfRootPath());
        const QFileInfoList files = rootDir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &fi : files) {
            const QString fileName = fi.fileName();
            if (!fileName.endsWith(fileEnding, Qt::CaseInsensitive))
                continue;

            const QString name = fileName.left(fileName.size() - fileEnding.size());
            if (name.isEmpty() || m_optionNames.contains(name))
                continue;

            m_optionNames.append(name);
            m_optionTitles.append(name.at(0).toUpper() + name.mid(1));
        }
    }

    endResetModel();

    Q_EMIT optionsListChanged();
}

int MpvOptionsModel::indexOfOption(const QString &name) const {
    const int idx = m_optionNames.indexOf(name);
    return idx < 0 ? 0 : idx;
}

QString MpvOptionsModel::optionNameAt(int index) const {
    if (index < 0 || index >= m_optionNames.size())
        return QStringLiteral("");

    return m_optionNames.at(index);
}

int MpvOptionsModel::getNumberOfOptions() {
    return m_optionNames.size();
}
