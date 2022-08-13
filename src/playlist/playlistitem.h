/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QObject>

class PlayListItem : public QObject
{
    Q_OBJECT
public:
    explicit PlayListItem(const QString &path, int i = 0, QObject *parent = nullptr);

    Q_INVOKABLE QString mediaTitle() const;
    void setMediaTitle(const QString &title);

    Q_INVOKABLE QString filePath() const;
    void setFilePath(const QString &filePath);

    Q_INVOKABLE QString fileName() const;
    void setFileName(const QString &fileName);

    Q_INVOKABLE QString folderPath() const;
    void setFolderPath(const QString &folderPath);

    Q_INVOKABLE QString duration() const;
    void setDuration(const QString &duration);

    Q_INVOKABLE QString separateAudioFile() const;
    void setSeparateAudioFile(const QString& audioFile);

    Q_INVOKABLE double startTime() const;
    void setStartTime(double startTime);

    Q_INVOKABLE double endTime() const;
    void setEndTime(double endTime);

    Q_INVOKABLE int loopMode() const;
    void setLoopMode(int loopMode);

    Q_INVOKABLE int transitionMode() const;
    void setTransitionMode(int transitionMode);

    Q_INVOKABLE int gridToMapOn() const;
    void setGridToMapOn(int gridToMapOn);

    Q_INVOKABLE int stereoVideo() const;
    void setStereoVideo(int stereoVideo);

    Q_INVOKABLE bool isPlaying() const;
    void setIsPlaying(bool isPlaying);

    Q_INVOKABLE int index() const;
    void setIndex(int index);

private:
    QString m_mediaTitle;
    QString m_filePath;
    QString m_fileName;
    QString m_folderPath;
    QString m_duration;
    QString m_separateAudioFile{ "" };
    double m_startTime{ 0.0 };
    double m_endTime{ 0.0 };
    int m_loopMode{ 0 };
    int m_transitionMode{ 1 };
    int m_gridToMapOn{ -1 };
    int m_stereoVideo{ -1 };
    bool m_isHovered {false};
    bool m_isPlaying {false};
    int m_index {-1};
};

#endif // PLAYLISTITEM_H
