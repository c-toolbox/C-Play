/*
 * SPDX-FileCopyrightText:
 * 2023-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QObject>

class MpvObject;
class HttpServerThread;
class SlidesModel;

class PlayerController : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.PlayerController")

    Q_PROPERTY(MpvObject *mpv READ mpv WRITE setMpv NOTIFY mpvChanged)
    Q_PROPERTY(SlidesModel *slides READ slidesModel WRITE setSlidesModel NOTIFY slidesModelChanged)

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() = default;

    void setupConnections();

public Q_SLOTS:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Rewind();
    void Seek(int timeInSec);
    void SetAutoPlay(bool value);
    void SetPosition(double pos);
    void LoadFromAudioTracks(int idx);
    void LoadFromPlaylist(int idx);
    void LoadFromSections(int idx);
    void LoadFromSlides(int idx);
    void SelectFromSlides(int idx);
    void SetSpeed(double factor);
    void SetVolume(int level);
    void SetSyncVolumeVisibilityFading(bool value);
    void FadeVolumeDown();
    void FadeVolumeUp();
    void FadeImageDown();
    void FadeImageUp();
    void SpinPitchUp(bool run);
    void SpinPitchDown(bool run);
    void SpinYawLeft(bool run);
    void SpinYawRight(bool run);
    void SpinRollCW(bool run);
    void SpinRollCCW(bool run);
    void OrientationAndSpinReset();
    void RunSurfaceTransition();
    void SlidePrevious();
    void SlideNext();

    QString returnRelativeOrAbsolutePath(const QString &path);
    QString checkAndCorrectPath(const QString &path);
    QString returnBaseName(const QString &path);

    float backgroundVisibility();
    void setBackgroundVisibility(float value);
    QString backgroundImageFile();
    QUrl backgroundImageFileUrl();
    void setBackgroundImageFile(const QString &path);
    int backgroundGridMode();
    void setBackgroundGridMode(int value);
    int backgroundStereoMode();
    void setBackgroundStereoMode(int value);

    float foregroundVisibility();
    void setForegroundVisibility(float value);
    QString foregroundImageFile();
    QUrl foregroundImageFileUrl();
    void setForegroundImageFile(const QString &path);
    int foregroundGridMode();
    void setForegroundGridMode(int value);
    int foregroundStereoMode();
    void setForegroundStereoMode(int value);

    float backgroundVisibilityOnMaster();
    void setViewModeOnMaster(int value);
    int getViewModeOnMaster();
    void setViewModeOnClients(int value);
    int getViewModeOnClients();

    bool rewindMediaOnEOF();
    void setRewindMediaOnEOF(bool value);

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
    void spinPitchUp(bool run);
    void spinPitchDown(bool run);
    void spinYawLeft(bool run);
    void spinYawRight(bool run);
    void spinRollCCW(bool run);
    void spinRollCW(bool run);
    void orientationAndSpinReset();
    void runSurfaceTransition();
    void mpvChanged();
    void slidesModelChanged();
    void backgroundImageChanged();
    void backgroundVisibilityChanged();
    void foregroundImageChanged();
    void foregroundVisibilityChanged();
    void viewModeOnClientsChanged();
    void rewindMediaOnEOFChanged();

private:
    MpvObject *mpv() const;
    void setMpv(MpvObject *mpv);

    SlidesModel *slidesModel() const;
    void setSlidesModel(SlidesModel* sm);

    void setupHttpServer();

    MpvObject *m_mpv;
    SlidesModel* m_slidesModel;
    HttpServerThread *httpServer;
    QString m_backgroundFile;
    QString m_foregroundFile;
    int m_viewModeOnMaster;
    bool m_rewindMediaOnEOF;
};

#endif // PLAYERCONTROLLER_H
