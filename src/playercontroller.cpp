#include "playercontroller.h"
#include "_debug.h"
#include "application.h"
#include "httpserverthread.h"
#include "imagesettings.h"
#include "locationsettings.h"
#include "mpvobject.h"
#include "playbacksettings.h"
#include "slidesmodel.h"
#include "presentationsettings.h"
#include "userinterfacesettings.h"

#include <QDir>
#include <QFileInfo>

#pragma warning(disable : 4996)

PlayerController::PlayerController(QObject *parent)
    : QObject(parent), 
    m_mpv(nullptr),
    m_slidesModel(nullptr),
    httpServer(new HttpServerThread(this)),
    m_backgroundFile(QStringLiteral("")),
    m_foregroundFile(QStringLiteral("")),
    m_viewModeOnMaster(0)
{
    connect(this, &PlayerController::mpvChanged,
            this, &PlayerController::setupConnections);

    setupHttpServer();

    setBackgroundImageFile(ImageSettings::imageToLoadAsBackground());
    setBackgroundGridMode(ImageSettings::gridToMapOnForBackground());
    setBackgroundStereoMode(ImageSettings::stereoModeForBackground());

    setForegroundImageFile(ImageSettings::imageToLoadAsForeground());
    setForegroundGridMode(ImageSettings::gridToMapOnForForeground());
    setForegroundStereoMode(ImageSettings::stereoModeForForeground());

    setRewindMediaOnEOF(PlaybackSettings::rewindOnEOFwhenPause());

    setNodeWindowsOnTop(UserInterfaceSettings::windowOnTopAtStartup());
}

void PlayerController::setupConnections() {
    if (!m_mpv) {
        return;
    }

    connect(m_mpv, &MpvObject::fileLoaded, this, &PlayerController::backgroundVisibilityChanged);
    connect(m_mpv, &MpvObject::visibilityChanged, this, &PlayerController::backgroundVisibilityChanged);
    connect(this, &PlayerController::backgroundImageChanged, this, &PlayerController::backgroundVisibilityChanged);

    Q_EMIT backgroundVisibilityChanged();
}

