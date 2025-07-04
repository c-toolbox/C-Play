/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "playlistmodel.h"
#include "_debug.h"
#include "application.h"
#include "locationsettings.h"
#include "playlistitem.h"
#include "worker.h"

#include <QCollator>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeDatabase>
#include <QUrl>

PlaySectionsModel::PlaySectionsModel(QObject *parent)
    : QAbstractListModel(parent),
      m_currentEditItem(nullptr) {
}

int PlaySectionsModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid() || !m_currentEditItem)
        return 0;

    return m_currentEditItem->numberOfSections();
}

QVariant PlaySectionsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || !m_currentEditItem || m_currentEditItem->numberOfSections() == 0)
        return QVariant();

    auto playListItemSection = m_currentEditItem->getSection(index.row());

    switch (role) {
    case TitleRole:
        return QVariant(playListItemSection.title);
    case StartTimeRole:
        return QVariant(Application::formatTime(playListItemSection.startTime));
    case EndTimeRole:
        return QVariant(Application::formatTime(playListItemSection.endTime));
    case DurationRole:
        return QVariant(Application::formatTime(playListItemSection.endTime - playListItemSection.startTime));
    case EndOfSectionModeRole:
        if (playListItemSection.eosMode == 1) { // Fade out
            return QVariant(QStringLiteral("Fade out"));
        } else if (playListItemSection.eosMode == 2) { // Continue
            return QVariant(QStringLiteral("Continue playing"));
        } else if (playListItemSection.eosMode == 3) { // Next
            return QVariant(QStringLiteral("Next section"));
        } else if (playListItemSection.eosMode == 4) { // Loop
            return QVariant(QStringLiteral("Loop section"));
        } else { // Pause (0)
            return QVariant(QStringLiteral("Pause"));
        }
    case PlayingRole:
        return QVariant(playListItemSection.isPlaying);
    }

    return QVariant();
}

QHash<int, QByteArray> PlaySectionsModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[TitleRole] = "title";
    roles[StartTimeRole] = "startTime";
    roles[EndTimeRole] = "endTime";
    roles[DurationRole] = "duration";
    roles[EndOfSectionModeRole] = "eosMode";
    roles[PlayingRole] = "isPlaying";
    return roles;
}

void PlaySectionsModel::setPlayingSection(int section) {
    if (!m_currentEditItem)
        return;

    if (m_playingSection >= 0 && m_playingSection < m_currentEditItem->numberOfSections()) {
        // unset current playing section
        m_currentEditItem->setIsSectionPlaying(m_playingSection, false);
        Q_EMIT dataChanged(index(m_playingSection, 0), index(m_playingSection, 0));
    }

    if (section < 0 || section >= m_currentEditItem->numberOfSections()) {
        m_playingSection = -1;
        Q_EMIT playingSectionChanged();
        return;
    }

    // set new playing section
    m_currentEditItem->setIsSectionPlaying(section, true);
    Q_EMIT dataChanged(index(section, 0), index(section, 0));

    m_playingSection = section;

    Q_EMIT playingSectionChanged();
}

int PlaySectionsModel::getPlayingSection() {
    if (!m_currentEditItem)
        return -1;

    return m_playingSection;
}

int PlaySectionsModel::getNumberOfSections() {
    if (!m_currentEditItem)
        return -1;

    return m_currentEditItem->numberOfSections();
}

void PlaySectionsModel::clear() {
    m_playingSection = -1;
    beginResetModel();
    delete m_currentEditItem;
    m_currentEditItem = nullptr;
    endResetModel();
}

bool PlaySectionsModel::isEmpty() {
    return m_currentEditItem == nullptr;
}

PlayListItem *PlaySectionsModel::currentEditItem() {
    return m_currentEditItem;
}

void PlaySectionsModel::setCurrentEditItem(PlayListItem *item) {
    m_playingSection = -1;
    beginResetModel();
    m_currentEditItem = item;
    Q_EMIT currentEditItemChanged();
    endResetModel();
}

void PlaySectionsModel::updateCurrentEditItem(PlayListItem &item) {
    m_playingSection = -1;
    beginResetModel();
    if (!m_currentEditItem) {
        m_currentEditItem = new PlayListItem(item.data());
    } else {
        m_currentEditItem->setData(item.data());
    }
    Q_EMIT currentEditItemChanged();
    endResetModel();
}

void PlaySectionsModel::setCurrentEditItemIsEdited(bool value) {
    m_currentEditItemIsEdited = value;
    Q_EMIT currentEditItemIsEditedChanged();
}

bool PlaySectionsModel::getCurrentEditItemIsEdited() {
    return m_currentEditItemIsEdited;
}

