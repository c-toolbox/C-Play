/*
 * SPDX-FileCopyrightText: 2021 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MEDIAPLAYER2PLAYER_H
#define MEDIAPLAYER2PLAYER_H

#include <QDBusAbstractAdaptor>

class MpvObject;
class QDBusObjectPath;
class HttpServerThread;

class MediaPlayer2Player : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")

    Q_PROPERTY(MpvObject *mpv READ mpv WRITE setMpv NOTIFY mpvChanged)

public:
    explicit MediaPlayer2Player(QObject *parent = nullptr);
    ~MediaPlayer2Player() = default;

    void setupConnections();
    void propertiesChanged(const QString &property, const QVariant &value);

public Q_SLOTS:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(double pos);
    void LoadFromPlaylist(int idx);
    void LoadFromSections(int idx);
    void SetVolume(int level);
    void FadeVolumeDown();
    void FadeVolumeUp();
    void FadeImageDown();
    void FadeImageUp();
    void SpinPitchUp();
    void SpinPitchDown();
    void SpinYawLeft();
    void SpinYawRight();
    void SpinRollCW();
    void SpinRollCCW();
    void OrientationAndSpinReset();
    void RunSurfaceTransistion();

Q_SIGNALS:
    void next();
    void previous();
    void pause();
    void playpause();
    void stop();
    void play();
    void seek(int offset);
    void loadFromPlaylist(int idx);
    void loadFromSections(int idx);
    void spinPitchUp();
    void spinPitchDown();
    void spinYawLeft();
    void spinYawRight();
    void spinRollCCW();
    void spinRollCW();
    void orientationAndSpinReset();
    void runSurfaceTransistion();
    void mpvChanged();

private:
    MpvObject *mpv() const;
    void setMpv(MpvObject *mpv);

    void setupHttpServer();

    MpvObject *m_mpv;
    HttpServerThread* httpServer;
};

#endif // MEDIAPLAYER2PLAYER_H
