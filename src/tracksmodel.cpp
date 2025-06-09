/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "tracksmodel.h"
#include "_debug.h"
#include "track.h"
#include <filesystem>
#include <utility>

TracksModel::TracksModel(QObject *parent)
    : QAbstractListModel(parent), m_tracks(nullptr) {
}

int TracksModel::rowCount(const QModelIndex & /*parent*/) const {
    return countTracks();
}

QVariant TracksModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_tracks == nullptr ||  m_tracks->empty())
        return QVariant();

    Track track = m_tracks->at(index.row());

    switch (role) {
    case TextRole: {
        QString text;
        if (!track.title().empty()) {
            text += QString::fromStdString(track.title()) + QStringLiteral(" ");
        }
        if (!track.lang().empty()) {
            text += QString::fromStdString(track.lang()) + QStringLiteral(" ");
        }
        if (!track.codec().empty()) {
            text += QString::fromStdString(track.codec()) + QStringLiteral(" ");
        }
        return QVariant(text);
    }
    case ShortTextRole: {
        QString shortText;
        if (!track.lang().empty()) {
            shortText = QString::fromStdString(track.lang());
        }
        else if (!track.title().empty()) {
            std::filesystem::path titlePath = std::filesystem::path(track.title());
            if (titlePath.has_extension()) {
                shortText = QString::fromStdString(titlePath.stem().string());
            }
            else {
                shortText = QString::fromStdString(track.title());
            }
        }
        return QVariant(shortText);
    }
    case LanguageRole:
        return QVariant(QString::fromStdString(track.lang()));
    case TitleRole:
        return QVariant(QString::fromStdString(track.title()));
    case IDRole:
        return QVariant(track.id());
    case CodecRole:
        return QVariant(QString::fromStdString(track.codec()));
    }

    return QVariant();
}

QHash<int, QByteArray> TracksModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[TextRole] = "text";
    roles[ShortTextRole] = "shortText";
    roles[LanguageRole] = "language";
    roles[TitleRole] = "title";
    roles[IDRole] = "id";
    roles[CodecRole] = "codec";
    return roles;
}

void TracksModel::setTracks(std::vector<Track>* tracks) {
    beginResetModel();
    m_tracks = tracks;
    endResetModel();
}

int TracksModel::countTracks() const {
    if (m_tracks) {
        return static_cast<int>(m_tracks->size());
    }
    else {
        return 0;
    }
}

std::string TracksModel::getListAsFormattedString(std::string removePrefix, int charsPerItem) const {
    if (m_tracks == nullptr)
        return "";

    std::string fullItemList = "";
    for (int i = 0; i < m_tracks->size(); i++) {
        std::string title = "";

        QString shortText;
        if (!m_tracks->at(i).lang().empty()) {
            shortText = QString::fromStdString(m_tracks->at(i).lang());
        }
        else if (!m_tracks->at(i).title().empty()) {
            std::filesystem::path titlePath = std::filesystem::path(m_tracks->at(i).title());
            if (titlePath.has_extension()) {
                shortText = QString::fromStdString(titlePath.stem().string());
            }
            else {
                shortText = QString::fromStdString(m_tracks->at(i).title());
            }
        }
        title += shortText.toStdString();

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
        } else if (countChars >= charsPerItem) {
            title.erase(title.end() - (countChars - charsPerItem + 4), title.end());
            title.insert(title.end(), 3, '.');
            title.insert(title.end(), 1, ' ');
        }
        std::string itemText = title;
        fullItemList += itemText;
        if (i < m_tracks->size() - 1)
            fullItemList += "\n";
    }
    return fullItemList;
}
