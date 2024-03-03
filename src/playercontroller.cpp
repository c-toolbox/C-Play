#include "playercontroller.h"
#include "_debug.h"
#include "application.h"
#include "mpvobject.h"
#include "httpserverthread.h"
#include "generalsettings.h"
#include "playbacksettings.h"

#include <QDir>
#include <QFileInfo>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , httpServer(new HttpServerThread(this))
    , m_backgroundFile("")
    , m_viewModeOnMaster(0)
{
    connect(this, &PlayerController::mpvChanged,
            this, &PlayerController::setupConnections);

    setupHttpServer();

    setBackgroundImageFile(PlaybackSettings::imageToLoadAsBackground());
    setBackgroundGridMode(PlaybackSettings::gridToMapOnForBackground());
    setBackgroundStereoMode(PlaybackSettings::stereoModeForBackground());
}

void PlayerController::setupConnections()
{
    if (!m_mpv) {
        return;
    }

    connect(m_mpv, &MpvObject::fileLoaded, this, &PlayerController::backgroundVisibilityChanged);
    connect(m_mpv, &MpvObject::visibilityChanged, this, &PlayerController::backgroundVisibilityChanged);
    connect(this, &PlayerController::backgroundImageChanged, this, &PlayerController::backgroundVisibilityChanged);

    emit backgroundVisibilityChanged();
}

void PlayerController::setupHttpServer()
{
    httpServer->setupHttpServer();

    connect(httpServer, &HttpServerThread::finished, httpServer, &QObject::deleteLater);
    connect(httpServer, &HttpServerThread::pauseMedia, this, &PlayerController::Pause);
    connect(httpServer, &HttpServerThread::playMedia, this, &PlayerController::Play);
    connect(httpServer, &HttpServerThread::rewindMedia, this, &PlayerController::Rewind);
    connect(httpServer, &HttpServerThread::setPosition, this, &PlayerController::SetPosition);
    connect(httpServer, &HttpServerThread::setVolume, this, &PlayerController::SetVolume);
    connect(httpServer, &HttpServerThread::fadeVolumeDown, this, &PlayerController::FadeVolumeDown);
    connect(httpServer, &HttpServerThread::fadeVolumeUp, this, &PlayerController::FadeVolumeUp);
    connect(httpServer, &HttpServerThread::fadeImageDown, this, &PlayerController::FadeImageDown);
    connect(httpServer, &HttpServerThread::fadeImageUp, this, &PlayerController::FadeImageUp);
    connect(httpServer, &HttpServerThread::loadFromAudioTracks, this, &PlayerController::LoadFromAudioTracks);
    connect(httpServer, &HttpServerThread::loadFromPlaylist, this, &PlayerController::LoadFromPlaylist);
    connect(httpServer, &HttpServerThread::loadFromSections, this, &PlayerController::LoadFromSections);
    connect(httpServer, &HttpServerThread::spinPitchUp, this, &PlayerController::SpinPitchUp);
    connect(httpServer, &HttpServerThread::spinPitchDown, this, &PlayerController::SpinPitchDown);
    connect(httpServer, &HttpServerThread::spinYawLeft, this, &PlayerController::SpinYawLeft);
    connect(httpServer, &HttpServerThread::spinYawRight, this, &PlayerController::SpinYawRight);
    connect(httpServer, &HttpServerThread::spinRollCCW, this, &PlayerController::SpinRollCCW);
    connect(httpServer, &HttpServerThread::spinRollCW, this, &PlayerController::SpinRollCW);
    connect(httpServer, &HttpServerThread::orientationAndSpinReset, this, &PlayerController::OrientationAndSpinReset);
    connect(httpServer, &HttpServerThread::runSurfaceTransistion, this, &PlayerController::RunSurfaceTransistion);

    httpServer->start();
}

void PlayerController::Next()
{
    Q_EMIT next();
}

void PlayerController::Previous()
{
    Q_EMIT previous();
}

void PlayerController::Pause()
{
    Q_EMIT pause();
}

void PlayerController::PlayPause()
{
    Q_EMIT playpause();
}

void PlayerController::Stop()
{
    Q_EMIT stop();
}

void PlayerController::Play()
{
    Q_EMIT play();
}

void PlayerController::Rewind()
{
    if (m_mpv) {
        m_mpv->performRewind();
    }
}

void PlayerController::Seek(qlonglong offset)
{
    Q_EMIT seek(offset/1000/1000);
}

void PlayerController::LoadFromAudioTracks(int idx)
{
    if (m_mpv) {
        m_mpv->setAudioId(idx+1); //AudioID starts from 1.
    }
}

void PlayerController::LoadFromPlaylist(int idx)
{
    Q_EMIT loadFromPlaylist(idx);
}

void PlayerController::LoadFromSections(int idx)
{
    Q_EMIT loadFromSections(idx);
}

void PlayerController::SetPosition(double pos)
{
    if (m_mpv) {
        m_mpv->setPosition(pos);
    }
}

void PlayerController::SetVolume(int level)
{
    if (m_mpv) {
        m_mpv->setVolume(level);
    }
}

void PlayerController::FadeVolumeDown()
{
    if (m_mpv) {
        Q_EMIT m_mpv->fadeVolumeDown();
    }
}

void PlayerController::FadeVolumeUp()
{
    if (m_mpv) {
        Q_EMIT m_mpv->fadeVolumeUp();
    }
}

void PlayerController::FadeImageDown()
{
    if (m_mpv) {
        Q_EMIT m_mpv->fadeImageDown();
    }
}

void PlayerController::FadeImageUp()
{
    if (m_mpv) {
        Q_EMIT m_mpv->fadeImageUp();
    }
}

