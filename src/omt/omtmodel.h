/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef OMTMODEL_H
#define OMTMODEL_H

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

class OMTSendersModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit OMTSendersModel(QObject *parent = nullptr);
    ~OMTSendersModel();

    enum {
        textRole = Qt::UserRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void updateSendersList();

    Q_PROPERTY(int numberOfSenders READ getNumberOfSenders NOTIFY sendersListChanged)
    int getNumberOfSenders();

    Q_INVOKABLE QString getOMTVersionString();

Q_SIGNALS:
    void sendersListChanged();

private:
    QStringList m_OMTsenders;
};

#endif // OMTMODEL_H
