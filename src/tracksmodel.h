/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRACKSMODEL_H
#define TRACKSMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <vector>
#include <QtQml/qqmlregistration.h>

class Track;

class TracksModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(TracksModel)

public:
    explicit TracksModel(QObject *parent = nullptr);
    enum {
        TextRole = Qt::UserRole,
        ShortTextRole,
        LanguageRole,
        TitleRole,
        IDRole,
        CodecRole,
    };
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;
    std::string getListAsFormattedString(std::string removePrefix = "", int charsPerItem = 40) const;

    Q_INVOKABLE void setTracks(std::vector<Track>* tracks);
    Q_INVOKABLE int countTracks() const;

private:
    std::vector<Track>* m_tracks;
};

#endif // TRACKSMODEL_H