void PlayerController::setupHttpServer() {
    httpServer->setupHttpServer();

    connect(httpServer, &HttpServerThread::finished, httpServer, &QObject::deleteLater);
    connect(httpServer, &HttpServerThread::pauseMedia, this, &PlayerController::Pause);
    connect(httpServer, &HttpServerThread::playMedia, this, &PlayerController::Play);
    connect(httpServer, &HttpServerThread::rewindMedia, this, &PlayerController::Rewind);
    connect(httpServer, &HttpServerThread::seekInMedia, this, &PlayerController::Seek);
    connect(httpServer, &HttpServerThread::setAutoPlay, this, &PlayerController::SetAutoPlay);
    connect(httpServer, &HttpServerThread::setPosition, this, &PlayerController::SetPosition);
    connect(httpServer, &HttpServerThread::setSpeed, this, &PlayerController::SetSpeed);
    connect(httpServer, &HttpServerThread::setVolume, this, &PlayerController::SetVolume);
    connect(httpServer, &HttpServerThread::setViewMode, this, &PlayerController::setViewModeOnClients);
    connect(httpServer, &HttpServerThread::setBackgroundVisibility, this, &PlayerController::setBackgroundVisibility);
    connect(httpServer, &HttpServerThread::setForegroundVisibility, this, &PlayerController::setForegroundVisibility);
    connect(httpServer, &HttpServerThread::setSyncVolumeVisibilityFading, this, &PlayerController::SetSyncVolumeVisibilityFading);
    connect(httpServer, &HttpServerThread::fadeVolumeDown, this, &PlayerController::FadeVolumeDown);
    connect(httpServer, &HttpServerThread::fadeVolumeUp, this, &PlayerController::FadeVolumeUp);
    connect(httpServer, &HttpServerThread::fadeImageDown, this, &PlayerController::FadeImageDown);
    connect(httpServer, &HttpServerThread::fadeImageUp, this, &PlayerController::FadeImageUp);
    connect(httpServer, &HttpServerThread::loadFromAudioTracks, this, &PlayerController::LoadFromAudioTracks);
    connect(httpServer, &HttpServerThread::loadFromPlaylist, this, &PlayerController::LoadFromPlaylist);
    connect(httpServer, &HttpServerThread::loadFromSections, this, &PlayerController::LoadFromSections);
    connect(httpServer, &HttpServerThread::loadFromSlides, this, &PlayerController::LoadFromSlides);
    connect(httpServer, &HttpServerThread::selectFromSlides, this, &PlayerController::SelectFromSlides);
    connect(httpServer, &HttpServerThread::spinPitchUp, this, &PlayerController::SpinPitchUp);
    connect(httpServer, &HttpServerThread::spinPitchDown, this, &PlayerController::SpinPitchDown);
    connect(httpServer, &HttpServerThread::spinYawLeft, this, &PlayerController::SpinYawLeft);
    connect(httpServer, &HttpServerThread::spinYawRight, this, &PlayerController::SpinYawRight);
    connect(httpServer, &HttpServerThread::spinRollCCW, this, &PlayerController::SpinRollCCW);
    connect(httpServer, &HttpServerThread::spinRollCW, this, &PlayerController::SpinRollCW);
    connect(httpServer, &HttpServerThread::orientationAndSpinReset, this, &PlayerController::OrientationAndSpinReset);
    connect(httpServer, &HttpServerThread::runSurfaceTransition, this, &PlayerController::RunSurfaceTransition);
    connect(httpServer, &HttpServerThread::slidePrevious, this, &PlayerController::SlidePrevious);
    connect(httpServer, &HttpServerThread::slideNext, this, &PlayerController::SlideNext);

    httpServer->start();
}

void PlayerController::Next() {
    Q_EMIT next();
}

void PlayerController::Previous() {
    Q_EMIT previous();
}

void PlayerController::Pause() {
    Q_EMIT pause();
}

void PlayerController::PlayPause() {
    Q_EMIT playpause();
}

void PlayerController::Stop() {
    Q_EMIT stop();
}

void PlayerController::Play() {
    Q_EMIT play();
}

void PlayerController::Rewind() {
    if (m_mpv) {
        m_mpv->performRewind();
    }
}

void PlayerController::Seek(int timeInSec) {
    if (m_mpv) {
        m_mpv->seek(timeInSec);
    }
}

void PlayerController::SetAutoPlay(bool value) {
    if (m_mpv) {
        m_mpv->setAutoPlay(value);
    }
}

void PlayerController::LoadFromAudioTracks(int idx) {
    if (m_mpv) {
        m_mpv->setAudioId(idx + 1); // AudioID starts from 1.
    }
}

void PlayerController::LoadFromPlaylist(int idx) {
    Q_EMIT loadFromPlaylist(idx);
}

void PlayerController::LoadFromSections(int idx) {
    Q_EMIT loadFromSections(idx);
}

void PlayerController::LoadFromSlides(int idx) {
    if (m_slidesModel != nullptr && idx < m_slidesModel->numberOfSlides() && idx >= 0) {
        m_slidesModel->setSlideFadeTime(PresentationSettings::fadeDurationToNextSlide());
        m_slidesModel->setSelectedSlideIdx(idx);
        m_slidesModel->setTriggeredSlideIdx(idx);
    }
}

void PlayerController::SelectFromSlides(int idx) {
    if (m_slidesModel != nullptr && idx < m_slidesModel->numberOfSlides() && idx >= 0) {
        m_slidesModel->setSelectedSlideIdx(idx);
    }
}

void PlayerController::SetPosition(double pos) {
    if (m_mpv) {
        m_mpv->setPosition(pos);
    }
}

void PlayerController::SetSpeed(double factor) {
    if (m_mpv) {
        m_mpv->setSpeed(factor);
    }
}

