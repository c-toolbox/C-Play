/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SPOUTMODEL_H
#define SPOUTMODEL_H

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

class SpoutSendersModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SpoutSendersModel(QObject *parent = nullptr);
    ~SpoutSendersModel();

    enum {
        textRole = Qt::UserRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void updateSendersList();

    Q_PROPERTY(int numberOfSenders READ getNumberOfSenders NOTIFY sendersListChanged)
    int getNumberOfSenders();

    Q_INVOKABLE QString getSpoutVersionString();

Q_SIGNALS:
    void sendersListChanged();

private:
    QStringList m_Spoutsenders;
};

#endif // SPOUTMODEL_H