QUrl PlaySectionsModel::getSuggestedFileURL() {
    QString suggestFolderPath;
    if (!LocationSettings::cPlayFileLocation().isEmpty()) {
        suggestFolderPath = LocationSettings::cPlayFileLocation();
    } else {
        suggestFolderPath = LocationSettings::fileDialogLastLocation();
    }

    if (m_currentEditItem) {
        QFileInfo filePathInfo(m_currentEditItem->filePath());
        QString fileExt = filePathInfo.suffix();
        if (fileExt == QStringLiteral("cplayfile") || fileExt == QStringLiteral("cplay_file")) {
            return QUrl(QStringLiteral("file:///") + m_currentEditItem->filePath());
        } else {
            QString fileName = filePathInfo.baseName();
            if (fileName.isEmpty()) {
                QFileInfo mediaInfo(m_currentEditItem->mediaFile());
                fileName = mediaInfo.baseName();
            }
            fileName.replace(QStringLiteral(" "), QStringLiteral("_"));
            return QUrl(QStringLiteral("file:///") + suggestFolderPath + QStringLiteral("/") + fileName + QStringLiteral(".cplayfile"));
        }
    } else {
        return QUrl();
    }
}

void PlaySectionsModel::addSection(QString name, QString startTime, QString endTime, int eosMode) {
    if (!m_currentEditItem)
        return;

    beginInsertRows(QModelIndex(), m_currentEditItem->numberOfSections(), m_currentEditItem->sections().size());
    m_currentEditItem->addSection(name, startTime, endTime, eosMode);
    endInsertRows();
    setCurrentEditItemIsEdited(true);
}

void PlaySectionsModel::removeSection(int i) {
    if (!m_currentEditItem)
        return;

    beginRemoveRows(QModelIndex(), i, i);
    if (m_playingSection == i) {
        m_playingSection = -1;
    }
    m_currentEditItem->removeSection(i);
    endRemoveRows();
    setCurrentEditItemIsEdited(true);
}

void PlaySectionsModel::replaceSection(int i, QString name, QString startTime, QString endTime, int eosMode) {
    if (!m_currentEditItem)
        return;

    beginResetModel();
    m_currentEditItem->replaceSection(i, name, startTime, endTime, eosMode);
    endResetModel();
    setCurrentEditItemIsEdited(true);
}

void PlaySectionsModel::moveSectionUp(int i) {
    if (!m_currentEditItem)
        return;

    if (i == 0)
        return;

    beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1);
    m_currentEditItem->moveSection(i, i - 1);
    endMoveRows();
    setCurrentEditItemIsEdited(true);
}

void PlaySectionsModel::moveSectionDown(int i) {
    if (!m_currentEditItem)
        return;

    if (i == (m_currentEditItem->numberOfSections() - 1))
        return;

    beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i);
    m_currentEditItem->moveSection(i, i + 1);
    endMoveRows();
    setCurrentEditItemIsEdited(true);
}

QString PlaySectionsModel::sectionTitle(int i) const {
    if (!m_currentEditItem)
        return QStringLiteral("");

    return m_currentEditItem->sectionTitle(i);
}

double PlaySectionsModel::sectionStartTime(int i) const {
    if (!m_currentEditItem)
        return -1;

    return m_currentEditItem->sectionStartTime(i);
}

double PlaySectionsModel::sectionEndTime(int i) const {
    if (!m_currentEditItem)
        return -1;

    return m_currentEditItem->sectionEndTime(i);
}

int PlaySectionsModel::sectionEOSMode(int i) const {
    if (!m_currentEditItem)
        return 0;

    return m_currentEditItem->sectionEOSMode(i);
}

std::string PlaySectionsModel::getSectionsAsFormattedString(size_t charsPerItem) const {
    if (!m_currentEditItem)
        return "";

    std::string fullItemList = "";
    for (int i = 0; i < m_currentEditItem->numberOfSections(); i++) {
        std::string title = std::to_string(i + 1) + ". ";

        auto playListItemSection = m_currentEditItem->getSection(i);

        title += playListItemSection.title.toStdString();

        std::string duration = Application::formatTime(playListItemSection.endTime - playListItemSection.startTime).toStdString();

        size_t countChars = title.size() + duration.size();
        if (countChars < charsPerItem) {
            title.insert(title.end(), charsPerItem - countChars, ' ');
        } else if (countChars >= charsPerItem) {
            title.erase(title.end() - (countChars - charsPerItem + 4), title.end());
            title.insert(title.end(), 3, '.');
            title.insert(title.end(), 1, ' ');
        }
        std::string itemText = title + duration;
        fullItemList += itemText;
        if (i < m_currentEditItem->numberOfSections() - 1)
            fullItemList += "\n";
    }
    return fullItemList;
}

