/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "playlistmodel.h"
#include "playlistitem.h"
#include "_debug.h"
#include "application.h"
#include "worker.h"

#include <QCollator>
#include <QDirIterator>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QUrl>

PlayListModel::PlayListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    m_config = KSharedConfig::openConfig("georgefb/haruna.conf");
    connect(this, &PlayListModel::videoAdded,
            Worker::instance(), &Worker::getMetaData);

    connect(Worker::instance(), &Worker::metaDataReady, this, [ = ](int i, KFileMetaData::PropertyMap metaData) {
        auto duration = metaData[KFileMetaData::Property::Duration].toInt();
        auto title = metaData[KFileMetaData::Property::Title].toString();

        m_playList[i]->setDuration(Application::formatTime(duration));
        m_playList[i]->setMediaTitle(title);

        emit dataChanged(index(i, 0), index(i, 0));

    });
}

int PlayListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_playList.size();
}

QVariant PlayListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || m_playList.empty())
        return QVariant();

    auto playListItem = m_playList.at(index.row());

    if (!playListItem) {
        qWarning() << "PlayListItem pointer was null";
        return QVariant();
    }

    switch (role) {
    case NameRole:
        return QVariant(playListItem->fileName());
    case TitleRole:
        return playListItem->mediaTitle().isEmpty()
                ? QVariant(playListItem->fileName())
                : QVariant(playListItem->mediaTitle());
    case PathRole:
        return QVariant(playListItem->filePath());
    case DurationRole:
        return QVariant(playListItem->duration());
    case PlayingRole:
        return QVariant(playListItem->isPlaying());
    case FolderPathRole:
        return QVariant(playListItem->folderPath());
    }

    return QVariant();
}

QHash<int, QByteArray> PlayListModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[TitleRole] = "title";
    roles[PathRole] = "path";
    roles[FolderPathRole] = "folderPath";
    roles[DurationRole] = "duration";
    roles[PlayingRole] = "isPlaying";
    return roles;
}

void PlayListModel::getVideos(QString path)
{
    clear();
    path = QUrl(path).toLocalFile().isEmpty() ? path : QUrl(path).toLocalFile();
    QFileInfo pathInfo(path);
    QStringList videoFiles;
    if (pathInfo.exists() && pathInfo.isFile()) {
        QDirIterator it(pathInfo.absolutePath(), QDir::Files, QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            QString file = it.next();
            QFileInfo fileInfo(file);
            QMimeDatabase db;
            QMimeType type = db.mimeTypeForFile(file);
            if (fileInfo.exists() && type.name().startsWith("video/")) {
                videoFiles.append(fileInfo.absoluteFilePath());
            }
        }
    }
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(videoFiles.begin(), videoFiles.end(), collator);

    beginInsertRows(QModelIndex(), 0, videoFiles.count() - 1);

    for (int i = 0; i < videoFiles.count(); ++i) {
        auto video = new PlayListItem(videoFiles.at(i), i, this);
        m_playList.append(QPointer<PlayListItem>(video));
        if (path == videoFiles.at(i)) {
            setPlayingVideo(i);
        }
        emit videoAdded(i, video->filePath());
    }

    endInsertRows();
}

Playlist PlayListModel::items() const
{
    return m_playList;
}

QString PlayListModel::configFolder()
{

    auto configPath = QStandardPaths::writableLocation(m_config->locationType());
    auto configFilePath = configPath.append(QStringLiteral("/")).append(m_config->name());
    QFileInfo fileInfo(configFilePath);

    return fileInfo.absolutePath();
}

Playlist PlayListModel::getPlayList() const
{
    return m_playList;
}

void PlayListModel::setPlayList(const Playlist &playList)
{
    beginInsertRows(QModelIndex(), 0, playList.size() - 1);
    m_playList = playList;
    endInsertRows();
}

int PlayListModel::getPlayingVideo() const
{
    return m_playingVideo;
}

void PlayListModel::clear()
{
    m_playingVideo = 0;
    qDeleteAll(m_playList);
    beginResetModel();
    m_playList.clear();
    endResetModel();
}

QString PlayListModel::getPath(int i)
{
    // when restoring a youtube playlist
    // ensure the requested path is valid
    if (m_playList.size() <= i) {
        return m_playList[0]->filePath();
    }
    return m_playList[i]->filePath();
}

QPointer<PlayListItem> PlayListModel::getItem(int i)
{
    if (m_playList.size() <= i) {
        return m_playList[0];
    }
    return m_playList[i];
}

void PlayListModel::setPlayingVideo(int playingVideo)
{
    // unset current playing video
    m_playList[m_playingVideo]->setIsPlaying(false);
    emit dataChanged(index(m_playingVideo, 0), index(m_playingVideo, 0));

    // set new playing video
    m_playList[playingVideo]->setIsPlaying(true);
    emit dataChanged(index(playingVideo, 0), index(playingVideo, 0));

    m_playingVideo = playingVideo;
    emit playingVideoChanged();
}

QString PlayListModel::mediaTitle(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->mediaTitle();
    else
        return "";
}

QString PlayListModel::filePath(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->filePath();
    else
        return "";
}

QString PlayListModel::fileName(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->fileName();
    else
        return "";
}

QString PlayListModel::folderPath(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->folderPath();
    else
        return "";
}

QString PlayListModel::duration(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->duration();
    else
        return "";
}

QString PlayListModel::separateAudioFile(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->separateAudioFile();
    else
        return "";
}

double PlayListModel::startTime(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->startTime();
    else
        return 0.0;
}

double PlayListModel::endTime(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->endTime();
    else
        return 0.0;
}

int PlayListModel::loopMode(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->loopMode();
    else
        return 0;
}

int PlayListModel::transitionMode(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->transitionMode();
    else
        return 0;
}

int PlayListModel::gridToMapOn(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->gridToMapOn();
    else
        return 0;
}

int PlayListModel::stereoVideo(int i) const
{
    if (i > 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->stereoVideo();
    else
        return 0;
}
