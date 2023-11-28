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

void PlayListItem::addSection(QString title, double startTime, double endTime, int eosMode)
{
    m_data.m_sections.append(PlayListItemData::Section(title, startTime, endTime, eosMode));
}

void PlayListItem::addSection(QString title, QString startTime, QString endTime, int eosMode)
{
    QTime start = QTime::fromString(startTime, "hh:mm:ss");
    QTime end = QTime::fromString(endTime, "hh:mm:ss");
    QTime zero = QTime(0, 0);
    addSection(title, double(zero.msecsTo(start)) / 1000.0, double(zero.msecsTo(end)) / 1000.0, eosMode);
}

void PlayListItem::removeSection(int i) 
{
    if (i < 0 || i >= m_data.m_sections.size())
        return;

    m_data.m_sections.removeAt(i);
}

void PlayListItem::moveSection(int from, int to)
{
    if (from < 0 || from >= m_data.m_sections.size() ||
        to < 0 || to >= m_data.m_sections.size())
        return;

    m_data.m_sections.move(from, to);
}

QString PlayListItem::sectionTitle(int i) const
{
    if (i < 0 || i >= m_data.m_sections.size())
        return "";

    return m_data.m_sections.at(i).title;
}

double PlayListItem::sectionStartTime(int i) const
{
    if (i < 0 || i >= m_data.m_sections.size())
        return 0;

    return m_data.m_sections.at(i).startTime;
}

double PlayListItem::sectionEndTime(int i) const
{
    if (i < 0 || i >= m_data.m_sections.size())
        return 0;

    return m_data.m_sections.at(i).endTime;
}

int PlayListItem::sectionEOSMode(int i) const
{
    if (i < 0 || i >= m_data.m_sections.size())
        return 0;

    return m_data.m_sections.at(i).eosMode;
}

int PlayListItem::numberOfSections() const
{
    return m_data.m_sections.size();
}

const PlayListItemData::Section& PlayListItem::getSection(int i) const
{
    return m_data.m_sections.at(i);
}

QList<PlayListItemData::Section> PlayListItem::sections()
{
    return m_data.m_sections;
}

bool PlayListItem::isSectionPlaying(int index) const {
    if (index < 0 || index >= m_data.m_sections.size())
        return false;

    return m_data.m_sections[index].isPlaying;
}

void PlayListItem::setIsSectionPlaying(int index, bool isPlaying) {
    if (index < 0 || index >= m_data.m_sections.size())
        return;

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
    setSeparateOverlayFile(QStringLiteral(""));
    setSeparateAudioFile(QStringLiteral(""));
}

void PlayListItem::loadDetailsFromDisk() {
    QFileInfo checkedFilePathInfo(filePath());
    QString fileExt = checkedFilePathInfo.suffix();

    if (fileExt == "cplayfile" || fileExt == "cplay_file") {
        loadJSONPlayfile();
    }
    else if (fileExt == "fdv") {
        loadUniviewFDV();
    }
}

