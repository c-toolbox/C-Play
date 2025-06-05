/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "spoutmodel.h"
#include "spoutlayer.h"
//#include <QRegularExpression>

SpoutSendersModel::SpoutSendersModel(QObject *parent)
    : QAbstractListModel(parent) {
}

SpoutSendersModel::~SpoutSendersModel() {
}

int SpoutSendersModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_Spoutsenders.size();
}

QVariant SpoutSendersModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_Spoutsenders.empty())
        return QVariant();

    if (!checkIndex(index)) {
        return QVariant();
    }
    if (role == textRole) {
        return m_Spoutsenders.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> SpoutSendersModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[textRole] = "typeName";
    return roles;
}

void SpoutSendersModel::updateSendersList() {
    std::vector<std::string> sendersList = SpoutFinder::instance().getSendersList();

    beginResetModel();
    m_Spoutsenders.clear();
    for (auto s : sendersList) {
        m_Spoutsenders.append(QString::fromStdString(s));
    }
    endResetModel();

    Q_EMIT sendersListChanged();
}

int SpoutSendersModel::getNumberOfSenders() {
    return m_Spoutsenders.size();
}

QString SpoutSendersModel::getSpoutVersionString() {
    QString spoutVersion = QString::fromStdString(SpoutFinder::instance().getSpoutVersionString());
    //QRegularExpression regExp(QStringLiteral("\\d*\\.\\d*\\.\\d*"));
    //QRegularExpressionMatch match = regExp.match(spoutVersion);
    //if (match.hasMatch()) {
        //return match.captured(0);
    //}
    return spoutVersion;
}