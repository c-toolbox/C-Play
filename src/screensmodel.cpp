/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "screensmodel.h"

#include <QSize>
#include <QPoint>
#include <QScreen>
#include <QGuiApplication>

ScreensModel::ScreensModel(QObject *parent)
    : QAbstractListModel(parent) {
}

ScreensModel::~ScreensModel() {
}

int ScreensModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_screenNames.size();
}

QVariant ScreensModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_screenNames.empty())
        return QVariant();

    if (!checkIndex(index)) {
        return QVariant();
    }
    if (role == nameRole) {
        return m_screenNames.at(index.row());
    }
    if (role == geometryRole) {
        return m_screenGeometry.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> ScreensModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[nameRole] = "name";
    roles[geometryRole] = "geometry";
    return roles;
}

QRect ScreensModel::geometry(int idx) const {
    if (idx >= m_screenGeometry.size())
        return QRect(0, 0, -1, -1);

    return m_screenGeometry[idx];
}

void ScreensModel::updateScreensList() {
    beginResetModel();
    m_screenNames.clear();
    m_screenGeometry.clear();

    QList<QScreen*> screens = QGuiApplication::screens();

    for (auto screen : screens) {
        if (screen != QGuiApplication::primaryScreen()) {
            m_screenNames.push_back(screen->name());
            m_screenGeometry.push_back(screen->geometry());
        }
    }

    endResetModel();
    Q_EMIT screensListChanged();
}

int ScreensModel::getNumberOfScreens() {
    return m_screenNames.size();
}