void PlayListItem::loadJSONPlayfile() {
    QFileInfo jsonFileInfo(filePath());
    if (!jsonFileInfo.exists())
    {
        qDebug() << "C-play file " << filePath() << " did not exist.";
        return;
    }

    QFile f(filePath());
    f.open(QIODevice::ReadOnly);
    QString fileContent = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileContent.toUtf8());
    if (doc.isNull())
    {
        qDebug() << "Parsing C-play file failed: " << filePath();
        return;
    }

    QJsonObject obj = doc.object();

    QStringList fileSearchPaths;
    fileSearchPaths.append(jsonFileInfo.absoluteDir().absolutePath());
    fileSearchPaths.append(GeneralSettings::cPlayMediaLocation());
    fileSearchPaths.append(GeneralSettings::cPlayFileLocation());
    fileSearchPaths.append(GeneralSettings::univiewVideoLocation());

    if (obj.contains("video")) {
        QString videoFile = obj.value("video").toString();
        QString videoFilePath = checkAndCorrectPath(videoFile, fileSearchPaths);
        if (!videoFilePath.isEmpty())
            setMediaFile(videoFilePath);
    }
    else if (obj.contains("stream")) {
        QString streamPath = obj.value("stream").toString();
        if (!streamPath.isEmpty())
            setMediaFile(streamPath);
    }

    if (obj.contains("overlay")) {
        QString overlayFile = obj.value("overlay").toString();
        QString overlayFilePath = checkAndCorrectPath(overlayFile, fileSearchPaths);
        if (!overlayFilePath.isEmpty())
            setSeparateOverlayFile(overlayFilePath);
    }

    if (obj.contains("audio")) {
        QString audioFile = obj.value("audio").toString();
        QString audioFilePath = checkAndCorrectPath(audioFile, fileSearchPaths);
        if (!audioFilePath.isEmpty())
            setSeparateAudioFile(audioFilePath);
    }

    if (obj.contains("title")) {
        QString title = obj.value("title").toString();
        setMediaTitle(title);
    }

    bool durationFound = false;
    if (obj.contains("duration")) {
        durationFound = true;
        double duration = obj.value("duration").toDouble();
        setDuration(duration);
    }

    if (obj.contains("grid")) {
        QString grid = obj.value("grid").toString();
        if (grid == "none") {
            setGridToMapOn(0);
        }
        else if (grid == "pre-split") {
            setGridToMapOn(0);
        }
        else if (grid == "plane") {
            setGridToMapOn(1);
        }
        else if (grid == "flat") {
            setGridToMapOn(1);
        }
        else if (grid == "dome") {
            setGridToMapOn(2);
        }
        else if (grid == "sphere") {
            setGridToMapOn(3);
        }
        else if (grid == "eqr") {
            setGridToMapOn(3);
        }
        else if (grid == "sphere-eqr") {
            setGridToMapOn(3);
        }
        else if (grid == "eac") {
            setGridToMapOn(4);
        }
        else if (grid == "sphere-eac") {
            setGridToMapOn(4);
        }
    }

    if (obj.contains("stereoscopic")) {
        QString stereo = obj.value("stereoscopic").toString();
        if (stereo == "no") {
            setStereoVideo(0);
        }
        else if (stereo == "mono") {
            setStereoVideo(0);
        }
        else if (stereo == "yes") { //Assume side-by-side if yes
            setStereoVideo(1);
        }
        else if (stereo == "sbs") {
            setStereoVideo(1);
        }
        else if (stereo == "side-by-side") {
            setStereoVideo(1);
        }
        else if (stereo == "tb") {
            setStereoVideo(2);
        }
        else if (stereo == "top-bottom") {
            setStereoVideo(2);
        }
        else if (stereo == "tbf") {
            setStereoVideo(3);
        }
        else if (stereo == "top-bottom-flip") {
            setStereoVideo(3);
        }
    }

    if (obj.contains("video_sections")) {
        m_data.m_sections.clear();
        QJsonValue value = obj.value("video_sections");
        QJsonArray array = value.toArray();
        foreach(const QJsonValue & v, array) {
            QJsonObject o = v.toObject();
            if (o.contains("section_title") && o.contains("time_begin") && o.contains("time_end")) {
                QString title = o.value("section_title").toString();
                double startTime = o.value("time_begin").toDouble();
                double endTime = o.value("time_end").toDouble();
                int eosMode = 0; //Pause by default
                if (o.contains("transition_at_end")) {
                    QString esoModeText = o.value("transition_at_end").toString();
                    if (esoModeText == "pause") {
                        eosMode = 0;
                    }
                    else if (esoModeText == "fade_out") {
                        eosMode = 1;
                    }
                    else if (esoModeText == "continue") {
                        eosMode = 2;
                    }
                    else if (esoModeText == "next") {
                        eosMode = 3;
                    }
                    else if (esoModeText == "loop") {
                        eosMode = 4;
                    }
                }
                addSection(title, startTime, endTime, eosMode);
            }
        }
    }
}

void PlayListItem::loadUniviewFDV()
{
    QFileInfo fileToLoad(filePath());
    if (!fileToLoad.exists())
    {
        qDebug() << "FDV files " << filePath() << " did not exist.";
        return;
    }

    QFile f(filePath());
    f.open(QIODevice::ReadOnly);
    QString fileContent = f.readAll();
    f.close();

    QStringList fileEntries = fileContent.split(QRegExp("[\r\n]"), Qt::SkipEmptyParts);
    qInfo() << "File Content: " << fileEntries;

    QString fileMainPath = GeneralSettings::univiewVideoLocation();

    if (!fileEntries.isEmpty()) {
        QString videoFileRelatiePath = fileEntries.at(1).mid(6); //"Video="
        QString videoFile = videoFileRelatiePath;
        if (!fileMainPath.isEmpty())
            videoFile = QDir::cleanPath(fileMainPath + QDir::separator() + videoFileRelatiePath);

        QString audioFileRelatiePath = fileEntries.at(2).mid(6); //"Audio="
        QString audioFile = audioFileRelatiePath;
        if (!fileMainPath.isEmpty())
            audioFile = QDir::cleanPath(fileMainPath + QDir::separator() + audioFileRelatiePath);

        QString title = fileEntries.at(3).mid(6); //"Title="
        double duration = fileEntries.at(4).mid(9).toDouble(); //"Duration="

        setMediaFile(videoFile);
        setSeparateAudioFile(audioFile);
        setMediaTitle(title);
        setDuration(duration);
        setGridToMapOn(0); //Assume Pre-split with FDV files

        if (title.contains("3D")) // Assume 3D side-by-side stereo
            setStereoVideo(1);
        else
            setStereoVideo(0);
    }
}

QString PlayListItem::checkAndCorrectPath(const QString& filePath, const QStringList& searchPaths) {
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists())
        return filePath;
    else if (fileInfo.isRelative()) { // Go through search list in order
        for (int i = 0; i < searchPaths.size(); i++) {
            QString newFilePath = QDir::cleanPath(searchPaths[i] + QDir::separator() + filePath);
            QFileInfo newFilePathInfo(newFilePath);
            if (newFilePathInfo.exists())
                return newFilePath;
        }
    }
    return "";
}
