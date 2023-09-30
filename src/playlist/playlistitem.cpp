/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "_debug.h"
#include "playlistitem.h"
#include "generalsettings.h"

#include <QFileInfo>
#include <QDir>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

QString PlayListItemData::filePath() const
{
    return m_filePath;
}

QString PlayListItemData::fileName() const
{
    return m_fileName;
}

QString PlayListItemData::fileFolderPath() const
{
    return m_fileFolderPath;
}

QString PlayListItemData::mediaFile() const
{
    return m_mediaFile;
}

QString PlayListItemData::mediaTitle() const
{
    return m_mediaTitle;
}

double PlayListItemData::duration() const
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

QList<PlayListItemData::Section> PlayListItemData::sections() const
{
    return m_sections;
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
    updateToNewFile(path);
    setIndex(i);
}

PlayListItem::PlayListItem(const PlayListItem& p1) {
    setData(p1.data());
}

PlayListItem::PlayListItem(const PlayListItemData& d) {
    setData(d);
}

PlayListItem& PlayListItem::operator =(const PlayListItem& p1) {
    PlayListItem* p2 = new PlayListItem(p1);
    return *p2;
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

QUrl PlayListItem::fileFolderPath() const
{
    return QUrl("file:///" + m_data.m_fileFolderPath);
}

void PlayListItem::setFileFolderPath(const QString &folderPath)
{
    m_data.m_fileFolderPath = folderPath;
}

QString PlayListItem::mediaFile() const
{
    return m_data.m_mediaFile;
}

void PlayListItem::setMediaFile(const QString& file)
{
    m_data.m_mediaFile = file;
}

QString PlayListItem::mediaTitle() const
{
    return m_data.m_mediaTitle;
}

void PlayListItem::setMediaTitle(const QString& title)
{
    m_data.m_mediaTitle = title;
}

QString PlayListItem::durationFormatted() const 
{
    QTime t(0, 0, 0);
    QString formattedTime = t.addSecs(static_cast<qint64>(m_data.m_duration)).toString("hh:mm:ss");
    return formattedTime;
}

double PlayListItem::duration() const
{
    return m_data.m_duration;
}

void PlayListItem::setDuration(double duration)
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

void PlayListItem::addSection(QString name, double startTime, double endTime, int eosMode) 
{
    m_data.m_sections.append(PlayListItemData::Section(name, startTime, endTime, eosMode));
}

void PlayListItem::addSection(QString name, QString startTime, QString endTime, int eosMode)
{
    QTime start = QTime::fromString(startTime, "hh:mm:ss");
    QTime end = QTime::fromString(endTime, "hh:mm:ss");
    QTime zero = QTime(0, 0);
    addSection(name, double(zero.msecsTo(start)) / 1000.0, double(zero.msecsTo(end)) / 1000.0, eosMode);
}

QList<PlayListItemData::Section> PlayListItem::sections()
{
    return m_data.m_sections;
}

bool PlayListItem::isSectionPlaying(int index) const {
    return m_data.m_sections[index].isPlaying;
}

void PlayListItem::setIsSectionPlaying(int index, bool isPlaying) {
    m_data.m_sections[index].isPlaying = isPlaying;
}

int PlayListItem::index() const
{
    return m_data.m_index;
}

void PlayListItem::setIndex(int index)
{
    m_data.m_index = index;
}

QString PlayListItem::makePathRelativeTo(const QString& filePath, const QStringList& pathsToConsider) const {
    // Assuming filePath is absolute
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            return foundDir.relativeFilePath(filePath);
        }
    }
    return filePath;
}

void PlayListItem::saveAsJSONPlayFile(const QString& path) const {
    QJsonDocument doc;
    QJsonObject obj = doc.object();

    QString fileToSave = path;
    fileToSave.replace("file:///", "");
    QFileInfo fileToSaveInfo(fileToSave);

    QStringList pathsToConsider;
    pathsToConsider.append(GeneralSettings::cPlayMediaLocation());
    pathsToConsider.append(fileToSaveInfo.absoluteDir().absolutePath());

    obj.insert("video", makePathRelativeTo(mediaFile(), pathsToConsider));

    if (!separateOverlayFile().isEmpty())
        obj.insert("overlay", makePathRelativeTo(separateOverlayFile(), pathsToConsider));

    if (!separateAudioFile().isEmpty())
        obj.insert("audio", makePathRelativeTo(separateAudioFile(), pathsToConsider));

    if (!mediaTitle().isEmpty())
        obj.insert("title", mediaTitle());

    obj.insert("duration", duration());

    QString grid;
    int gridIdx = gridToMapOn();
    if (gridIdx == 1) {
        grid = "plane";
    }
    else if (gridIdx == 2) {
        grid = "dome";
    }
    else if (gridIdx == 3) {
        grid = "sphere-eqr";
    }
    else if (gridIdx == 4) {
        grid = "sphere-eac";
    }
    else { // 0
        grid = "pre-split";
    }
    obj.insert("grid", grid);

    QString sv;
    int stereoVideoIdx = stereoVideo();
    if (stereoVideoIdx == 1) {
        sv = "side-by-side";
    }
    else if (stereoVideoIdx == 2) {
        sv = "top-bottom";
    }
    else if (stereoVideoIdx == 3) {
        sv = "top-bottom-flip";
    }
    else { // 0
        sv = "no";
    }
    obj.insert("stereoscopic", sv);

    QJsonArray sectionsArray;
    for (int i = 0; i < m_data.m_sections.size(); i++)
    {
        QJsonObject item_data;

        int eosMode = m_data.m_sections[i].eosMode;
        QString eosModeText;
        switch (eosMode)
        {
        case 1:
            eosModeText = "fade_out";
            break;
        case 2:
            eosModeText = "continue";
            break;
        case 3:
            eosModeText = "next";
            break;
        case 4:
            eosModeText = "loop";
            break;
        default:
            eosModeText = "pause";
            break;
        }

        item_data.insert("section_title", QJsonValue(m_data.m_sections[i].title));
        item_data.insert("time_begin", QJsonValue(m_data.m_sections[i].startTime));
        item_data.insert("time_end", QJsonValue(m_data.m_sections[i].endTime));
        item_data.insert("transition_at_end", QJsonValue(eosModeText));

        sectionsArray.push_back(QJsonValue(item_data));
    }
    if(!sectionsArray.isEmpty())
        obj.insert(QString("video_sections"), QJsonValue(sectionsArray));

    doc.setObject(obj);

    QFile jsonFile(fileToSave);
    jsonFile.open(QFile::WriteOnly);
    jsonFile.write(doc.toJson());
    jsonFile.close();
}

PlayListItemData PlayListItem::data() const {
    return m_data;
}

void PlayListItem::setData(PlayListItemData d) {
    m_data = d;
}

void PlayListItem::updateToNewFile(const QString& path) {
    QUrl url(path);

    if (url.scheme().startsWith("http")) {
        setFilePath(url.toString());
        setFileName(QStringLiteral(""));
        setFileFolderPath(QStringLiteral(""));
        setMediaFile(url.toString());
    }
    else {
        QFileInfo fileInfo(path);
        setFileName(fileInfo.fileName());
        setFilePath(fileInfo.absoluteFilePath());
        setFileFolderPath(fileInfo.absolutePath());
        setMediaFile(fileInfo.absoluteFilePath());
    }
    setIsPlaying(false);
    setSeparateOverlayFile(QStringLiteral(""));
    setSeparateAudioFile(QStringLiteral(""));
}
