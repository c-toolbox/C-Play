#include "playercontroller.h"
#include "_debug.h"
#include "application.h"
#include "mpvobject.h"
#include "httpserverthread.h"
#include "generalsettings.h"
#include "playbacksettings.h"

#include <QDir>
#include <QFileInfo>

#pragma warning(disable : 4996)

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , httpServer(new HttpServerThread(this))
    , m_backgroundFile("")
    , m_foregroundFile("")
    , m_viewModeOnMaster(0)
{
    connect(this, &PlayerController::mpvChanged,
            this, &PlayerController::setupConnections);

    setupHttpServer();

    setBackgroundImageFile(PlaybackSettings::imageToLoadAsBackground());
    setBackgroundGridMode(PlaybackSettings::gridToMapOnForBackground());
    setBackgroundStereoMode(PlaybackSettings::stereoModeForBackground());

    setForegroundImageFile(PlaybackSettings::imageToLoadAsForeground());
    setForegroundGridMode(PlaybackSettings::gridToMapOnForForeground());
    setForegroundStereoMode(PlaybackSettings::stereoModeForForeground());
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
    connect(httpServer, &HttpServerThread::setAutoPlay, this, &PlayerController::SetAutoPlay);
    connect(httpServer, &HttpServerThread::setPosition, this, &PlayerController::SetPosition);
    connect(httpServer, &HttpServerThread::setVolume, this, &PlayerController::SetVolume);
    connect(httpServer, &HttpServerThread::setViewMode, this, &PlayerController::setViewModeOnClients);
    connect(httpServer, &HttpServerThread::setBackgroundVisibility, this, &PlayerController::setBackgroundVisibility);
    connect(httpServer, &HttpServerThread::setForegroundVisibility, this, &PlayerController::setForegroundVisibility);
    connect(httpServer, &HttpServerThread::setSyncImageVideoFading, this, &PlayerController::SetSyncImageVideoFading);
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
    connect(httpServer, &HttpServerThread::runSurfaceTransition, this, &PlayerController::RunSurfaceTransition);

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

void PlayerController::SetAutoPlay(bool value)
{
    if (m_mpv) {
        m_mpv->setAutoPlay(value);
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

void PlayerController::SetSyncImageVideoFading(bool value)
{
    if (m_mpv) {
        m_mpv->setSyncImageVideoFading(value);
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

void PlayerController::SpinPitchUp(bool run)
{
    Q_EMIT spinPitchUp(run);
}

void PlayerController::SpinPitchDown(bool run)
{
    Q_EMIT spinPitchDown(run);
}

void PlayerController::SpinYawLeft(bool run)
{
    Q_EMIT spinYawLeft(run);
}

void PlayerController::SpinYawRight(bool run)
{
    Q_EMIT spinYawRight(run);
}

void PlayerController::SpinRollCW(bool run)
{
    Q_EMIT spinRollCW(run);
}

void PlayerController::SpinRollCCW(bool run)
{
    Q_EMIT spinRollCCW(run);
}

void PlayerController::OrientationAndSpinReset()
{
    Q_EMIT orientationAndSpinReset();
}

void PlayerController::RunSurfaceTransition()
{
    Q_EMIT runSurfaceTransition();
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
    return SyncHelper::instance().variables.alphaBg;
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
        SyncHelper::instance().variables.bgImageFileDirty = true;
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
        SyncHelper::instance().variables.bgImageFileDirty = true;
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

float PlayerController::foregroundVisibility()
{
    return SyncHelper::instance().variables.alphaFg;
}

void PlayerController::setForegroundVisibility(float value)
{
    SyncHelper::instance().variables.alphaFg = value;

    emit foregroundVisibilityChanged();
}

QString PlayerController::foregroundImageFile()
{
    return m_foregroundFile;
}

QUrl PlayerController::foregroundImageFileUrl()
{
    if (SyncHelper::instance().variables.fgImageFile.empty()) {
        return QUrl();
    }

    return QUrl::fromLocalFile(QString::fromStdString(SyncHelper::instance().variables.fgImageFile));
}

void PlayerController::setForegroundImageFile(const QString& path)
{
    if (path.isEmpty()) {
        m_foregroundFile = "";
        SyncHelper::instance().variables.fgImageFile = "";
        SyncHelper::instance().variables.fgImageFileDirty = true;
        emit foregroundImageChanged();
        return;
    }

    QString filePath = path;
    filePath.replace("file:///", "");
    QFileInfo fileInfo(filePath);

    QString absolutePath;
    if (fileInfo.exists()) { //isAbsolute
        m_foregroundFile = returnRelativeOrAbsolutePath(path);
        absolutePath = path;
    }
    else if (fileInfo.isRelative()) { //isRelative
        QString fgFilePath = checkAndCorrectPath(path);
        if (fgFilePath.isEmpty()) {
            return;
        }
        m_foregroundFile = path;
        absolutePath = fgFilePath;
    }
    else
        return;

    absolutePath.replace("file:///", "");
    std::string str = absolutePath.toStdString();
    if (SyncHelper::instance().variables.fgImageFile != str) {
        SyncHelper::instance().variables.fgImageFile = str;
        SyncHelper::instance().variables.fgImageFileDirty = true;
    }

    emit foregroundImageChanged();
}

int PlayerController::foregroundGridMode()
{
    return SyncHelper::instance().variables.gridToMapOnFg;
}

void PlayerController::setForegroundGridMode(int value)
{
    SyncHelper::instance().variables.gridToMapOnFg = value;
}

int PlayerController::foregroundStereoMode()
{
    return SyncHelper::instance().variables.stereoscopicModeFg;
}

void PlayerController::setForegroundStereoMode(int value)
{
    SyncHelper::instance().variables.stereoscopicModeFg = value;
}

float PlayerController::backgroundVisibilityOnMaster()
{
    if (getViewModeOnMaster() == 1) {
        return 0.f;
    }
    else if (getViewModeOnMaster() == 2) {
        return 1.f;
    }

    if (backgroundVisibility() > 0.f) {
        if (SyncHelper::instance().variables.loadedFile.empty()) {
            return backgroundVisibility();
        }
        else {
            return 1.f - SyncHelper::instance().variables.alpha;
        }
    }

    return 0.f;
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

void PlayerController::setViewModeOnClients(int value)
{
    //0 = Auto 2D/3D switch
    //1 = Force 2D for all
    SyncHelper::instance().variables.viewMode = value;

    emit viewModeOnClientsChanged();
}

int PlayerController::getViewModeOnClients()
{
    return SyncHelper::instance().variables.viewMode;
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
