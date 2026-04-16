/*
 * SPDX-FileCopyrightText:
 * 2021-2026 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QPointer>
#include <QtQml/qqmlregistration.h>
#include <KSharedConfig>
#include <map>
#include <memory>

class PlayListItem;
using Playlist = QList<QPointer<PlayListItem>>;

class PlaySectionsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit PlaySectionsModel(QObject *parent = nullptr);

    enum {
        TitleRole = Qt::UserRole,
        StartTimeRole,
        EndTimeRole,
        DurationRole,
        EndOfSectionModeRole,
        PlayingRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_PROPERTY(PlayListItem *currentEditItem
                   READ currentEditItem
                       WRITE setCurrentEditItem
                           NOTIFY currentEditItemChanged)

    Q_PROPERTY(bool currentEditItemIsEdited
                   MEMBER m_currentEditItemIsEdited
                       READ getCurrentEditItemIsEdited
                           WRITE setCurrentEditItemIsEdited
                               NOTIFY currentEditItemIsEditedChanged)

    Q_INVOKABLE void setPlayingSection(int section);
    Q_INVOKABLE int getPlayingSection();
    Q_INVOKABLE int getNumberOfSections();
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool isEmpty();

    PlayListItem *currentEditItem();
    void setCurrentEditItem(PlayListItem *item);
    void updateCurrentEditItem(PlayListItem &item);

    Q_INVOKABLE void setCurrentEditItemIsEdited(bool value);
    Q_INVOKABLE bool getCurrentEditItemIsEdited();
    Q_INVOKABLE QUrl getSuggestedFileURL();

    Q_INVOKABLE void addSection(QString name, QString startTime, QString endTime, int eosMode);
    Q_INVOKABLE void removeSection(int i);
    Q_INVOKABLE void replaceSection(int i, QString name, QString startTime, QString endTime, int eosMode);
    Q_INVOKABLE void moveSection(int i, int t);
    Q_INVOKABLE void moveSectionUp(int i);
    Q_INVOKABLE void moveSectionDown(int i);
    Q_INVOKABLE QString sectionTitle(int i) const;
    Q_INVOKABLE double sectionStartTime(int i) const;
    Q_INVOKABLE double sectionEndTime(int i) const;
    Q_INVOKABLE int sectionEOSMode(int i) const;

    std::string getSectionsAsFormattedString(size_t charsPerItem = 40) const;

Q_SIGNALS:
    void currentEditItemChanged();
    void currentEditItemIsEditedChanged();
    void playingSectionChanged();

private:
    PlayListItem *m_currentEditItem;
    int m_playingSection = -1;
    bool m_currentEditItemIsEdited = false;
};

class PlayListModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(int playingVideo
                   MEMBER m_playingVideo
                       READ getPlayingVideo
                           WRITE setPlayingVideo
                               NOTIFY playingVideoChanged)

    Q_PROPERTY(bool playListIsEdited
                   MEMBER m_playListEdited
                       READ getPlayListIsEdited
                           WRITE setPlayListIsEdited
                               NOTIFY playListIsEditedChanged)

public:
    explicit PlayListModel(QObject *parent = nullptr);

    enum {
        NameRole = Qt::UserRole,
        TitleRole,
        DurationRole,
        PathRole,
        FolderPathRole,
        PlayingRole,
        StereoRole,
        GridRole,
        EofRole,
        HasDescriptionFileRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Playlist getPlayList() const;
    void setPlayList(const Playlist &playList);

    std::string getListAsFormattedString(int charsPerItem = 40) const;

    Q_INVOKABLE void setPlayListName(QString name);
    Q_INVOKABLE QString getPlayListName() const;

    Q_INVOKABLE void setPlayListPath(QString path);
    Q_INVOKABLE QString getPlayListPath() const;
    Q_INVOKABLE QUrl getPlayListPathAsURL() const;

    Q_INVOKABLE QString getPath(int i);
    Q_INVOKABLE int getPlayListSize() const;
    Q_INVOKABLE QPointer<PlayListItem> getItem(int i);
    Q_INVOKABLE void addItem(PlayListItem *item);
    Q_INVOKABLE void removeItem(int i);
    Q_INVOKABLE void moveItem(int i, int t);
    Q_INVOKABLE void moveItemUp(int i);
    Q_INVOKABLE void moveItemDown(int i);
    Q_INVOKABLE void updateItem(int i);
    Q_INVOKABLE void refreshItem(int i);
    Q_INVOKABLE void setPlayListIsEdited(bool value);
    Q_INVOKABLE bool getPlayListIsEdited();
    Q_INVOKABLE void setPlayingVideo(int playingVideo);
    Q_INVOKABLE int getPlayingVideo() const;
    Q_INVOKABLE void getVideos(QString path);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString filePath(int i) const;
    Q_INVOKABLE QUrl filePathAsURL(int i) const;
    Q_INVOKABLE QString fileName(int i) const;
    Q_INVOKABLE QString fileFolderPath(int i) const;
    Q_INVOKABLE QUrl fileFolderPathAsURL(int i) const;
    Q_INVOKABLE QString mediaFile(int i) const;
    Q_INVOKABLE QString mediaTitle(int i) const;
    Q_INVOKABLE QString duration(int i) const;
    Q_INVOKABLE QString separateAudioFile(int i) const;
    Q_INVOKABLE QString separateOverlayFile(int i) const;
    Q_INVOKABLE int eofMode(int i) const;
    Q_INVOKABLE void setEofMode(int i, int eofMode);
    Q_INVOKABLE int transitionMode(int i) const;
    Q_INVOKABLE int gridToMapOn(int i) const;
    Q_INVOKABLE int stereoVideo(int i) const;
    Q_INVOKABLE int numberOfSections(int i) const;
    Q_INVOKABLE bool hasDescriptionFile(int i) const;
    Q_INVOKABLE double listFileStartTime(int i) const;
    Q_INVOKABLE double listFileEndTime(int i) const;
    Q_INVOKABLE QString listFileStartTimeFormatted(int i) const;
    Q_INVOKABLE QString listFileEndTimeFormatted(int i) const;
    Q_INVOKABLE int listStereoMode(int i) const;
    Q_INVOKABLE void setListStereoMode(int i, int stereoMode);
    Q_INVOKABLE int listGridMode(int i) const;
    Q_INVOKABLE void setListGridMode(int i, int gridMode);
    Q_INVOKABLE QString listTitle(int i) const;
    Q_INVOKABLE void setListTitle(int i, const QString &title);
    Q_INVOKABLE bool useListTitle(int i) const;
    Q_INVOKABLE void setUseListTitle(int i, bool use);
    Q_INVOKABLE bool useListFileStartTime(int i) const;
    Q_INVOKABLE void setUseListFileStartTime(int i, bool use);
    Q_INVOKABLE bool useListFileEndTime(int i) const;
    Q_INVOKABLE void setUseListFileEndTime(int i, bool use);
    Q_INVOKABLE bool useListStereoMode(int i) const;
    Q_INVOKABLE void setUseListStereoMode(int i, bool use);
    Q_INVOKABLE bool useListGridMode(int i) const;
    Q_INVOKABLE void setUseListGridMode(int i, bool use);
    Q_INVOKABLE void setListFileStartTime(int i, double startTime);
    Q_INVOKABLE void setListFileEndTime(int i, double endTime);
    Q_INVOKABLE void setListFileStartTimeFromString(int i, const QString &startTime);
    Q_INVOKABLE void setListFileEndTimeFromString(int i, const QString &endTime);
    Q_INVOKABLE QString makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider);
    Q_INVOKABLE void saveAsJSONPlaylist(const QString &path);

    void asJSON(QJsonObject& obj);

Q_SIGNALS:
    void videoAdded(int index, QString path);
    void playingVideoChanged();
    void playListIsEditedChanged();

private:
    Playlist items() const;
    QString configFolder();
    Playlist m_playList;
    QString m_playListName;
    QString m_playListPath;
    int m_playingVideo = -1;
    int m_defaultEofMode = 2; // Looping
    bool m_playListEdited = false;
    KSharedConfig::Ptr m_config;
};

#endif // PLAYLISTMODEL_H
