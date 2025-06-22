/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef STREAMMODEL_H
#define STREAMMODEL_H

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

class StreamModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(StreamModel)

public:
    explicit StreamModel(QObject *parent = nullptr);
    ~StreamModel();

    enum {
        pathRole = Qt::UserRole,
        titleRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void updateStreamsList();

    Q_PROPERTY(int numberOfStreams READ getNumberOfStreams NOTIFY streamsListChanged)
    int getNumberOfStreams();

Q_SIGNALS:
    void streamsListChanged();

private:
    QStringList m_streamTitles;
    QStringList m_streamPaths;
};

#endif // STREAMMODEL_H
