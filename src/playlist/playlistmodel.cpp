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
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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

    m_playListName = "";
    m_playListPath = "";
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
        return QVariant(playListItem->mediaFile());
    case DurationRole:
        return QVariant(playListItem->duration());
    case PlayingRole:
        return QVariant(playListItem->isPlaying());
    case FolderPathRole:
        return QVariant(playListItem->fileFolderPath());
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

void PlayListModel::setPlayListName(QString name)
{
    m_playListName = name;
}

QString PlayListModel::getPlayListName() const
{
    return m_playListName;
}

void PlayListModel::setPlayListPath(QString path)
{
    m_playListPath = path;
}

QString PlayListModel::getPlayListPath() const
{
    return m_playListPath;
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
        return m_playList[0]->mediaFile();
    }
    return m_playList[i]->mediaFile();
}

void PlayListModel::addItem(PlayListItem* item)
{
    beginInsertRows(QModelIndex(), m_playList.size(), m_playList.size());
    m_playList.append(QPointer<PlayListItem>(item));
    endInsertRows();
}

QPointer<PlayListItem> PlayListModel::getItem(int i)
{
    if (m_playList.size() <= i) {
        return m_playList[0];
    }
    return m_playList[i];
}

void PlayListModel::removeItem(int i) {
    beginRemoveRows(QModelIndex(), i, i);
    m_playList.removeAt(i);
    if (m_playingVideo == i)
        m_playingVideo = 0;
    else if (m_playingVideo > i)
        m_playingVideo -= 1;
    endRemoveRows();
}

void PlayListModel::moveItemUp(int i) {
    if (i == 0) return;
    beginMoveRows(QModelIndex(), i, i, QModelIndex(), i-1);
    m_playList.move(i, i-1);
    if (m_playingVideo == i)
        m_playingVideo -= 1;
    else if (m_playingVideo == i-1)
        m_playingVideo = i;
    endMoveRows();
}

void PlayListModel::moveItemDown(int i) {
    if (i == (m_playList.size()-1)) return;
    beginMoveRows(QModelIndex(), i+1, i+1, QModelIndex(), i);
    m_playList.move(i, i+1);
    if (m_playingVideo == i)
        m_playingVideo += 1;
    else if (m_playingVideo == i-1)
        m_playingVideo = i;
    endMoveRows();
}

void PlayListModel::updateItem(int i) {
    emit dataChanged(index(i, 0), index(i, 0));
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

QString PlayListModel::filePath(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->filePath();
    else
        return "";
}

QString PlayListModel::fileName(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->fileName();
    else
        return "";
}

QUrl PlayListModel::fileFolderPath(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->fileFolderPath();
    else
        return "";
}

QString PlayListModel::mediaFile(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->mediaFile();
    else
        return "";
}

QString PlayListModel::mediaTitle(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->mediaTitle();
    else
        return "";
}

QString PlayListModel::duration(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->duration();
    else
        return "";
}

QString PlayListModel::separateAudioFile(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->separateAudioFile();
    else
        return "";
}

double PlayListModel::startTime(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->startTime();
    else
        return 0.0;
}

double PlayListModel::endTime(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->endTime();
    else
        return 0.0;
}

int PlayListModel::loopMode(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->loopMode();
    else
        return m_defaultLoopMode;
}

void PlayListModel::setLoopMode(int i, int loopMode) {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        m_playList[i]->setLoopMode(loopMode);

    emit dataChanged(index(m_playingVideo, 0), index(m_playingVideo, 0));
}

int PlayListModel::transitionMode(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->transitionMode();
    else
        return 0;
}

int PlayListModel::gridToMapOn(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->gridToMapOn();
    else
        return 0;
}

int PlayListModel::stereoVideo(int i) const
{
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->stereoVideo();
    else
        return 0;
}

void PlayListModel::saveAsJSONPlaylist(const QString& path) {
    QJsonDocument doc;
    QJsonObject obj = doc.object();

    QString fileToSave = path;
    fileToSave.replace("file:///", "");

    QJsonArray playlistArray;
    for (int i=0; i < m_playList.size(); i++)
    {
        QJsonObject item_data;

        int loopMode = m_playList[i]->loopMode();
        QString loopModeText;
        switch (loopMode)
        {
        case 1:
            loopModeText = "pause";
            break;
        case 2:
            loopModeText = "loop";
            break;
        default:
            loopModeText = "continue";
            break;
        }

        item_data.insert("file", QJsonValue(m_playList[i]->filePath()));
        item_data.insert("on_file_end", QJsonValue(loopModeText));

        playlistArray.push_back(QJsonValue(item_data));
    }

    obj.insert(QString("playlist"), QJsonValue(playlistArray));
    doc.setObject(obj);

    QFile jsonFile(fileToSave);
    jsonFile.open(QFile::WriteOnly);
    jsonFile.write(doc.toJson());
    jsonFile.close();

    QFileInfo fileInfo(jsonFile);
    setPlayListName(fileInfo.baseName());
}