void PlayerController::SetVolume(int level) {
    if (m_mpv) {
        m_mpv->setVolume(level);
    }
}

void PlayerController::SetSyncVolumeVisibilityFading(bool value) {
    if (m_mpv) {
        m_mpv->setSyncVolumeVisibilityFading(value);
    }
}

void PlayerController::FadeVolumeDown() {
    if (m_mpv) {
        Q_EMIT m_mpv->fadeVolumeDown();
    }
}

void PlayerController::FadeVolumeUp() {
    if (m_mpv) {
        Q_EMIT m_mpv->fadeVolumeUp();
    }
}

void PlayerController::FadeImageDown() {
    if (m_mpv) {
        Q_EMIT m_mpv->fadeImageDown();
    }
}

void PlayerController::FadeImageUp() {
    if (m_mpv) {
        Q_EMIT m_mpv->fadeImageUp();
    }
}

void PlayerController::SpinPitchUp(bool run) {
    Q_EMIT spinPitchUp(run);
}

void PlayerController::SpinPitchDown(bool run) {
    Q_EMIT spinPitchDown(run);
}

void PlayerController::SpinYawLeft(bool run) {
    Q_EMIT spinYawLeft(run);
}

void PlayerController::SpinYawRight(bool run) {
    Q_EMIT spinYawRight(run);
}

void PlayerController::SpinRollCW(bool run) {
    Q_EMIT spinRollCW(run);
}

void PlayerController::SpinRollCCW(bool run) {
    Q_EMIT spinRollCCW(run);
}

void PlayerController::OrientationAndSpinReset() {
    Q_EMIT orientationAndSpinReset();
}

void PlayerController::RunSurfaceTransition() {
    Q_EMIT runSurfaceTransition();
}

void PlayerController::SlidePrevious() {
    if (m_slidesModel != nullptr) {
        Q_EMIT m_slidesModel->previousSlide();
    }
}
void PlayerController::SlideNext() {
    if (m_slidesModel != nullptr) {
        Q_EMIT m_slidesModel->nextSlide();
    }
}

QString PlayerController::returnRelativeOrAbsolutePath(const QString &path) {
    QString filePath = path;
    filePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(filePath);

    QStringList pathsToConsider;
    pathsToConsider.append(LocationSettings::cPlayMediaLocation());
    pathsToConsider.append(LocationSettings::univiewVideoLocation());
    pathsToConsider.append(LocationSettings::cPlayFileLocation());

    // Assuming filePath is absolute
    // Looking for shortest relative path
    QString shortFilePath = filePath;
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            QString newRelativePath = foundDir.relativeFilePath(filePath);
            if (newRelativePath.length() < shortFilePath.length())
                shortFilePath = newRelativePath;
        }
    }
    return shortFilePath;
}

QString PlayerController::checkAndCorrectPath(const QString &path) {
    QString filePath = path;
    filePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(filePath);

    if (fileInfo.exists())
        return filePath;
    else if (fileInfo.isRelative()) { // Go through search list in order
        QStringList searchPaths;
        searchPaths.append(LocationSettings::cPlayMediaLocation());
        searchPaths.append(LocationSettings::cPlayFileLocation());
        searchPaths.append(LocationSettings::univiewVideoLocation());

        for (int i = 0; i < searchPaths.size(); i++) {
            QString newFilePath = QDir::cleanPath(searchPaths[i] + QDir::separator() + filePath);
            QFileInfo newFilePathInfo(newFilePath);
            if (newFilePathInfo.exists())
                return newFilePath;
        }
    }
    return QStringLiteral("");
}

QString PlayerController::returnFileBaseName(const QString &path) {
    QFileInfo fileInfo(path);
    return fileInfo.baseName();
}

QString PlayerController::returnFileName(const QString& path) {
    QFileInfo fileInfo(path);
    return fileInfo.fileName();
}

