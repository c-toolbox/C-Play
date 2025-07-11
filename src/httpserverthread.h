/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HTTPSERVERTHREAD_H
#define HTTPSERVERTHREAD_H

#include <QMutex>
#include <QSize>
#include <QThread>
#include <QWaitCondition>
#include <cpp-httplib/httplib.h>

class MpvObject;
class SlidesModel;

class HttpServerThread : public QThread {
    Q_OBJECT

public:
    HttpServerThread(QObject *parent = nullptr);
    ~HttpServerThread();

    void setupHttpServer();
    void setMpv(MpvObject *mpv);
    void setSlidesModel(SlidesModel* sm);

    Q_INVOKABLE void terminate();

Q_SIGNALS:
    void pauseMedia();
    void playMedia();
    void rewindMedia();
    void setPosition(double position);
    void seekInMedia(int timeInSec);
    void setAutoPlay(bool value);
    void setSpeed(double factor);
    void setVolume(int level);
    void setViewMode(int mode);
    void setSyncVolumeVisibilityFading(bool sync);
    void setBackgroundVisibility(float value);
    void setForegroundVisibility(float value);
    void fadeVolumeDown();
    void fadeVolumeUp();
    void fadeImageDown();
    void fadeImageUp();
    void loadFromAudioTracks(int idx);
    void loadFromPlaylist(int idx);
    void loadFromSections(int idx);
    void loadFromSlides(int idx);
    void selectFromSlides(int idx);
    void spinPitchUp(bool run);
    void spinPitchDown(bool run);
    void spinYawLeft(bool run);
    void spinYawRight(bool run);
    void spinRollCCW(bool run);
    void spinRollCW(bool run);
    void orientationAndSpinReset();
    void runSurfaceTransition();
    void slidePrevious();
    void slideNext();

protected:
    void run() override;

private:
    bool stringToInt(std::string str, int &parsedInt);
    bool stringToDouble(std::string str, double &parsedDouble);

    void setPositionFromStr(std::string positionTimeStr);
    void setSpeedFromStr(std::string speedFactorStr);
    void setVolumeFromStr(std::string volumeLevelStr);
    void setViewModeFromStr(std::string volumeLevelStr);
    void setSyncImageFadingFromStr(std::string valueStr);

    const std::string getAudioTracksItems(std::string charsPerItemStr = "", std::string removeLoadedFilePrefix = "");
    const std::string getPlayListItems(std::string charsPerItemStr = "");
    const std::string getSectionsItems(std::string charsPerItemStr = "");
    const std::string getSlideItems(std::string charsPerItemStr = "");

    const std::string getPlaylingItemIndexFromAudioTracks();
    const std::string getPlaylingItemIndexFromPlaylist();
    const std::string getPlaylingItemIndexFromSections();
    const std::string getPlaylingItemIndexFromSlides();

    const std::string LoadIndexFromAudioTracks(std::string indexStr);
    const std::string LoadIndexFromPlaylist(std::string indexStr);
    const std::string LoadIndexFromSections(std::string indexStr);
    const std::string LoadIndexFromSlides(std::string indexStr);
    const std::string SelectIndexFromSlides(std::string indexStr);

    MpvObject *m_mpv;
    SlidesModel* m_slidesModel;
    httplib::Server svr;
    bool runServer;
    int portServer;

    QMutex mutex;
    QWaitCondition condition;
    bool abort = false;
};

#endif // HTTPSERVERTHREAD_H
