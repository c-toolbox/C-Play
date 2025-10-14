/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SCREENSMODEL_H
#define SCREENSMODEL_H

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

class ScreensModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit ScreensModel(QObject *parent = nullptr);
    ~ScreensModel();

    enum {
        nameRole = Qt::UserRole,
        geometryRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE QRect geometry(int idx) const;
    Q_INVOKABLE void updateScreensList();

    Q_PROPERTY(int numberOfScreens READ getNumberOfScreens NOTIFY screensListChanged)
    int getNumberOfScreens();

Q_SIGNALS:
    void screensListChanged();

private:
    QStringList m_screenNames;
    QList<QRect> m_screenGeometry;
};

#endif // SCREENSMODEL_H