PlayListModel::PlayListModel(QObject *parent)
    : QAbstractListModel(parent) {
    m_config = KSharedConfig::openConfig(QStringLiteral("C-Play/cplay.conf"));
    connect(this, &PlayListModel::videoAdded,
            Worker::instance(), &Worker::getMetaData);

    connect(Worker::instance(), &Worker::metaDataReady, this, [=](int i, KFileMetaData::PropertyMultiMap metaData) {
        auto duration = metaData.value(KFileMetaData::Property::Duration).toInt();
        auto title = metaData.value(KFileMetaData::Property::Title).toString();

        m_playList[i]->setDuration(duration);
        m_playList[i]->setMediaTitle(title);

        Q_EMIT dataChanged(index(i, 0), index(i, 0));
    });

    m_playListName = QStringLiteral("");
    m_playListPath = QStringLiteral("");
}

int PlayListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_playList.size();
}

QVariant PlayListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_playList.empty())
        return QVariant();

    auto playListItem = m_playList.at(index.row());

    if (!playListItem) {
        qWarning() << "PlayListItem pointer was null";
        return QVariant();
    }

    int stereoVideo = playListItem->stereoVideo();
    int gridToMapOn = playListItem->gridToMapOn();
    int eofMode = playListItem->eofMode();

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
        return QVariant(Application::formatTime(playListItem->duration()));
    case PlayingRole:
        return QVariant(playListItem->isPlaying());
    case FolderPathRole:
        return QVariant(playListItem->fileFolderPath());
    case StereoRole:
        if (stereoVideo == 0) {
            return QVariant(QStringLiteral("2D"));
        } else if (stereoVideo > 0) {
            return QVariant(QStringLiteral("3D"));
        } else {
            return QVariant(QStringLiteral(""));
        }
    case GridRole:
        if (gridToMapOn == 0) {
            return QVariant(QStringLiteral("Split"));
        } else if (gridToMapOn == 1) {
            return QVariant(QStringLiteral("Flat"));
        } else if (gridToMapOn == 2) {
            return QVariant(QStringLiteral("Dome"));
        } else if (gridToMapOn == 3 || gridToMapOn == 4) {
            return QVariant(QStringLiteral("Sphere"));
        } else {
            return QVariant(QStringLiteral(""));
        }
    case EofRole:
        if (eofMode == 1) { // Continue
            return QVariant(QStringLiteral("Continue to next"));
        } else if (eofMode == 2) { // Loop
            return QVariant(QStringLiteral("Loop video"));
        } else { // Pause (0)
            return QVariant(QStringLiteral("Pause at end"));
        }
    case HasDescriptionFileRole:
        return QVariant(playListItem->hasDescriptionFile());
    }

    return QVariant();
}

QHash<int, QByteArray> PlayListModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[TitleRole] = "title";
    roles[PathRole] = "path";
    roles[FolderPathRole] = "folderPath";
    roles[DurationRole] = "duration";
    roles[PlayingRole] = "isPlaying";
    roles[StereoRole] = "stereoVideo";
    roles[GridRole] = "gridToMapOn";
    roles[EofRole] = "eofMode";
    roles[HasDescriptionFileRole] = "hasDescriptionFile";
    return roles;
}

void PlayListModel::getVideos(QString path) {
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
            if (fileInfo.exists() && type.name().startsWith(QStringLiteral("video/"))) {
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
        Q_EMIT videoAdded(i, video->filePath());
    }

    endInsertRows();
}

Playlist PlayListModel::items() const {
    return m_playList;
}

QString PlayListModel::configFolder() {
    auto configPath = QStandardPaths::writableLocation(m_config->locationType());
    auto configFilePath = configPath.append(QStringLiteral("/")).append(m_config->name());
    QFileInfo fileInfo(configFilePath);

    return fileInfo.absolutePath();
}

Playlist PlayListModel::getPlayList() const {
    return m_playList;
}

void PlayListModel::setPlayList(const Playlist &playList) {
    beginInsertRows(QModelIndex(), 0, playList.size() - 1);
    m_playList = playList;
    endInsertRows();
    setPlayListIsEdited(false);
}

