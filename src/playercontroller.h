#ifndef PLAYERCONTROLLER_H
#define PLAYERCONTROLLER_H

#include <QObject>

class MpvObject;
class HttpServerThread;

class PlayerController : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.PlayerController")

    Q_PROPERTY(MpvObject *mpv READ mpv WRITE setMpv NOTIFY mpvChanged)

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
    void Seek(qlonglong offset);
    void SetPosition(double pos);
    void LoadFromAudioTracks(int idx);
    void LoadFromPlaylist(int idx);
    void LoadFromSections(int idx);
    void SetVolume(int level);
    void SetSyncImageVideoFading(bool value);
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

    QString returnRelativeOrAbsolutePath(const QString& path);
    QString checkAndCorrectPath(const QString& path);

    float backgroundVisibility();
    void setBackgroundVisibility(float value);
    QString backgroundImageFile();
    QUrl backgroundImageFileUrl();
    void setBackgroundImageFile(const QString& path);
    int backgroundGridMode();
    void setBackgroundGridMode(int value);
    int backgroundStereoMode();
    void setBackgroundStereoMode(int value);

    void setViewModeOnMaster(int value);
    int getViewModeOnMaster();
    void setViewModeOnClients(int value);
    int getViewModeOnClients();

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
    void backgroundImageChanged();
    void backgroundVisibilityChanged();
    void viewModeOnClientsChanged();

private:
    MpvObject *mpv() const;
    void setMpv(MpvObject *mpv);

    void setupHttpServer();

    MpvObject *m_mpv;
    HttpServerThread* httpServer;
    QString m_backgroundFile;
    int m_viewModeOnMaster;
};

#endif // PLAYERCONTROLLER_H