QString PlayerController::returnFileExtension(const QString& path) {
    QFileInfo fileInfo(path);
    return fileInfo.suffix();
}

float PlayerController::backgroundVisibility() {
    return SyncHelper::instance().variables.alphaBg;
}

void PlayerController::setBackgroundVisibility(float value) {
    SyncHelper::instance().variables.alphaBg = value;

    Q_EMIT backgroundVisibilityChanged();
}

QString PlayerController::backgroundImageFile() {
    return m_backgroundFile;
}

QUrl PlayerController::backgroundImageFileUrl() {
    if (SyncHelper::instance().variables.bgImageFile.empty()) {
        return QUrl();
    }

    return QUrl::fromLocalFile(QString::fromStdString(SyncHelper::instance().variables.bgImageFile));
}

void PlayerController::setBackgroundImageFile(const QString &path) {
    if (path.isEmpty()) {
        m_backgroundFile = QStringLiteral("");
        SyncHelper::instance().variables.bgImageFile = "";
        SyncHelper::instance().variables.bgImageFileDirty = true;
        setBackgroundVisibility(0.f);
        Q_EMIT backgroundImageChanged();
        return;
    }

    QString filePath = path;
    filePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(filePath);

    QString absolutePath;
    if (fileInfo.exists()) { // isAbsolute
        m_backgroundFile = returnRelativeOrAbsolutePath(path);
        absolutePath = path;
    } else if (fileInfo.isRelative()) { // isRelative
        QString bgFilePath = checkAndCorrectPath(path);
        if (bgFilePath.isEmpty()) {
            return;
        }
        m_backgroundFile = path;
        absolutePath = bgFilePath;
    } else
        return;

    absolutePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    std::string str = absolutePath.toStdString();
    if (SyncHelper::instance().variables.bgImageFile != str) {
        SyncHelper::instance().variables.bgImageFile = str;
        SyncHelper::instance().variables.bgImageFileDirty = true;
        setBackgroundVisibility(1.f);
    }

    Q_EMIT backgroundImageChanged();
}

int PlayerController::backgroundGridMode() {
    return SyncHelper::instance().variables.gridToMapOnBg;
}

void PlayerController::setBackgroundGridMode(int value) {
    SyncHelper::instance().variables.gridToMapOnBg = value;
}

int PlayerController::backgroundStereoMode() {
    return SyncHelper::instance().variables.stereoscopicModeBg;
}

void PlayerController::setBackgroundStereoMode(int value) {
    SyncHelper::instance().variables.stereoscopicModeBg = value;
}

float PlayerController::foregroundVisibility() {
    return SyncHelper::instance().variables.alphaFg;
}

void PlayerController::setForegroundVisibility(float value) {
    SyncHelper::instance().variables.alphaFg = value;

    Q_EMIT foregroundVisibilityChanged();
}

QString PlayerController::foregroundImageFile() {
    return m_foregroundFile;
}

QUrl PlayerController::foregroundImageFileUrl() {
    if (SyncHelper::instance().variables.fgImageFile.empty()) {
        return QUrl();
    }

    return QUrl::fromLocalFile(QString::fromStdString(SyncHelper::instance().variables.fgImageFile));
}

void PlayerController::setForegroundImageFile(const QString &path) {
    if (path.isEmpty()) {
        m_foregroundFile = QStringLiteral("");
        SyncHelper::instance().variables.fgImageFile = "";
        SyncHelper::instance().variables.fgImageFileDirty = true;
        Q_EMIT foregroundImageChanged();
        return;
    }

    QString filePath = path;
    filePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(filePath);

    QString absolutePath;
    if (fileInfo.exists()) { // isAbsolute
        m_foregroundFile = returnRelativeOrAbsolutePath(path);
        absolutePath = path;
    } else if (fileInfo.isRelative()) { // isRelative
        QString fgFilePath = checkAndCorrectPath(path);
        if (fgFilePath.isEmpty()) {
            return;
        }
        m_foregroundFile = path;
        absolutePath = fgFilePath;
    } else
        return;

    absolutePath.replace(QStringLiteral("file:///"), QStringLiteral(""));
    std::string str = absolutePath.toStdString();
    if (SyncHelper::instance().variables.fgImageFile != str) {
        SyncHelper::instance().variables.fgImageFile = str;
        SyncHelper::instance().variables.fgImageFileDirty = true;
    }

    Q_EMIT foregroundImageChanged();
}

