/*
 * SPDX-FileCopyrightText: 
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com> 
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYLISTMODEL_H
#define PLAYLISTMODEL_H

#include <QAbstractTableModel>
#include <KSharedConfig>
#include <map>
#include <memory>

class PlayListItem;

using Playlist = QList<QPointer<PlayListItem>>;

class PlaySectionsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit PlaySectionsModel(QObject* parent = nullptr);

    enum {
        TitleRole = Qt::UserRole,
        StartTimeRole,
        EndTimeRole,
        DurationRole,
        EndOfSectionModeRole,
        PlayingRole
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_PROPERTY(PlayListItem* currentEditItem
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

    PlayListItem* currentEditItem();
    void setCurrentEditItem(PlayListItem* item);
    void updateCurrentEditItem(PlayListItem& item);

    Q_INVOKABLE void setCurrentEditItemIsEdited(bool value);
    Q_INVOKABLE bool getCurrentEditItemIsEdited();
    Q_INVOKABLE QUrl getSuggestedFileURL();

    Q_INVOKABLE void addSection(QString name, QString startTime, QString endTime, int eosMode);
    Q_INVOKABLE void removeSection(int i);
    Q_INVOKABLE void replaceSection(int i, QString name, QString startTime, QString endTime, int eosMode);
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
    PlayListItem* m_currentEditItem;
    int m_playingSection = -1;
    int m_currentEditItemIsEdited = false;
};

class PlayListModel : public QAbstractListModel
{
    Q_OBJECT
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
    void setPlayList(const Playlist& playList);

    std::string getListAsFormattedString(int charsPerItem = 40) const;

    Q_INVOKABLE void setPlayListName(QString name);
    Q_INVOKABLE QString getPlayListName() const;

    Q_INVOKABLE void setPlayListPath(QString path);
    Q_INVOKABLE QString getPlayListPath() const;
    Q_INVOKABLE QUrl getPlayListPathAsURL() const;

    Q_INVOKABLE QString getPath(int i);
    Q_INVOKABLE int getPlayListSize() const;
    Q_INVOKABLE QPointer<PlayListItem> getItem(int i);
    Q_INVOKABLE void addItem(PlayListItem* item);
    Q_INVOKABLE void removeItem(int i);
    Q_INVOKABLE void moveItemUp(int i);
    Q_INVOKABLE void moveItemDown(int i);
    Q_INVOKABLE void updateItem(int i);
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
    Q_INVOKABLE QString makePathRelativeTo(const QString& filePath, const QStringList& pathsToConsider);
    Q_INVOKABLE void asJSON(QJsonObject& obj);
    Q_INVOKABLE void saveAsJSONPlaylist(const QString& path);

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