void PlayerController::SpinPitchUp()
{
    Q_EMIT spinPitchUp();
}

void PlayerController::SpinPitchDown()
{
    Q_EMIT spinPitchDown();
}

void PlayerController::SpinYawLeft()
{
    Q_EMIT spinYawLeft();
}

void PlayerController::SpinYawRight()
{
    Q_EMIT spinYawRight();
}

void PlayerController::SpinRollCW()
{
    Q_EMIT spinRollCW();
}

void PlayerController::SpinRollCCW()
{
    Q_EMIT spinRollCCW();
}

void PlayerController::OrientationAndSpinReset()
{
    Q_EMIT orientationAndSpinReset();
}

void PlayerController::RunSurfaceTransistion()
{
    Q_EMIT runSurfaceTransistion();
}

QString PlayerController::returnRelativeOrAbsolutePath(const QString& path)
{
    QString filePath = path;
    filePath.replace("file:///", "");
    QFileInfo fileInfo(filePath);

    QStringList pathsToConsider;
    pathsToConsider.append(GeneralSettings::cPlayMediaLocation());
    pathsToConsider.append(GeneralSettings::univiewVideoLocation());
    pathsToConsider.append(GeneralSettings::cPlayFileLocation());

    // Assuming filePath is absolute
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            return foundDir.relativeFilePath(filePath);
        }
    }
    return filePath;
}

QString PlayerController::checkAndCorrectPath(const QString& path) {
    QString filePath = path;
    filePath.replace("file:///", "");
    QFileInfo fileInfo(filePath);

    if (fileInfo.exists())
        return filePath;
    else if (fileInfo.isRelative()) { // Go through search list in order
        QStringList searchPaths;
        searchPaths.append(GeneralSettings::cPlayMediaLocation());
        searchPaths.append(GeneralSettings::cPlayFileLocation());
        searchPaths.append(GeneralSettings::univiewVideoLocation());

        for (int i = 0; i < searchPaths.size(); i++) {
            QString newFilePath = QDir::cleanPath(searchPaths[i] + QDir::separator() + filePath);
            QFileInfo newFilePathInfo(newFilePath);
            if (newFilePathInfo.exists())
                return newFilePath;
        }
    }
    return "";
}

float PlayerController::backgroundVisibility()
{
    if (getViewModeOnMaster() == 1) {
        return 0.f;
    }
    else if (getViewModeOnMaster() == 2) {
        return 1.f;
    }

    if (SyncHelper::instance().variables.alphaBg > 0.f) {
        if (SyncHelper::instance().variables.loadedFile.empty()) {
            return SyncHelper::instance().variables.alphaBg;
        }
        else {
            return 1.f - SyncHelper::instance().variables.alpha;
        }
    }

    return 0.f;
}

void PlayerController::setBackgroundVisibility(float value)
{
    SyncHelper::instance().variables.alphaBg = value;

    emit backgroundVisibilityChanged();
}

QString PlayerController::backgroundImageFile()
{
    return m_backgroundFile;
}

QUrl PlayerController::backgroundImageFileUrl()
{
    if (SyncHelper::instance().variables.bgImageFile.empty()) {
        return QUrl();
    }

    return QUrl::fromLocalFile(QString::fromStdString(SyncHelper::instance().variables.bgImageFile));
}

void PlayerController::setBackgroundImageFile(const QString& path)
{
    if (path.isEmpty()) {
        m_backgroundFile = "";
        SyncHelper::instance().variables.bgImageFile = "";
        setBackgroundVisibility(0.f);
        emit backgroundImageChanged();
        return;
    }

    QString filePath = path;
    filePath.replace("file:///", "");
    QFileInfo fileInfo(filePath);

    QString absolutePath;
    if (fileInfo.exists()) { //isAbsolute
        m_backgroundFile = returnRelativeOrAbsolutePath(path);
        absolutePath = path;
    }
    else if (fileInfo.isRelative()) { //isRelative
        QString bgFilePath = checkAndCorrectPath(path);
        if (bgFilePath.isEmpty()) {
            return;
        }
        m_backgroundFile = path;
        absolutePath = bgFilePath;
    }
    else
        return;

    absolutePath.replace("file:///", "");
    std::string str = absolutePath.toStdString();
    if (SyncHelper::instance().variables.bgImageFile != str) {
        SyncHelper::instance().variables.bgImageFile = str;
        setBackgroundVisibility(1.f);
    }

    emit backgroundImageChanged();
}

int PlayerController::backgroundGridMode()
{
    return SyncHelper::instance().variables.gridToMapOnBg;
}

void PlayerController::setBackgroundGridMode(int value)
{
    SyncHelper::instance().variables.gridToMapOnBg = value;
}

int PlayerController::backgroundStereoMode()
{
    return SyncHelper::instance().variables.stereoscopicModeBg;
}

void PlayerController::setBackgroundStereoMode(int value)
{
    SyncHelper::instance().variables.stereoscopicModeBg = value;
}

void PlayerController::setViewModeOnMaster(int value)
{
    //0 = Same as nodes
    //1 = Show media always
    //2 = Show background always
    m_viewModeOnMaster = value;

    emit backgroundVisibilityChanged();
}

int PlayerController::getViewModeOnMaster()
{
    return m_viewModeOnMaster;
}

MpvObject *PlayerController::mpv() const
{
    return m_mpv;
}

void PlayerController::setMpv(MpvObject *mpv)
{
    if (m_mpv == mpv) {
        return;
    }
    m_mpv = mpv;
    httpServer->setMpv(mpv);
    Q_EMIT mpvChanged();
}
