/*
 * SPDX-FileCopyrightText: 
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com> 
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QString>
#include <QObject>

class PlayListItemData
{
public:
    struct Section {
        Section(QString name, double start, double end, int eos) 
            : title(name), startTime(start), endTime(end), eosMode(eos), isPlaying(false) {}
        QString title;
        double startTime;
        double endTime;
        // 0 = Pause
        // 1 = Fade out (then pause)
        // 2 = Continue
        // 3 = Next
        // 4 = Loop
        int eosMode; //End of section
        bool isPlaying;
    };
    
    QString filePath() const;
    QString fileName() const;
    QString fileFolderPath() const;
    QString mediaFile() const;
    QString mediaTitle() const;
    double duration() const;
    QString separateOverlayFile() const;
    QString separateAudioFile() const;
    int loopMode() const;
    int transitionMode() const;
    int gridToMapOn() const;
    int stereoVideo() const;
    QList<PlayListItemData::Section> sections() const;

    bool isPlaying() const;
    int index() const;

    QString m_filePath;
    QString m_fileName;
    QString m_fileFolderPath;
    QString m_mediaFile;
    QString m_mediaTitle;
    double m_duration;
    QString m_separateOverlayFile{ "" };
    QString m_separateAudioFile{ "" };
    int m_loopMode{ 2 };
    int m_transitionMode{ 0 };
    int m_gridToMapOn{ -1 };
    int m_stereoVideo{ -1 };
    QList<Section> m_sections;

    bool m_isHovered{ false };
    bool m_isPlaying{ false };
    int m_index{ -1 };
};

class PlayListItem : public QObject
{
    Q_OBJECT
public:
    explicit PlayListItem(const QString &path, int i = 0, QObject *parent = nullptr);
    PlayListItem(const PlayListItem&);
    PlayListItem(const PlayListItemData&);

    Q_INVOKABLE PlayListItem& operator =(const PlayListItem&);

    Q_INVOKABLE QString filePath() const;
    Q_INVOKABLE void setFilePath(const QString &filePath);

    Q_INVOKABLE QString fileName() const;
    Q_INVOKABLE void setFileName(const QString &fileName);

    Q_INVOKABLE QUrl fileFolderPath() const;
    Q_INVOKABLE void setFileFolderPath(const QString &folderPath);

    Q_INVOKABLE QString mediaFile() const;
    Q_INVOKABLE void setMediaFile(const QString& title);

    Q_INVOKABLE QString mediaTitle() const;
    Q_INVOKABLE void setMediaTitle(const QString& title);

    Q_INVOKABLE QString durationFormatted() const;
    Q_INVOKABLE double duration() const;
    Q_INVOKABLE void setDuration(double duration);

    Q_INVOKABLE QString separateOverlayFile() const;
    Q_INVOKABLE void setSeparateOverlayFile(const QString& audioFile);

    Q_INVOKABLE QString separateAudioFile() const;
    Q_INVOKABLE void setSeparateAudioFile(const QString& audioFile);

    // 0 = Continue to next
    // 1 = Pause
    // 2 = Loop
    Q_INVOKABLE int loopMode() const;
    Q_INVOKABLE void setLoopMode(int loopMode);

    // 0 = Instant
    // 1 = Fade
    Q_INVOKABLE int transitionMode() const;
    Q_INVOKABLE void setTransitionMode(int transitionMode);

    // 0 = Pre-split
    // 1 = Plane
    // 2 = Dome
    // 3 = Sphere EQR
    // 4 = Sphere EAC
    Q_INVOKABLE int gridToMapOn() const;
    Q_INVOKABLE void setGridToMapOn(int gridToMapOn);

    // 0 = 2D (mono)
    // 1 = 3D (side-by-side)
    // 2 = 3D (top-bottom)
    // 3 = 3D (top-bottom-flip)
    Q_INVOKABLE int stereoVideo() const;
    Q_INVOKABLE void setStereoVideo(int stereoVideo);

    Q_INVOKABLE bool isPlaying() const;
    Q_INVOKABLE void setIsPlaying(bool isPlaying);

    Q_INVOKABLE void addSection(QString title, double startTime, double endTime, int eosMode);
    Q_INVOKABLE void addSection(QString title, QString startTime, QString endTime, int eosMode);
    Q_INVOKABLE void removeSection(int i);
    Q_INVOKABLE void moveSection(int from, int to);
    Q_INVOKABLE QString sectionTitle(int i) const;
    Q_INVOKABLE double sectionStartTime(int i) const;
    Q_INVOKABLE double sectionEndTime(int i) const;
    Q_INVOKABLE int sectionEOSMode(int i) const;
    Q_INVOKABLE int numberOfSections() const;
    Q_INVOKABLE const PlayListItemData::Section& getSection(int i) const;
    Q_INVOKABLE QList<PlayListItemData::Section> sections();

    Q_INVOKABLE bool isSectionPlaying(int index) const;
    Q_INVOKABLE void setIsSectionPlaying(int index, bool isPlaying);

    Q_INVOKABLE int index() const;
    Q_INVOKABLE void setIndex(int index);

    Q_INVOKABLE QString makePathRelativeTo(const QString& filePath, const QStringList& pathsToConsider) const;
    Q_INVOKABLE void asJSON(QJsonObject& obj);
    Q_INVOKABLE void saveAsJSONPlayFile(const QString& path) const;

    PlayListItemData data() const;
    void setData(PlayListItemData d);
    void updateToNewFile(const QString& path);
    void loadDetailsFromDisk();

private:
    void loadJSONPlayfile();
    void loadUniviewFDV();

    PlayListItemData m_data;

    QString checkAndCorrectPath(const QString& filePath, const QStringList& searchPaths);
};

#endif // PLAYLISTITEM_H
