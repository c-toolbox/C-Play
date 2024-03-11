/*
 * SPDX-FileCopyrightText: 
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com> 
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "_debug.h"
#include "track.h"
#include "tracksmodel.h"
#include <utility>

TracksModel::TracksModel(QObject *parent)
    : QAbstractListModel(parent)
{

}

int TracksModel::rowCount(const QModelIndex &/*parent*/) const
{
    return m_tracks.size();
}

QVariant TracksModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || m_tracks.isEmpty())
        return QVariant();

    Track *track = m_tracks[index.row()];

    switch (role) {
    case TextRole:
        return QVariant(track->text());
    case ShortTextRole:
        return QVariant(track->shortText());
    case LanguageRole:
        return QVariant(track->lang());
    case TitleRole:
        return QVariant(track->title());
    case IDRole:
        return QVariant(track->id());
    case CodecRole:
        return QVariant(track->codec());
    }

    return QVariant();
}

QHash<int, QByteArray> TracksModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TextRole] = "text";
    roles[ShortTextRole] = "shortText";
    roles[LanguageRole] = "language";
    roles[TitleRole] = "title";
    roles[IDRole] = "id";
    roles[CodecRole] = "codec";
    return roles;
}

void TracksModel::setTracks(QMap<int, Track *> tracks)
{
    beginResetModel();
    m_tracks = std::move(tracks);
    endResetModel();
}

int TracksModel::countTracks() const {
    return m_tracks.size();
}

std::string TracksModel::getListAsFormattedString(std::string removePrefix, int charsPerItem) const
{
    std::string fullItemList = "";
    for (int i = 0; i < m_tracks.size(); i++)
    {
        std::string title = "";
        title += m_tracks[i]->shortText().toStdString();

        if (!removePrefix.empty()) {
            int mc = 0;
            for (int c = 0; c < title.length(); c++) {
                if (removePrefix[c] == title[c])
                    mc++;
                else
                    break;
            }
            if (mc > 0) {
                title.erase(0, mc);
            }
        }
        std::replace(title.begin(), title.end(), '_', ' ');

        size_t countChars = title.size();
        if (countChars < charsPerItem) {
            title.insert(title.end(), charsPerItem - countChars, ' ');
        }
        else if (countChars >= charsPerItem) {
            title.erase(title.end() - (countChars - charsPerItem + 4), title.end());
            title.insert(title.end(), 3, '.');
            title.insert(title.end(), 1, ' ');
        }
        std::string itemText = title;
        fullItemList += itemText;
        if (i < m_tracks.size())
            fullItemList += "\n";
    }
    return fullItemList;
}