int PlayerController::foregroundGridMode() {
    return SyncHelper::instance().variables.gridToMapOnFg;
}

void PlayerController::setForegroundGridMode(int value) {
    SyncHelper::instance().variables.gridToMapOnFg = value;
}

int PlayerController::foregroundStereoMode() {
    return SyncHelper::instance().variables.stereoscopicModeFg;
}

void PlayerController::setForegroundStereoMode(int value) {
    SyncHelper::instance().variables.stereoscopicModeFg = value;
}

float PlayerController::backgroundVisibilityOnMaster() {
    if (getViewModeOnMaster() == 1) {
        return 0.f;
    } else if (getViewModeOnMaster() == 2) {
        return 1.f;
    }

    if (backgroundVisibility() > 0.f) {
        if (SyncHelper::instance().variables.loadedFile.empty()) {
            return backgroundVisibility();
        } else {
            return 1.f - SyncHelper::instance().variables.alpha;
        }
    }

    return 0.f;
}

void PlayerController::setViewModeOnMaster(int value) {
    // 0 = Same as nodes
    // 1 = Show media always
    // 2 = Show background always
    m_viewModeOnMaster = value;

    Q_EMIT backgroundVisibilityChanged();
}

int PlayerController::getViewModeOnMaster() {
    return m_viewModeOnMaster;
}

void PlayerController::setViewModeOnClients(int value) {
    // 0 = Auto 2D/3D switch
    // 1 = Force 2D for all
    SyncHelper::instance().variables.viewMode = value;

    Q_EMIT viewModeOnClientsChanged();
}

int PlayerController::getViewModeOnClients() {
    return SyncHelper::instance().variables.viewMode;
}

bool PlayerController::rewindMediaOnEOF() {
    return m_rewindMediaOnEOF;
}

void PlayerController::setRewindMediaOnEOF(bool value) {
    m_rewindMediaOnEOF = value;

    Q_EMIT rewindMediaOnEOFChanged();
}

bool PlayerController::nodeWindowsOnTop() {
    return SyncHelper::instance().variables.windowOnTop;
}

void PlayerController::setNodeWindowsOnTop(bool value) {
    SyncHelper::instance().variables.windowOnTop = value;

    Q_EMIT nodeWindowOnTopChanged();
}

float PlayerController::nodeWindowsOpacity() {
    return SyncHelper::instance().variables.windowOpacity;
}

void PlayerController::setNodeWindowsOpacity(float value) {
    SyncHelper::instance().variables.windowOpacity = value;

    Q_EMIT nodeWindowOpacityChanged();
}

bool PlayerController::syncProperties() {
    return SyncHelper::instance().variables.syncOn;
}

void PlayerController::setSyncProperties(bool value) {
    SyncHelper::instance().variables.syncOn = value;
    Q_EMIT syncPropertiesChanged();
}

MpvObject *PlayerController::mpv() const {
    return m_mpv;
}

void PlayerController::setMpv(MpvObject *mpv) {
    if (m_mpv == mpv) {
        return;
    }
    m_mpv = mpv;
    httpServer->setMpv(mpv);
    Q_EMIT mpvChanged();
}

SlidesModel* PlayerController::slidesModel() const {
    return m_slidesModel;
}

void PlayerController::setSlidesModel(SlidesModel* sm) {
    if (m_slidesModel == sm) {
        return;
    }
    m_slidesModel = sm;
    httpServer->setSlidesModel(sm);
    Q_EMIT slidesModelChanged();
}