std::string PlayListModel::getListAsFormattedString(int charsPerItem) const {
    std::string fullItemList = "";
    for (int i = 0; i < m_playList.size(); i++) {
        std::string title = std::to_string(i + 1) + ". ";
        if (m_playList[i]->mediaTitle().isEmpty()) {
            title += m_playList[i]->fileName().toStdString();
        } else {
            title += m_playList[i]->mediaTitle().toStdString();
        }
        std::string duration = Application::formatTime(m_playList[i]->duration()).toStdString();

        size_t countChars = title.size() + duration.size();
        if (countChars < charsPerItem) {
            title.insert(title.end(), charsPerItem - countChars, ' ');
        } else if (countChars >= charsPerItem) {
            title.erase(title.end() - (countChars - charsPerItem + 4), title.end());
            title.insert(title.end(), 3, '.');
            title.insert(title.end(), 1, ' ');
        }
        std::string itemText = title + duration;
        fullItemList += itemText;
        if (i < m_playList.size() - 1)
            fullItemList += "\n";
    }
    return fullItemList;
}

void PlayListModel::setPlayListName(QString name) {
    m_playListName = name;
}

QString PlayListModel::getPlayListName() const {
    return m_playListName;
}

void PlayListModel::setPlayListPath(QString path) {
    m_playListPath = path;
}

QString PlayListModel::getPlayListPath() const {
    return m_playListPath;
}

QUrl PlayListModel::getPlayListPathAsURL() const {
    return QUrl(QStringLiteral("file:///") + m_playListPath);
}

int PlayListModel::getPlayingVideo() const {
    return m_playingVideo;
}

void PlayListModel::clear() {
    m_playingVideo = -1;
    qDeleteAll(m_playList);
    beginResetModel();
    m_playList.clear();
    m_playListName = QStringLiteral("");
    m_playListPath = QStringLiteral("");
    endResetModel();
}

QString PlayListModel::getPath(int i) {
    // when restoring a youtube playlist
    // ensure the requested path is valid
    if (m_playList.size() <= i) {
        return m_playList[0]->mediaFile();
    }
    return m_playList[i]->mediaFile();
}

int PlayListModel::getPlayListSize() const {
    return m_playList.size();
}

void PlayListModel::addItem(PlayListItem *item) {
    beginInsertRows(QModelIndex(), m_playList.size(), m_playList.size());
    m_playList.append(QPointer<PlayListItem>(item));
    endInsertRows();
    setPlayListIsEdited(true);
}

QPointer<PlayListItem> PlayListModel::getItem(int i) {
    if (i < 0 || i >= getPlayListSize()) {
        return nullptr;
    }

    return m_playList[i];
}

void PlayListModel::removeItem(int i) {
    if (i < 0 || i >= m_playList.size())
        return;

    beginRemoveRows(QModelIndex(), i, i);
    m_playList.removeAt(i);
    if (m_playingVideo == i)
        m_playingVideo = -1;
    else if (m_playingVideo > i)
        m_playingVideo -= 1;
    endRemoveRows();
    setPlayListIsEdited(true);
}

void PlayListModel::moveItemUp(int i) {
    if (i < 1)
        return;
    beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1);
    m_playList.move(i, i - 1);
    if (m_playingVideo == i)
        m_playingVideo -= 1;
    else if (m_playingVideo == i - 1)
        m_playingVideo = i;
    endMoveRows();
    setPlayListIsEdited(true);
}

void PlayListModel::moveItemDown(int i) {
    if (i < 0 || i == (m_playList.size() - 1))
        return;
    beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i);
    m_playList.move(i, i + 1);
    if (m_playingVideo == i)
        m_playingVideo += 1;
    else if (m_playingVideo == i - 1)
        m_playingVideo = i;
    endMoveRows();
    setPlayListIsEdited(true);
}

void PlayListModel::updateItem(int i) {
    Q_EMIT dataChanged(index(i, 0), index(i, 0));
    setPlayListIsEdited(true);
}

void PlayListModel::setPlayListIsEdited(bool value) {
    m_playListEdited = value;
    Q_EMIT playListIsEditedChanged();
}

bool PlayListModel::getPlayListIsEdited() {
    return m_playListEdited;
}

void PlayListModel::setPlayingVideo(int playingVideo) {
    // unset current playing video
    if (m_playingVideo >= 0 && m_playingVideo < m_playList.size()) {
        m_playList[m_playingVideo]->setIsPlaying(false);
        Q_EMIT dataChanged(index(m_playingVideo, 0), index(m_playingVideo, 0));
    }

    if (playingVideo < 0 || playingVideo >= m_playList.size()) {
        m_playingVideo = -1;
        Q_EMIT playingVideoChanged();
        return;
    }

    // set new playing video
    m_playList[playingVideo]->setIsPlaying(true);
    Q_EMIT dataChanged(index(playingVideo, 0), index(playingVideo, 0));

    m_playingVideo = playingVideo;
    Q_EMIT playingVideoChanged();
}

