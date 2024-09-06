/*
 * SPDX-FileCopyrightText:
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "playlistitem.h"
#include "_debug.h"
#include "locationsettings.h"
#include <algorithm>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>

QString PlayListItemData::filePath() const {
    return m_filePath;
}

QString PlayListItemData::fileName() const {
    return m_fileName;
}

QString PlayListItemData::fileFolderPath() const {
    return m_fileFolderPath;
}

QString PlayListItemData::mediaFile() const {
    return m_mediaFile;
}

QString PlayListItemData::mediaTitle() const {
    return m_mediaTitle;
}

double PlayListItemData::duration() const {
    return m_duration;
}

QString PlayListItemData::separateOverlayFile() const {
    return m_separateOverlayFile;
}

QString PlayListItemData::separateAudioFile() const {
    return m_separateAudioFile;
}

int PlayListItemData::eofMode() const {
    return m_eofMode;
}

int PlayListItemData::transitionMode() const {
    return m_transitionMode;
}

int PlayListItemData::gridToMapOn() const {
    return m_gridToMapOn;
}

int PlayListItemData::stereoVideo() const {
    return m_stereoVideo;
}

QList<PlayListItemData::Section> PlayListItemData::sections() const {
    return m_sections;
}

bool PlayListItemData::isPlaying() const {
    return m_isPlaying;
}

int PlayListItemData::index() const {
    return m_index;
}

PlayListItem::PlayListItem(const QString &path, int i, QObject *parent)
    : QObject(parent) {
    updateToNewFile(path);
    setIndex(i);
}

PlayListItem::PlayListItem(const PlayListItem &p1) {
    setData(p1.data());
}

PlayListItem::PlayListItem(const PlayListItemData &d) {
    setData(d);
}

PlayListItem &PlayListItem::operator=(const PlayListItem &p1) {
    PlayListItem *p2 = new PlayListItem(p1);
    return *p2;
}

QString PlayListItem::filePath() const {
    return m_data.m_filePath;
}

void PlayListItem::setFilePath(const QString &filePath) {
    m_data.m_filePath = filePath;
}

QString PlayListItem::fileName() const {
    return m_data.m_fileName;
}

void PlayListItem::setFileName(const QString &fileName) {
    m_data.m_fileName = fileName;
}

QString PlayListItem::fileFolderPath() const {
    return m_data.m_fileFolderPath;
}

void PlayListItem::setFileFolderPath(const QString &folderPath) {
    m_data.m_fileFolderPath = folderPath;
}

QString PlayListItem::mediaFile() const {
    return m_data.m_mediaFile;
}

void PlayListItem::setMediaFile(const QString &file) {
    m_data.m_mediaFile = file;
}

QString PlayListItem::mediaTitle() const {
    return m_data.m_mediaTitle;
}

void PlayListItem::setMediaTitle(const QString &title) {
    m_data.m_mediaTitle = title;
}

QString PlayListItem::durationFormatted() const {
    QTime t(0, 0, 0);
    QString formattedTime = t.addSecs(static_cast<qint64>(m_data.m_duration)).toString(QStringLiteral("hh:mm:ss"));
    return formattedTime;
}

double PlayListItem::duration() const {
    return m_data.m_duration;
}

void PlayListItem::setDuration(double duration) {
    m_data.m_duration = duration;
}

QString PlayListItem::separateOverlayFile() const {
    return m_data.m_separateOverlayFile;
}

void PlayListItem::setSeparateOverlayFile(const QString &overlayFile) {
    m_data.m_separateOverlayFile = overlayFile;
}

QString PlayListItem::separateAudioFile() const {
    return m_data.m_separateAudioFile;
}

void PlayListItem::setSeparateAudioFile(const QString &audioFile) {
    m_data.m_separateAudioFile = audioFile;
}

int PlayListItem::eofMode() const {
    return m_data.m_eofMode;
}

void PlayListItem::setEofMode(int eofMode) {
    m_data.m_eofMode = eofMode;
}

int PlayListItem::transitionMode() const {
    return m_data.m_transitionMode;
}

void PlayListItem::setTransitionMode(int transitionMode) {
    m_data.m_transitionMode = transitionMode;
}

int PlayListItem::gridToMapOn() const {
    return m_data.m_gridToMapOn;
}

void PlayListItem::setGridToMapOn(int gridToMapOn) {
    m_data.m_gridToMapOn = gridToMapOn;
}

int PlayListItem::stereoVideo() const {
    return m_data.m_stereoVideo;
}

void PlayListItem::setStereoVideo(int stereoVideo) {
    m_data.m_stereoVideo = stereoVideo;
}

bool PlayListItem::isPlaying() const {
    return m_data.m_isPlaying;
}

void PlayListItem::setIsPlaying(bool isPlaying) {
    m_data.m_isPlaying = isPlaying;
}

void PlayListItem::addSection(QString title, double startTime, double endTime, int eosMode) {
    m_data.m_sections.append(PlayListItemData::Section(title, startTime, endTime, eosMode));
}

void PlayListItem::addSection(QString title, QString startTime, QString endTime, int eosMode) {
    QTime start = QTime::fromString(startTime, QStringLiteral("hh:mm:ss"));
    QTime end = QTime::fromString(endTime, QStringLiteral("hh:mm:ss"));
    QTime zero = QTime(0, 0);
    addSection(title, double(zero.msecsTo(start)) / 1000.0, double(zero.msecsTo(end)) / 1000.0, eosMode);
}

void PlayListItem::removeSection(int i) {
    if (i < 0 || i >= m_data.m_sections.size())
        return;

    m_data.m_sections.removeAt(i);
}

void PlayListItem::replaceSection(int i, QString title, double startTime, double endTime, int eosMode) {
    if (i < 0 || i >= m_data.m_sections.size())
        return;

    m_data.m_sections.replace(i, PlayListItemData::Section(title, startTime, endTime, eosMode));
}

void PlayListItem::replaceSection(int i, QString title, QString startTime, QString endTime, int eosMode) {
    if (i < 0 || i >= m_data.m_sections.size())
        return;

    QTime start = QTime::fromString(startTime, QStringLiteral("hh:mm:ss"));
    QTime end = QTime::fromString(endTime, QStringLiteral("hh:mm:ss"));
    QTime zero = QTime(0, 0);
    replaceSection(i, title, double(zero.msecsTo(start)) / 1000.0, double(zero.msecsTo(end)) / 1000.0, eosMode);
}

void PlayListItem::moveSection(int from, int to) {
    if (from < 0 || from >= m_data.m_sections.size() ||
        to < 0 || to >= m_data.m_sections.size())
        return;

    m_data.m_sections.move(from, to);
}

QString PlayListItem::sectionTitle(int i) const {
    if (i < 0 || i >= m_data.m_sections.size())
        return QStringLiteral("");

    return m_data.m_sections.at(i).title;
}

double PlayListItem::sectionStartTime(int i) const {
    if (i < 0 || i >= m_data.m_sections.size())
        return 0;

    return m_data.m_sections.at(i).startTime;
}

double PlayListItem::sectionEndTime(int i) const {
    if (i < 0 || i >= m_data.m_sections.size())
        return 0;

    return m_data.m_sections.at(i).endTime;
}

int PlayListItem::sectionEOSMode(int i) const {
    if (i < 0 || i >= m_data.m_sections.size())
        return 0;

    return m_data.m_sections.at(i).eosMode;
}

int PlayListItem::numberOfSections() const {
    return m_data.m_sections.size();
}

const PlayListItemData::Section &PlayListItem::getSection(int i) const {
    return m_data.m_sections.at(i);
}

QList<PlayListItemData::Section> PlayListItem::sections() {
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

int PlayListItem::index() const {
    return m_data.m_index;
}

void PlayListItem::setIndex(int index) {
    m_data.m_index = index;
}

QString PlayListItem::makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider) const {
    // Assuming filePath is absolute
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            return foundDir.relativeFilePath(filePath);
        }
    }
    return filePath;
}

void PlayListItem::asJSON(QJsonObject &obj) {
    obj.insert(QStringLiteral("video"), mediaFile());

    if (!separateOverlayFile().isEmpty())
        obj.insert(QStringLiteral("overlay"), separateOverlayFile());

    if (!separateAudioFile().isEmpty())
        obj.insert(QStringLiteral("audio"), separateAudioFile());

    if (!mediaTitle().isEmpty())
        obj.insert(QStringLiteral("title"), mediaTitle());

    obj.insert(QStringLiteral("duration"), duration());

    QString grid;
    int gridIdx = gridToMapOn();
    if (gridIdx == 1) {
        grid = QStringLiteral("plane");
    } else if (gridIdx == 2) {
        grid = QStringLiteral("dome");
    } else if (gridIdx == 3) {
        grid = QStringLiteral("sphere-eqr");
    } else if (gridIdx == 4) {
        grid = QStringLiteral("sphere-eac");
    } else { // 0
        grid = QStringLiteral("pre-split");
    }
    obj.insert(QStringLiteral("grid"), grid);

    QString sv;
    int stereoVideoIdx = stereoVideo();
    if (stereoVideoIdx == 1) {
        sv = QStringLiteral("side-by-side");
    } else if (stereoVideoIdx == 2) {
        sv = QStringLiteral("top-bottom");
    } else if (stereoVideoIdx == 3) {
        sv = QStringLiteral("top-bottom-flip");
    } else { // 0
        sv = QStringLiteral("no");
    }
    obj.insert(QStringLiteral("stereoscopic"), sv);

    QJsonArray sectionsArray;
    for (int i = 0; i < m_data.m_sections.size(); i++) {
        QJsonObject item_data;

        int eosMode = m_data.m_sections[i].eosMode;
        QString eosModeText;
        switch (eosMode) {
        case 1:
            eosModeText = QStringLiteral("fade_out");
            break;
        case 2:
            eosModeText = QStringLiteral("continue");
            break;
        case 3:
            eosModeText = QStringLiteral("next");
            break;
        case 4:
            eosModeText = QStringLiteral("loop");
            break;
        default:
            eosModeText = QStringLiteral("pause");
            break;
        }

        item_data.insert(QStringLiteral("section_title"), QJsonValue(m_data.m_sections[i].title));
        item_data.insert(QStringLiteral("time_begin"), QJsonValue(m_data.m_sections[i].startTime));
        item_data.insert(QStringLiteral("time_end"), QJsonValue(m_data.m_sections[i].endTime));
        item_data.insert(QStringLiteral("transition_at_end"), QJsonValue(eosModeText));

        sectionsArray.push_back(QJsonValue(item_data));
    }
    if (!sectionsArray.isEmpty())
        obj.insert(QStringLiteral("video_sections"), QJsonValue(sectionsArray));
}

void PlayListItem::saveAsJSONPlayFile(const QString &path) const {
    QJsonDocument doc;
    QJsonObject obj = doc.object();

    QString fileToSave = path;
    fileToSave.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileToSaveInfo(fileToSave);

    QStringList pathsToConsider;
    pathsToConsider.append(LocationSettings::cPlayMediaLocation());
    pathsToConsider.append(fileToSaveInfo.absoluteDir().absolutePath());

    obj.insert(QStringLiteral("video"), makePathRelativeTo(mediaFile(), pathsToConsider));

    if (!separateOverlayFile().isEmpty())
        obj.insert(QStringLiteral("overlay"), makePathRelativeTo(separateOverlayFile(), pathsToConsider));

    if (!separateAudioFile().isEmpty())
        obj.insert(QStringLiteral("audio"), makePathRelativeTo(separateAudioFile(), pathsToConsider));

    if (!mediaTitle().isEmpty())
        obj.insert(QStringLiteral("title"), mediaTitle());

    obj.insert(QStringLiteral("duration"), duration());

    QString grid;
    int gridIdx = gridToMapOn();
    if (gridIdx == 1) {
        grid = QStringLiteral("plane");
    } else if (gridIdx == 2) {
        grid = QStringLiteral("dome");
    } else if (gridIdx == 3) {
        grid = QStringLiteral("sphere-eqr");
    } else if (gridIdx == 4) {
        grid = QStringLiteral("sphere-eac");
    } else { // 0
        grid = QStringLiteral("pre-split");
    }
    obj.insert(QStringLiteral("grid"), grid);

    QString sv;
    int stereoVideoIdx = stereoVideo();
    if (stereoVideoIdx == 1) {
        sv = QStringLiteral("side-by-side");
    } else if (stereoVideoIdx == 2) {
        sv = QStringLiteral("top-bottom");
    } else if (stereoVideoIdx == 3) {
        sv = QStringLiteral("top-bottom-flip");
    } else { // 0
        sv = QStringLiteral("no");
    }
    obj.insert(QStringLiteral("stereoscopic"), sv);

    QJsonArray sectionsArray;
    for (int i = 0; i < m_data.m_sections.size(); i++) {
        QJsonObject item_data;

        int eosMode = m_data.m_sections[i].eosMode;
        QString eosModeText;
        switch (eosMode) {
        case 1:
            eosModeText = QStringLiteral("fade_out");
            break;
        case 2:
            eosModeText = QStringLiteral("continue");
            break;
        case 3:
            eosModeText = QStringLiteral("next");
            break;
        case 4:
            eosModeText = QStringLiteral("loop");
            break;
        default:
            eosModeText = QStringLiteral("pause");
            break;
        }

        item_data.insert(QStringLiteral("section_title"), QJsonValue(m_data.m_sections[i].title));
        item_data.insert(QStringLiteral("time_begin"), QJsonValue(m_data.m_sections[i].startTime));
        item_data.insert(QStringLiteral("time_end"), QJsonValue(m_data.m_sections[i].endTime));
        item_data.insert(QStringLiteral("transition_at_end"), QJsonValue(eosModeText));

        sectionsArray.push_back(QJsonValue(item_data));
    }
    if (!sectionsArray.isEmpty())
        obj.insert(QStringLiteral("video_sections"), QJsonValue(sectionsArray));

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

void PlayListItem::updateToNewFile(const QString &path) {
    QUrl url(path);

    if (url.scheme().startsWith(QStringLiteral("http"))) {
        setFilePath(url.toString());
        setFileName(QStringLiteral(""));
        setFileFolderPath(QStringLiteral(""));
        setMediaFile(url.toString());
    } else {
        QFileInfo fileInfo(path);
        setFileName(fileInfo.fileName());
        setFilePath(fileInfo.absoluteFilePath());
        setFileFolderPath(fileInfo.absolutePath());
        setMediaFile(fileInfo.absoluteFilePath());
    }
    setSeparateOverlayFile(QStringLiteral(""));
    setSeparateAudioFile(QStringLiteral(""));
    m_hasDescriptionFile = false;
}

void PlayListItem::loadDetailsFromDisk() {
    QFileInfo checkedFilePathInfo(filePath());
    QString fileExt = checkedFilePathInfo.suffix();

    m_hasDescriptionFile = false;

    if (fileExt == QStringLiteral("cplayfile") || fileExt == QStringLiteral("cplay_file")) {
        loadJSONPlayfile();
    } else if (fileExt == QStringLiteral("fdv")) {
        loadUniviewFDV();
    }
}

bool PlayListItem::hasDescriptionFile() {
    return m_hasDescriptionFile;
}

void PlayListItem::loadJSONPlayfile() {
    QFileInfo jsonFileInfo(filePath());
    if (!jsonFileInfo.exists()) {
        qDebug() << QStringLiteral("C-play file ") << filePath() << QStringLiteral(" did not exist.");
        return;
    }

    QFile f(filePath());
    f.open(QIODevice::ReadOnly);
    QByteArray fileContent = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileContent);
    if (doc.isNull()) {
        qDebug() << QStringLiteral("Parsing C-play file failed: ") << filePath();
        return;
    }

    QJsonObject obj = doc.object();

    QStringList fileSearchPaths;
    fileSearchPaths.append(jsonFileInfo.absoluteDir().absolutePath());
    fileSearchPaths.append(LocationSettings::cPlayMediaLocation());
    fileSearchPaths.append(LocationSettings::cPlayFileLocation());
    fileSearchPaths.append(LocationSettings::univiewVideoLocation());

    if (obj.contains(QStringLiteral("video"))) {
        QString videoFile = obj.value(QStringLiteral("video")).toString();
        QString videoFilePath = checkAndCorrectPath(videoFile, fileSearchPaths);
        if (!videoFilePath.isEmpty())
            setMediaFile(videoFilePath);
    } else if (obj.contains(QStringLiteral("stream"))) {
        QString streamPath = obj.value(QStringLiteral("stream")).toString();
        if (!streamPath.isEmpty())
            setMediaFile(streamPath);
    }

    if (obj.contains(QStringLiteral("overlay"))) {
        QString overlayFile = obj.value(QStringLiteral("overlay")).toString();
        QString overlayFilePath = checkAndCorrectPath(overlayFile, fileSearchPaths);
        if (!overlayFilePath.isEmpty())
            setSeparateOverlayFile(overlayFilePath);
    }

    if (obj.contains(QStringLiteral("audio"))) {
        QString audioFile = obj.value(QStringLiteral("audio")).toString();
        QString audioFilePath = checkAndCorrectPath(audioFile, fileSearchPaths);
        if (!audioFilePath.isEmpty())
            setSeparateAudioFile(audioFilePath);
    }

    if (obj.contains(QStringLiteral("title"))) {
        QString title = obj.value(QStringLiteral("title")).toString();
        setMediaTitle(title);
    }

    bool durationFound = false;
    if (obj.contains(QStringLiteral("duration"))) {
        durationFound = true;
        double duration = obj.value(QStringLiteral("duration")).toDouble();
        setDuration(duration);
    }

    if (obj.contains(QStringLiteral("grid"))) {
        QString grid = obj.value(QStringLiteral("grid")).toString();
        if (grid == QStringLiteral("none")) {
            setGridToMapOn(0);
        } else if (grid == QStringLiteral("pre-split")) {
            setGridToMapOn(0);
        } else if (grid == QStringLiteral("plane")) {
            setGridToMapOn(1);
        } else if (grid == QStringLiteral("flat")) {
            setGridToMapOn(1);
        } else if (grid == QStringLiteral("dome")) {
            setGridToMapOn(2);
        } else if (grid == QStringLiteral("sphere")) {
            setGridToMapOn(3);
        } else if (grid == QStringLiteral("eqr")) {
            setGridToMapOn(3);
        } else if (grid == QStringLiteral("sphere-eqr")) {
            setGridToMapOn(3);
        } else if (grid == QStringLiteral("eac")) {
            setGridToMapOn(4);
        } else if (grid == QStringLiteral("sphere-eac")) {
            setGridToMapOn(4);
        }
    }

    if (obj.contains(QStringLiteral("stereoscopic"))) {
        QString stereo = obj.value(QStringLiteral("stereoscopic")).toString();
        if (stereo == QStringLiteral("no")) {
            setStereoVideo(0);
        } else if (stereo == QStringLiteral("mono")) {
            setStereoVideo(0);
        } else if (stereo == QStringLiteral("yes")) { // Assume side-by-side if yes
            setStereoVideo(1);
        } else if (stereo == QStringLiteral("sbs")) {
            setStereoVideo(1);
        } else if (stereo == QStringLiteral("side-by-side")) {
            setStereoVideo(1);
        } else if (stereo == QStringLiteral("tb")) {
            setStereoVideo(2);
        } else if (stereo == QStringLiteral("top-bottom")) {
            setStereoVideo(2);
        } else if (stereo == QStringLiteral("tbf")) {
            setStereoVideo(3);
        } else if (stereo == QStringLiteral("top-bottom-flip")) {
            setStereoVideo(3);
        }
    }

    if (obj.contains(QStringLiteral("video_sections"))) {
        m_data.m_sections.clear();
        QJsonValue value = obj.value(QStringLiteral("video_sections"));
        QJsonArray array = value.toArray();
        for (auto v : array) {
            QJsonObject o = v.toObject();
            if (o.contains(QStringLiteral("section_title")) && o.contains(QStringLiteral("time_begin")) && o.contains(QStringLiteral("time_end"))) {
                QString title = o.value(QStringLiteral("section_title")).toString();
                double startTime = o.value(QStringLiteral("time_begin")).toDouble();
                double endTime = o.value(QStringLiteral("time_end")).toDouble();
                int eosMode = 0; // Pause by default
                if (o.contains(QStringLiteral("transition_at_end"))) {
                    QString eosModeText = o.value(QStringLiteral("transition_at_end")).toString();
                    if (eosModeText == QStringLiteral("pause")) {
                        eosMode = 0;
                    } else if (eosModeText == QStringLiteral("fade_out")) {
                        eosMode = 1;
                    } else if (eosModeText == QStringLiteral("continue")) {
                        eosMode = 2;
                    } else if (eosModeText == QStringLiteral("next")) {
                        eosMode = 3;
                    } else if (eosModeText == QStringLiteral("loop")) {
                        eosMode = 4;
                    }
                }
                addSection(title, startTime, endTime, eosMode);
            }
        }
    }

    m_hasDescriptionFile = true;
}

void PlayListItem::loadUniviewFDV() {
    QFileInfo fileToLoad(filePath());
    if (!fileToLoad.exists()) {
        qDebug() << QStringLiteral("FDV files ") << filePath() << QStringLiteral(" did not exist.");
        return;
    }

    QFile f(filePath());
    f.open(QIODevice::ReadOnly);
    QString fileContent = QString::fromUtf8(f.readAll());
    f.close();

    QStringList fileEntries = fileContent.split(QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts);
    qInfo() << QStringLiteral("File Content: ") << fileEntries;

    QString fileMainPath = LocationSettings::univiewVideoLocation();

    if (!fileEntries.isEmpty()) {
        QString videoFileRelatiePath = fileEntries.at(1).mid(6); //"Video="
        QString videoFile = videoFileRelatiePath;
        if (!fileMainPath.isEmpty())
            videoFile = QDir::cleanPath(fileMainPath + QDir::separator() + videoFileRelatiePath);

        QString audioFileRelatiePath = fileEntries.at(2).mid(6); //"Audio="
        QString audioFile = audioFileRelatiePath;
        if (!fileMainPath.isEmpty())
            audioFile = QDir::cleanPath(fileMainPath + QDir::separator() + audioFileRelatiePath);

        QString title = fileEntries.at(3).mid(6);              //"Title="
        double duration = fileEntries.at(4).mid(9).toDouble(); //"Duration="

        setMediaFile(videoFile);
        setSeparateAudioFile(audioFile);
        setMediaTitle(title);
        setDuration(duration);
        setGridToMapOn(0); // Assume Pre-split with FDV files

        if (title.contains(QStringLiteral("3D"))) // Assume 3D side-by-side stereo
            setStereoVideo(1);
        else
            setStereoVideo(0);
    }

    m_hasDescriptionFile = true;
}

QString PlayListItem::checkAndCorrectPath(const QString &filePath, const QStringList &searchPaths) {
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
    return QStringLiteral("");
}
