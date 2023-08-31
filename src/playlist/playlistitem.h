/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QObject>

class PlayListItemData
{
public:
    
    QString filePath() const;
    QString fileName() const;
    QString fileFolderPath() const;
    QString mediaFile() const;
    QString mediaTitle() const;
    QString duration() const;
    QString separateOverlayFile() const;
    QString separateAudioFile() const;
    double startTime() const;
    double endTime() const;
    int loopMode() const;
    int transitionMode() const;
    int gridToMapOn() const;
    int stereoVideo() const;
    bool isPlaying() const;
    int index() const;

    QString m_filePath;
    QString m_fileName;
    QString m_fileFolderPath;
    QString m_mediaFile;
    QString m_mediaTitle;
    QString m_duration;
    QString m_separateOverlayFile{ "" };
    QString m_separateAudioFile{ "" };
    double m_startTime{ 0.0 };
    double m_endTime{ 0.0 };
    int m_loopMode{ 2 };
    int m_transitionMode{ 0 };
    int m_gridToMapOn{ -1 };
    int m_stereoVideo{ -1 };
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

    Q_INVOKABLE QString duration() const;
    Q_INVOKABLE void setDuration(const QString &duration);

    Q_INVOKABLE QString separateOverlayFile() const;
    Q_INVOKABLE void setSeparateOverlayFile(const QString& audioFile);

    Q_INVOKABLE QString separateAudioFile() const;
    Q_INVOKABLE void setSeparateAudioFile(const QString& audioFile);

    Q_INVOKABLE double startTime() const;
    Q_INVOKABLE void setStartTime(double startTime);

    Q_INVOKABLE double endTime() const;
    Q_INVOKABLE void setEndTime(double endTime);

    // 0 = Continue
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

    Q_INVOKABLE int index() const;
    Q_INVOKABLE void setIndex(int index);

    Q_INVOKABLE void saveAsJSONPlayFile(const QString& path) const;

    PlayListItemData data() const;
    void setData(PlayListItemData d);
    void updateToNewFile(const QString& path);

private:
    PlayListItemData m_data;
};

#endif // PLAYLISTITEM_H