QString PlayListModel::filePath(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->filePath();
    else
        return QStringLiteral("");
}

QUrl PlayListModel::filePathAsURL(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return QUrl(QStringLiteral("file:///") + m_playList[i].data()->filePath());
    else
        return QUrl();
}

QString PlayListModel::fileName(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->fileName();
    else
        return QStringLiteral("");
}

QString PlayListModel::fileFolderPath(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->fileFolderPath();
    else
        return QStringLiteral("");
}

QUrl PlayListModel::fileFolderPathAsURL(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return QUrl(QStringLiteral("file:///") + m_playList[i].data()->fileFolderPath());
    else
        return QUrl();
}

QString PlayListModel::mediaFile(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->mediaFile();
    else
        return QStringLiteral("");
}

QString PlayListModel::mediaTitle(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->mediaTitle();
    else
        return QStringLiteral("");
}

QString PlayListModel::duration(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->durationFormatted();
    else
        return QStringLiteral("");
}

QString PlayListModel::separateAudioFile(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->separateAudioFile();
    else
        return QStringLiteral("");
}

QString PlayListModel::separateOverlayFile(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->separateOverlayFile();
    else
        return QStringLiteral("");
}

int PlayListModel::eofMode(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->eofMode();
    else
        return m_defaultEofMode;
}

void PlayListModel::setEofMode(int i, int eofMode) {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        m_playList[i]->setEofMode(eofMode);
}

int PlayListModel::transitionMode(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->transitionMode();
    else
        return 0;
}

int PlayListModel::gridToMapOn(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->gridToMapOn();
    else
        return 0;
}

int PlayListModel::stereoVideo(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->stereoVideo();
    else
        return 0;
}

int PlayListModel::numberOfSections(int i) const {
    if (i >= 0 && m_playList.size() > i && m_playList[i])
        return m_playList[i].data()->numberOfSections();
    else
        return 0;
}

QString PlayListModel::makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider) {
    // Assuming filePath is absolute
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            return foundDir.relativeFilePath(filePath);
        }
    }
    return filePath;
}

void PlayListModel::saveAsJSONPlaylist(const QString &path) {
    QJsonDocument doc;
    QJsonObject obj = doc.object();

    QString fileToSave = path;
    fileToSave.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileToSaveInfo(fileToSave);

    QStringList pathsToConsider;
    pathsToConsider.append(LocationSettings::cPlayFileLocation());
    pathsToConsider.append(fileToSaveInfo.absoluteDir().absolutePath());

    QJsonArray playlistArray;
    for (int i = 0; i < m_playList.size(); i++) {
        QJsonObject item_data;

        int eofMode = m_playList[i]->eofMode();
        QString eofModeText;
        switch (eofMode) {
        case 1:
            eofModeText = QStringLiteral("continue");
            break;
        case 2:
            eofModeText = QStringLiteral("loop");
            break;
        default:
            eofModeText = QStringLiteral("pause");
            break;
        }

        QString checkedFilePath = makePathRelativeTo(m_playList[i]->filePath(), pathsToConsider);

        item_data.insert(QStringLiteral("file"), QJsonValue(checkedFilePath));
        item_data.insert(QStringLiteral("on_file_end"), QJsonValue(eofModeText));

        playlistArray.push_back(QJsonValue(item_data));
    }

    obj.insert(QString(QStringLiteral("playlist")), QJsonValue(playlistArray));
    doc.setObject(obj);

    QFile jsonFile(fileToSave);
    jsonFile.open(QFile::WriteOnly);
    jsonFile.write(doc.toJson());
    jsonFile.close();

    QFileInfo fileInfo(jsonFile);
    setPlayListName(fileInfo.baseName());

    setPlayListIsEdited(false);
}

void PlayListModel::asJSON(QJsonObject& obj) {

    QJsonArray playlistArray;
    for (int i = 0; i < m_playList.size(); i++) {
        QJsonObject item_data;

        m_playList[i]->asJSON(item_data);

        int eofMode = m_playList[i]->eofMode();
        QString eofModeText;
        switch (eofMode) {
        case 1:
            eofModeText = QStringLiteral("continue");
            break;
        case 2:
            eofModeText = QStringLiteral("loop");
            break;
        default:
            eofModeText = QStringLiteral("pause");
            break;
        }
        item_data.insert(QStringLiteral("on_file_end"), QJsonValue(eofModeText));

        playlistArray.push_back(QJsonValue(item_data));
    }

    obj.insert(QString(QStringLiteral("playlist")), QJsonValue(playlistArray));
}
