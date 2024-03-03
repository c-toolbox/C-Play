/*
 * SPDX-FileCopyrightText: 
 * 2021-2023 Erik Sundén <eriksunden85@gmail.com> 
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef TRACKSMODEL_H
#define TRACKSMODEL_H

#include <QAbstractListModel>
#include <QObject>

class Track;

class TracksModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit TracksModel(QObject *parent = nullptr);
    enum {
        TextRole = Qt::UserRole,
        LanguageRole,
        TitleRole,
        ShortTitleRole,
        IDRole,
        CodecRole,
    };
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;
    int countTracks() const;
    std::string getListAsFormattedString(std::string removePrefix = "", int charsPerItem = 40) const;

public slots:
    void setTracks(QMap<int, Track *> tracks);
private:
    QMap<int, Track *> m_tracks;
};

#endif // TRACKSMODEL_H
