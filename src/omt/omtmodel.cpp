/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "omtmodel.h"
#include "omtlayer.h"

OMTSendersModel::OMTSendersModel(QObject *parent)
    : QAbstractListModel(parent) {
}

OMTSendersModel::~OMTSendersModel() {
}

int OMTSendersModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_OMTsenders.size();
}

QVariant OMTSendersModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_OMTsenders.empty())
        return QVariant();

    if (!checkIndex(index)) {
        return QVariant();
    }
    if (role == textRole) {
        return m_OMTsenders.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> OMTSendersModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[textRole] = "typeName";
    return roles;
}

void OMTSendersModel::updateSendersList() {
    std::vector<std::string> sendersList = OmtFinder::instance().getSendersList();

    beginResetModel();
    m_OMTsenders.clear();
    for (const auto& s : sendersList) {
        m_OMTsenders.append(QString::fromStdString(s));
    }
    endResetModel();

    Q_EMIT sendersListChanged();
}

int OMTSendersModel::getNumberOfSenders() {
    return m_OMTsenders.size();
}

QString OMTSendersModel::getOMTVersionString() {
    return QString::fromStdString(OmtFinder::instance().getOMTVersionString());
}
