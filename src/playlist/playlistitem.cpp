/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "_debug.h"
#include "playlistitem.h"

#include <QFileInfo>
#include <QUrl>

QString PlayListItemData::mediaTitle() const
{
    return m_mediaTitle;
}

QString PlayListItemData::filePath() const
{
    return m_filePath;
}

QString PlayListItemData::fileName() const
{
    return m_fileName;
}

QString PlayListItemData::folderPath() const
{
    return m_folderPath;
}

QString PlayListItemData::duration() const
{
    return m_duration;
}

QString PlayListItemData::separateOverlayFile() const
{
    return m_separateOverlayFile;
}

QString PlayListItemData::separateAudioFile() const
{
    return m_separateAudioFile;
}

double PlayListItemData::startTime() const
{
    return m_startTime;
}

double PlayListItemData::endTime() const
{
    return m_endTime;
}

int PlayListItemData::loopMode() const
{
    return m_loopMode;
}

int PlayListItemData::transitionMode() const
{
    return m_transitionMode;
}

int PlayListItemData::gridToMapOn() const
{
    return m_gridToMapOn;
}

int PlayListItemData::stereoVideo() const
{
    return m_stereoVideo;
}

bool PlayListItemData::isPlaying() const
{
    return m_isPlaying;
}

int PlayListItemData::index() const
{
    return m_index;
}

PlayListItem::PlayListItem(const QString &path, int i, QObject *parent)
    : QObject(parent)
{
    QUrl url(path);

    if (url.scheme().startsWith("http")) {
        setFilePath(url.toString());
        setFileName(QStringLiteral(""));
        setFolderPath(QStringLiteral(""));
    } else {
        QFileInfo fileInfo(path);
        setFileName(fileInfo.fileName());
        setFilePath(fileInfo.absoluteFilePath());
        setFolderPath(fileInfo.absolutePath());
    }
    setIndex(i);
    setIsPlaying(false);
    setSeparateOverlayFile(QStringLiteral(""));
    setSeparateAudioFile(QStringLiteral(""));
}

PlayListItem::PlayListItem(const PlayListItem& p1) {
    setData(p1.data());
}

PlayListItem::PlayListItem(const PlayListItemData& d) {
    setData(d);
}

QString PlayListItem::mediaTitle() const
{
    return m_data.m_mediaTitle;
}

void PlayListItem::setMediaTitle(const QString &title)
{
    m_data.m_mediaTitle = title;
}

QString PlayListItem::filePath() const
{
    return m_data.m_filePath;
}

void PlayListItem::setFilePath(const QString &filePath)
{
    m_data.m_filePath = filePath;
}

QString PlayListItem::fileName() const
{
    return m_data.m_fileName;
}

void PlayListItem::setFileName(const QString &fileName)
{
    m_data.m_fileName = fileName;
}

QString PlayListItem::folderPath() const
{
    return m_data.m_folderPath;
}

void PlayListItem::setFolderPath(const QString &folderPath)
{
    m_data.m_folderPath = folderPath;
}

QString PlayListItem::duration() const
{
    return m_data.m_duration;
}

void PlayListItem::setDuration(const QString &duration)
{
    m_data.m_duration = duration;
}

QString PlayListItem::separateOverlayFile() const
{
    return m_data.m_separateOverlayFile;
}

void PlayListItem::setSeparateOverlayFile(const QString& overlayFile)
{
    m_data.m_separateOverlayFile = overlayFile;
}

QString PlayListItem::separateAudioFile() const
{
    return m_data.m_separateAudioFile;
}

void PlayListItem::setSeparateAudioFile(const QString& audioFile) 
{
    m_data.m_separateAudioFile = audioFile;
}

double PlayListItem::startTime() const
{
    return m_data.m_startTime;
}

void PlayListItem::setStartTime(double startTime)
{
    m_data.m_startTime = startTime;
}

double PlayListItem::endTime() const
{
    return m_data.m_endTime;
}

void PlayListItem::setEndTime(double endTime)
{
    m_data.m_endTime = endTime;
}

int PlayListItem::loopMode() const
{
    return m_data.m_loopMode;
}

void PlayListItem::setLoopMode(int loopMode)
{
    m_data.m_loopMode = loopMode;
}

int PlayListItem::transitionMode() const
{
    return m_data.m_transitionMode;
}

void PlayListItem::setTransitionMode(int transitionMode)
{
    m_data.m_transitionMode = transitionMode;
}

int PlayListItem::gridToMapOn() const
{
    return m_data.m_gridToMapOn;
}

void PlayListItem::setGridToMapOn(int gridToMapOn)
{
    m_data.m_gridToMapOn = gridToMapOn;
}

int PlayListItem::stereoVideo() const
{
    return m_data.m_stereoVideo;
}

void PlayListItem::setStereoVideo(int stereoVideo)
{
    m_data.m_stereoVideo = stereoVideo;
}

bool PlayListItem::isPlaying() const
{
    return m_data.m_isPlaying;
}

void PlayListItem::setIsPlaying(bool isPlaying)
{
    m_data.m_isPlaying = isPlaying;
}

int PlayListItem::index() const
{
    return m_data.m_index;
}

void PlayListItem::setIndex(int index)
{
    m_data.m_index = index;
}

PlayListItemData PlayListItem::data() const {
    return m_data;
}

void PlayListItem::setData(PlayListItemData d) {
    m_data = d;
}
