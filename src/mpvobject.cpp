/*
 * SPDX-FileCopyrightText: 
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com> 
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma warning(disable : 4996)

#include "_debug.h"
#include "mpvobject.h"
#include "application.h"
#include "audiosettings.h"
#include "locationsettings.h"
#include "playbacksettings.h"
#include "playlistsettings.h"
#include "gridsettings.h"
#include "imagesettings.h"
#include "playlistitem.h"
#include "track.h"
#include "tracksmodel.h"
#include <iostream>

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QProcess>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QtGlobal>

void on_mpv_redraw(void *ctx)
{
    QMetaObject::invokeMethod(static_cast<MpvObject*>(ctx), "update", Qt::QueuedConnection);
}

static void *get_proc_address_mpv(void *ctx, const char *name)
{
    Q_UNUSED(ctx)

    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    if (!glctx) return nullptr;

    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}

MpvRenderer::MpvRenderer(MpvObject *new_obj)
    : obj{new_obj}
{}

void MpvRenderer::render()
{
    QOpenGLFramebufferObject *fbo = framebufferObject();
    mpv_opengl_fbo mpfbo;
    mpfbo.fbo = static_cast<int>(fbo->handle());
    mpfbo.w = fbo->width();
    mpfbo.h = fbo->height();
    mpfbo.internal_format = 0;

    mpv_render_param params[] = {
        // Specify the default framebuffer (0) as target. This will
        // render onto the entire screen. If you want to show the video
        // in a smaller rectangle or apply fancy transformations, you'll
        // need to render into a separate FBO and draw it manually.
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    // See render_gl.h on what OpenGL environment mpv expects, and
    // other API details.
    mpv_render_context_render(obj->mpv_gl, params);
}

QOpenGLFramebufferObject * MpvRenderer::createFramebufferObject(const QSize &size)
{
    // init mpv_gl:
    if (!obj->mpv_gl)
    {
        mpv_opengl_init_params gl_init_params{get_proc_address_mpv, nullptr};
        mpv_render_param params[]{
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };

        if (mpv_render_context_create(&obj->mpv_gl, obj->mpv, params) < 0)
            throw std::runtime_error("failed to initialize mpv GL context");
        mpv_render_context_set_update_callback(obj->mpv_gl, on_mpv_redraw, obj);
        Q_EMIT obj->ready();
    }

    return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
}

MpvObject::MpvObject(QQuickItem * parent)
    : QQuickFramebufferObject(parent)
    , mpv{mpv_create()}
    , mpv_gl(nullptr)
    , m_audioTracksModel(new TracksModel)
    , m_playlistModel(new PlayListModel)
    , m_playSectionsModel(new PlaySectionsModel)
    , m_currentSectionsIndex(-1)
    , m_currentSection(QStringLiteral(""), 0, 0, 0)
    , m_loadedFileStructure(QStringLiteral(""))
{
    if (!mpv)
        throw std::runtime_error("could not create mpv context");

    //setProperty("terminal", "yes");
    //setProperty("msg-level", "all=v");

    m_rotationSpeed = GridSettings::surfaceRotationSpeed();
    m_radius = GridSettings::surfaceRadius();
    m_fov = GridSettings::surfaceFov();
    m_angle = GridSettings::surfaceAngle();
    m_rotate = QVector3D(GridSettings::surfaceRotateX(), GridSettings::surfaceRotateY(), GridSettings::surfaceRotateZ());
    m_translate = QVector3D(GridSettings::surfaceTranslateX(), GridSettings::surfaceTranslateY(), GridSettings::surfaceTranslateZ());
    m_surfaceTransitionTime = GridSettings::surfaceTransitionTime();
    m_surfaceTransitionOnGoing = false;
    m_planeWidth = GridSettings::plane_Width_CM();
    m_planeHeight = GridSettings::plane_Height_CM();
    m_planeElevation = GridSettings::plane_Elevation_Degrees();
    m_planeDistance = GridSettings::plane_Distance_CM();
    m_syncVolumeVisibilityFading = PlaybackSettings::syncVolumeVisibilityFading();
    m_autoPlay = PlaylistSettings::autoPlayOnLoad();

    SyncHelper::instance().variables.radius = m_radius;
    SyncHelper::instance().variables.fov = m_fov;
    SyncHelper::instance().variables.angle = m_angle;
    SyncHelper::instance().variables.rotateX = m_rotate.x();
    SyncHelper::instance().variables.rotateY = m_rotate.y();
    SyncHelper::instance().variables.rotateZ = m_rotate.z();
    SyncHelper::instance().variables.translateX = m_translate.x();
    SyncHelper::instance().variables.translateY = m_translate.y();
    SyncHelper::instance().variables.translateZ = m_translate.z();
    SyncHelper::instance().variables.planeWidth = m_planeWidth;
    SyncHelper::instance().variables.planeHeight = m_planeHeight;
    SyncHelper::instance().variables.planeElevation = m_planeElevation;
    SyncHelper::instance().variables.planeDistance = m_planeDistance;
    SyncHelper::instance().variables.alpha = float(PlaybackSettings::visibility()) / 100.f;

    QString loadAudioInVidFolder = AudioSettings::loadAudioFileInVideoFolder() ? QStringLiteral("all") : QStringLiteral("no");
    setProperty(QStringLiteral("audio-file-auto"), loadAudioInVidFolder);
    setProperty(QStringLiteral("screenshot-template"), LocationSettings::screenshotTemplate());
    setProperty(QStringLiteral("sub-auto"), QStringLiteral("exact"));
    setProperty(QStringLiteral("volume-max"), QStringLiteral("100"));
    setProperty(QStringLiteral("keep-open"), QStringLiteral("yes"));

    setStereoscopicMode(ImageSettings::stereoModeForBackground());
    setGridToMapOn(ImageSettings::gridToMapOnForBackground());

    mpv::qt::load_configurations(mpv, QString::fromStdString(SyncHelper::instance().configuration.confAll));
    mpv::qt::load_configurations(mpv, QString::fromStdString(SyncHelper::instance().configuration.confMasterOnly));

    updateAudioDeviceList();

    mpv_observe_property(mpv, 0, "audio-device-list", MPV_FORMAT_NODE);
    mpv_observe_property(mpv, 0, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(mpv, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "time-remaining", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(mpv, 0, "volume", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(mpv, 0, "chapter", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "aid", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "sid", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "secondary-sid", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "contrast", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "brightness", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "gamma", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "saturation", MPV_FORMAT_INT64);
    mpv_observe_property(mpv, 0, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(mpv, 0, "video-params", MPV_FORMAT_NODE);

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");

    mpv_set_wakeup_callback(mpv, MpvObject::mpvEvents, this);

    connect(this, &MpvObject::fileLoaded,
            this, &MpvObject::loadTracks);

    connect(this, &MpvObject::planeChanged,
            this, &MpvObject::updatePlane);

    connect(this, &MpvObject::stereoscopicModeChanged,
        this, &MpvObject::updatePlane);

    connect(this, &MpvObject::positionChanged, this, [this]() {
        int pos = getProperty(QStringLiteral("time-pos")).toInt();
        double duration = getProperty(QStringLiteral("duration")).toDouble();
        if (!m_secondsWatched.contains(pos)) {
            m_secondsWatched << pos;
            setWatchPercentage(m_secondsWatched.count() * 100 / duration);
        }
    });
}

MpvObject::~MpvObject()
{
    // only initialized if something got drawn
    if (mpv_gl) {
        mpv_render_context_free(mpv_gl);
    }
    mpv_terminate_destroy(mpv);
}

PlayListModel *MpvObject::playlistModel()
{
    return m_playlistModel;
}

void MpvObject::setPlaylistModel(PlayListModel *model)
{
    m_playlistModel = model;
    Q_EMIT playlistModelChanged();
}

PlaySectionsModel* MpvObject::playSectionsModel()
{
    return m_playSectionsModel;
}

void MpvObject::setPlaySectionsModel(PlaySectionsModel* model)
{
    m_playSectionsModel = model;
    Q_EMIT playSectionsModelChanged();
}

QVariantList MpvObject::audioDevices() const
{
    return m_audioDevices;
}

void MpvObject::setAudioDevices(QVariantList devices)
{
    m_audioDevices = devices;
}

QStringList MpvObject::recentMediaFiles() const 
{
    return LocationSettings::recentLoadedMediaFiles();
}

void MpvObject::setRecentMediaFiles(QStringList list)
{
    LocationSettings::setRecentLoadedMediaFiles(list);
    LocationSettings::self()->save();
    Q_EMIT recentMediaFilesChanged();
}

QStringList MpvObject::recentPlaylists() const
{
    return LocationSettings::recentLoadedPlaylists();
}

void MpvObject::setRecentPlaylists(QStringList list) 
{
    LocationSettings::setRecentLoadedPlaylists(list);
    LocationSettings::self()->save();
    Q_EMIT recentPlaylistsChanged();
}

QString MpvObject::mediaTitle()
{
    return getProperty(QStringLiteral("media-title")).toString();
}

QString MpvObject::separateAudioFile()
{
    if(getProperty(QStringLiteral("current-tracks/audio/external")).toBool()) {
        return getProperty(QStringLiteral("current-tracks/audio/external-filename")).toString();
    }

    return m_separateAudioFile;
}

double MpvObject::position()
{
    return getProperty(QStringLiteral("time-pos")).toDouble();
}

void MpvObject::setPosition(double value)
{
    SyncHelper::instance().variables.timePosition = value;
    SyncHelper::instance().variables.timeDirty = true;
    m_lastSetPosition = value;
    SyncHelper::instance().variables.timeThreshold = double(PlaybackSettings::thresholdToSyncTimePosition())/1000.0;
    SyncHelper::instance().variables.timeThresholdEnabled = PlaybackSettings::useThresholdToSyncTimePosition();
    SyncHelper::instance().variables.timeThresholdOnLoopOnly = PlaybackSettings::applyThresholdSyncOnLoopOnly();
    SyncHelper::instance().variables.timeThresholdOnLoopCheckTime = PlaybackSettings::timeToCheckThresholdSyncAfterLoop()/1000.0;
    if (value == position()) {
        return;
    }
    setProperty(QStringLiteral("time-pos"), value);
    Q_EMIT positionChanged();
}

double MpvObject::remaining()
{
    return getProperty(QStringLiteral("time-remaining")).toDouble();
}

double MpvObject::duration()
{
    return getProperty(QStringLiteral("duration")).toDouble();
}

bool MpvObject::pause()
{
    return getProperty(QStringLiteral("pause")).toBool();
}

void MpvObject::setPause(bool value)
{
    SyncHelper::instance().variables.paused = value;
    setProperty(QStringLiteral("pause"), value);
    Q_EMIT pauseChanged();

    //If playing again and no visibility -> Fade up
    if (!value && (visibility() == 0)) {
        Q_EMIT fadeImageUp();
    }
}

bool MpvObject::autoPlay()
{
    return m_autoPlay;
}

void MpvObject::setAutoPlay(bool value)
{
    m_autoPlay = value;
    Q_EMIT autoPlayChanged();
}

void MpvObject::togglePlayPause()
{
    setPause(!pause());
}

void MpvObject::clearRecentMediaFilelist() {
    QStringList empty;
    LocationSettings::setRecentLoadedMediaFiles(empty);
    Q_EMIT recentMediaFilesChanged();
    LocationSettings::self()->save();
}

void MpvObject::clearRecentPlaylist() {
    QStringList empty;
    LocationSettings::setRecentLoadedPlaylists(empty);
    Q_EMIT recentPlaylistsChanged();
    LocationSettings::self()->save();
}

void MpvObject::performRewind() {
    if(PlaybackSettings::fadeDownBeforeRewind()) {
        Q_EMIT fadeDownTheRewind();
    }
    else {
        setPause(true);
        setPosition(0);
        Q_EMIT rewind();
    }
}

void MpvObject::seek(int timeInSec) {
    setPause(true);
    command(QStringList() << QStringLiteral("seek") << QString::number(timeInSec) << QStringLiteral("exact"));
    SyncHelper::instance().variables.timeDirty = true;
}

int MpvObject::volume()
{
    return getProperty(QStringLiteral("volume")).toInt();
}

void MpvObject::setVolume(int value)
{
    if (value == volume()) {
        return;
    }
    setProperty(QStringLiteral("volume"), value);
    Q_EMIT volumeChanged();
}

int MpvObject::chapter()
{
    return getProperty(QStringLiteral("chapter")).toInt();
}

void MpvObject::setChapter(int value)
{
    if (value == chapter()) {
        return;
    }
    setProperty(QStringLiteral("chapter"), value);
    Q_EMIT chapterChanged();
}

int MpvObject::audioId()
{
    return getProperty(QStringLiteral("aid")).toInt();
}

void MpvObject::setAudioId(int value)
{
    if (value == audioId()) {
        return;
    }
    if(value < 0) {
        setProperty(QStringLiteral("aid"), "auto");
    }
    else
        setProperty(QStringLiteral("aid"), value);
    
    Q_EMIT audioIdChanged();
}

int MpvObject::contrast()
{
    return getProperty(QStringLiteral("contrast")).toInt();
}

void MpvObject::setContrast(int value)
{
    SyncHelper::instance().variables.eqContrast = value;
    SyncHelper::instance().variables.eqDirty = true;
    if (value == contrast()) {
        return;
    }
    setProperty(QStringLiteral("contrast"), value);
    Q_EMIT contrastChanged();
}

int MpvObject::brightness()
{
    return getProperty(QStringLiteral("brightness")).toInt();
}

void MpvObject::setBrightness(int value)
{
    SyncHelper::instance().variables.eqBrightness = value;
    SyncHelper::instance().variables.eqDirty = true;
    if (value == brightness()) {
        return;
    }
    setProperty(QStringLiteral("brightness"), value);
    Q_EMIT brightnessChanged();
}

int MpvObject::gamma()
{
    return getProperty(QStringLiteral("gamma")).toInt();
}

void MpvObject::setGamma(int value)
{
    SyncHelper::instance().variables.eqGamma = value;
    SyncHelper::instance().variables.eqDirty = true;
    if (value == gamma()) {
        return;
    }
    setProperty(QStringLiteral("gamma"), value);
    Q_EMIT gammaChanged();
}

int MpvObject::saturation()
{
    return getProperty(QStringLiteral("saturation")).toInt();
}

void MpvObject::setSaturation(int value)
{
    SyncHelper::instance().variables.eqSaturation = value;
    SyncHelper::instance().variables.eqDirty = true;
    if (value == saturation()) {
        return;
    }
    setProperty(QStringLiteral("saturation"), value);
    Q_EMIT saturationChanged();
}

bool MpvObject::hwDecoding()
{
    if (getProperty(QStringLiteral("hwdec")) == QStringLiteral("yes")) {
        return true;
    } else {
        return false;
    }
}

void MpvObject::setHWDecoding(bool value)
{
    if (value) {
        setProperty(QStringLiteral("hwdec"), QStringLiteral("yes"));
    } else  {
        setProperty(QStringLiteral("hwdec"), QStringLiteral("no"));
    }
    Q_EMIT hwDecodingChanged();
}

double MpvObject::watchPercentage()
{
    return m_watchPercentage;
}

void MpvObject::setWatchPercentage(double value)
{
    if (m_watchPercentage == value) {
        return;
    }
    m_watchPercentage = value;
    Q_EMIT watchPercentageChanged();
}

bool MpvObject::syncVideo()
{
    return SyncHelper::instance().variables.syncOn;
}

void MpvObject::setSyncVideo(bool value)
{
    SyncHelper::instance().variables.syncOn = value;
    Q_EMIT syncVideoChanged();
}

bool MpvObject::syncVolumeVisibilityFading()
{
    return m_syncVolumeVisibilityFading;
}

void MpvObject::setSyncVolumeVisibilityFading(bool value)
{
    m_syncVolumeVisibilityFading = value;
    Q_EMIT syncVolumeVisibilityFadingChanged();
}

int MpvObject::visibility()
{
    return int(SyncHelper::instance().variables.alpha*100.f);
}

void MpvObject::setVisibility(int value)
{
    SyncHelper::instance().variables.alpha = float(value)/100.f;
    Q_EMIT visibilityChanged();
}

int MpvObject::eofMode() {
    return SyncHelper::instance().variables.eofMode;
}

void MpvObject::setEofMode(int value) {
    SyncHelper::instance().variables.eofMode = value;

    if (value == 0) { //Pause
        setProperty(QStringLiteral("loop-file"), QStringLiteral("no"));
        setProperty(QStringLiteral("keep-open"), QStringLiteral("yes"));
    }
    else if (value == 1) { //Continue
        setProperty(QStringLiteral("loop-file"), QStringLiteral("no"));
        setProperty(QStringLiteral("keep-open"), QStringLiteral("yes"));
    }
    else { //Loop
        setProperty(QStringLiteral("loop-file"), QStringLiteral("inf"));
        setProperty(QStringLiteral("keep-open"), QStringLiteral("yes"));
    }
    Q_EMIT eofModeChanged();
}

int MpvObject::stereoscopicMode()
{
    return SyncHelper::instance().variables.stereoscopicMode;
}

void MpvObject::setStereoscopicMode(int value)
{
    SyncHelper::instance().variables.stereoscopicMode = value;
    Q_EMIT stereoscopicModeChanged();
}

int MpvObject::gridToMapOn()
{
    return SyncHelper::instance().variables.gridToMapOn;
}

void MpvObject::setGridToMapOn(int value)
{
    if (SyncHelper::instance().variables.gridToMapOn != value) {
        SyncHelper::instance().variables.gridToMapOn = value;
        Q_EMIT gridToMapOnChanged();
    }
}

double MpvObject::rotationSpeed()
{
    return m_rotationSpeed;
}

void MpvObject::setRotationSpeed(double value)
{
    if (m_rotationSpeed == value) {
        return;
    }
    m_rotationSpeed = value;
    Q_EMIT rotationSpeedChanged();
}

double MpvObject::radius()
{
    return m_radius;
}

void MpvObject::setRadius(double value)
{
    SyncHelper::instance().variables.radius = value;
    if (m_radius == value) {
        return;
    }
    m_radius = value;
    Q_EMIT radiusChanged();
}

double MpvObject::fov()
{
    return m_fov;
}

void MpvObject::setFov(double value)
{
    SyncHelper::instance().variables.fov = value;
    if (m_fov == value) {
        return;
    }
    m_fov = value;
    Q_EMIT fovChanged();
}

double MpvObject::angle()
{
    return m_angle;
}

void MpvObject::setAngle(double value)
{
    if (qFuzzyCompare(m_angle, value)) {
        return;
    }
    SyncHelper::instance().variables.angle = value;
    m_angle = value;
    Q_EMIT angleChanged();
}

QVector3D MpvObject::rotate()
{
    return m_rotate;
}

void MpvObject::setRotate(QVector3D value)
{
    if (qFuzzyCompare(m_rotate, value)) {
        return;
    }
    m_rotate = value;
    SyncHelper::instance().variables.rotateX = value.x();
    SyncHelper::instance().variables.rotateY = value.y();
    SyncHelper::instance().variables.rotateZ = value.z();
    Q_EMIT rotateChanged();
}

QVector3D MpvObject::translate()
{
    return m_translate;
}

void MpvObject::setTranslate(QVector3D value)
{
    if (qFuzzyCompare(m_translate, value)) {
        return;
    }
    m_translate = value;
    SyncHelper::instance().variables.translateX = value.x();
    SyncHelper::instance().variables.translateY = value.y();
    SyncHelper::instance().variables.translateZ = value.z();
    Q_EMIT translateChanged();
}

double MpvObject::planeWidth()
{
    return m_planeWidth;
}

void MpvObject::setPlaneWidth(double value, bool updatePlane)
{
    SyncHelper::instance().variables.planeWidth = value;
    if (m_planeWidth == value) {
        return;
    }
    m_planeWidth = value;

    if(updatePlane)
        Q_EMIT planeChanged();
}

double MpvObject::planeHeight()
{
    return m_planeHeight;
}

void MpvObject::setPlaneHeight(double value, bool updatePlane)
{
    SyncHelper::instance().variables.planeHeight = value;
    if (m_planeHeight == value) {
        return;
    }
    m_planeHeight = value;

    if (updatePlane)
        Q_EMIT planeChanged();
}

double MpvObject::planeElevation()
{
    return m_planeElevation;
}

void MpvObject::setPlaneElevation(double value)
{
    SyncHelper::instance().variables.planeElevation = value;
    if (m_planeElevation == value) {
        return;
    }
    m_planeElevation = value;
    Q_EMIT planeChanged();
}

double MpvObject::planeDistance()
{
    return m_planeDistance;
}

void MpvObject::setPlaneDistance(double value)
{
    SyncHelper::instance().variables.planeDistance = value;
    if (m_planeDistance == value) {
        return;
    }
    m_planeDistance = value;
    Q_EMIT planeChanged();
}

int MpvObject::surfaceTransitionTime() {
    return m_surfaceTransitionTime;
}

void MpvObject::setSurfaceTransitionTime(int value) {
    m_surfaceTransitionTime = value;
    Q_EMIT surfaceTransitionTimeChanged();
}

bool MpvObject::surfaceTransitionOnGoing() {
    return m_surfaceTransitionOnGoing;
}

void MpvObject::setSurfaceTransitionOnGoing(bool value) {
    m_surfaceTransitionOnGoing = value;
    Q_EMIT surfaceTransitionOnGoingChanged();
}

QVariant MpvObject::getAudioDeviceList()
{
  return getProperty(QStringLiteral("audio-device-list"));
}

void MpvObject::updateAudioDeviceList()
{
  bool preferredDeviceFound = false;
  bool savedFirstDevice = false;
  QString firstDevice;
  QVariantList audioDevicesList;
  QVariant list = getAudioDeviceList();
  for(const QVariant& audioDevice : list.toList())
  {
    Q_ASSERT(audioDevice.type() == QVariant::Map);
    QVariantMap dmap = audioDevice.toMap();

    if(!preferredDeviceFound){
        QString device = dmap[QStringLiteral("name")].toString();
        if(device == AudioSettings::preferredAudioOutputDevice()){
            preferredDeviceFound = true;
        }
        if(!savedFirstDevice){
            firstDevice = device;
            savedFirstDevice = true;
        }
    }

    audioDevicesList << dmap;
  }

  m_audioDevices = audioDevicesList;

  if(AudioSettings::useCustomAudioOutput()){
      if(AudioSettings::useAudioDevice()){
          if(preferredDeviceFound){
            setProperty(QStringLiteral("audio-device"), AudioSettings::preferredAudioOutputDevice());
          }
          else{
            //AudioSettings::setPreferredAudioOutputDevice(firstDevice);
          }
      }
      else if(AudioSettings::useAudioDriver()){
          setProperty(QStringLiteral("ao"), AudioSettings::preferredAudioOutputDriver());
      }
  }

  Q_EMIT audioDevicesChanged();
}

QQuickFramebufferObject::Renderer *MpvObject::createRenderer() const
{
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(const_cast<MpvObject *>(this));
}

PlayListModel* MpvObject::getPlayListModel() const
{
    return m_playlistModel;
}

PlaySectionsModel* MpvObject::getPlaySectionsModel() const
{
    return m_playSectionsModel;
}

QString MpvObject::checkAndCorrectPath(const QString& filePath, const QStringList& searchPaths) {
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists())
        return filePath;
    else if (fileInfo.isRelative()) { // Go through search list in order
        for (int i = 0; i < searchPaths.size(); i++) {
            QString newFilePath = QDir::cleanPath(searchPaths[i] + QDir::separator() + filePath);
            QFileInfo newFilePathInfo(newFilePath);
            if (newFilePathInfo.exists())
                return newFilePath;
        }
    }
    return QStringLiteral("");
}

void MpvObject::loadFile(const QString &file, bool updateLastPlayedFile)
{
    QString fileToLoad = file;
    fileToLoad.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(fileToLoad);
    if (!fileInfo.exists()) {
        QStringList fileSearchPaths;
        fileSearchPaths.append(fileInfo.absoluteDir().absolutePath());
        fileSearchPaths.append(LocationSettings::cPlayFileLocation());
        fileSearchPaths.append(LocationSettings::cPlayMediaLocation());
        fileSearchPaths.append(LocationSettings::univiewVideoLocation());
        fileToLoad = checkAndCorrectPath(fileToLoad, fileSearchPaths);
        if (fileToLoad.isEmpty()) {
            return;
        }
    }

    m_playlistModel->setPlayingVideo(-1);

    QString ext = fileInfo.suffix();
    if (ext == QStringLiteral("cplayfile") || ext == QStringLiteral("cplay_file") || ext == QStringLiteral("fdv")) {
        PlayListItem* videoFile = loadMediaFileDescription(fileToLoad);
        if (videoFile) {
            loadItem(videoFile->data(), updateLastPlayedFile);
            m_playSectionsModel->updateCurrentEditItem(*videoFile);
            Q_EMIT playSectionsModelChanged();
        }

        if (ext == QStringLiteral("cplayfile")) {
            m_playSectionsModel->setCurrentEditItemIsEdited(false);
        }
        else {
            m_playSectionsModel->setCurrentEditItemIsEdited(true);
        }

    }
    else if (ext == QStringLiteral("playlist")) {
        loadUniviewPlaylist(fileToLoad);
    }
    else if (ext == QStringLiteral("cplaylist") || ext == QStringLiteral("cplay_playlist")) {
        loadJSONPlayList(fileToLoad);
    }
    else {
        m_loadedFileStructure = fileToLoad;
        m_separateAudioFile = QStringLiteral("");
        SyncHelper::instance().variables.loadedFile = fileToLoad.toStdString();
        SyncHelper::instance().variables.overlayFile = "";
        SyncHelper::instance().variables.loadFile = true;
        SyncHelper::instance().variables.overlayFileDirty = true;

        //setProperty("lavfi-complex", "");
        setAudioId(-1);
        m_currentSectionsIndex = -1;
        m_playSectionsModel->clear();
        m_playSectionsModel->setCurrentEditItemIsEdited(true);
        command(QStringList() << QStringLiteral("loadfile") << fileToLoad, true);

        if (updateLastPlayedFile) {
            LocationSettings::setLastPlayedFile(file);
            updateRecentLoadedMediaFiles(fileToLoad);
            LocationSettings::self()->save();
        }
        else {
            PlaylistSettings::setLastPlaylistIndex(m_playlistModel->getPlayingVideo());
            PlaylistSettings::self()->save();
        }
    }
}

void MpvObject::addFileToPlaylist(const QString& file) {
    QFileInfo fi(file);
    QString ext = fi.suffix();
    
    PlayListItem* item = loadMediaFileDescription(file);

    if (item) {
        m_playlistModel->addItem(item);
        Q_EMIT playlistModelChanged();
    }
}

void MpvObject::clearPlaylist() {
    m_playlistModel->clear();
    Q_EMIT playlistModelChanged();
}

void MpvObject::setLoadedAsCurrentEditItem() {
    if (m_loadedFileStructure.isEmpty())
        return;

    m_currentSectionsIndex = -1;
    PlayListItem newCurrentItem(m_loadedFileStructure);

    if(!m_playSectionsModel->isEmpty())
        newCurrentItem.setData(m_playSectionsModel->currentEditItem()->data());

    newCurrentItem.setMediaFile(QString::fromStdString(SyncHelper::instance().variables.loadedFile));
    newCurrentItem.setSeparateOverlayFile(QString::fromStdString(SyncHelper::instance().variables.overlayFile));
    newCurrentItem.setSeparateAudioFile(separateAudioFile());
    newCurrentItem.setMediaTitle(mediaTitle());
    newCurrentItem.setDuration(duration());
    newCurrentItem.setGridToMapOn(gridToMapOn());
    newCurrentItem.setStereoVideo(stereoscopicMode());
    newCurrentItem.setEofMode(eofMode());
    m_playSectionsModel->updateCurrentEditItem(newCurrentItem);
}

void MpvObject::loadSection(int playSectionsIndex) {
    if (m_playSectionsModel->currentEditItem()) {
        m_currentSectionsIndex = -1; //Disabling current section first
        
        if (playSectionsIndex < 0 || playSectionsIndex >= m_playSectionsModel->getNumberOfSections()) {
            m_currentSection = PlayListItemData::Section(QStringLiteral(""), 0, 0, 0);
            if (SyncHelper::instance().variables.loopTimeEnabled) {
                SyncHelper::instance().variables.loopTimeA = 0;
                SyncHelper::instance().variables.loopTimeB = 0;
                SyncHelper::instance().variables.loopTimeEnabled = false;
                SyncHelper::instance().variables.loopTimeDirty = true;
                setProperty(QStringLiteral("ab-loop-a"), QStringLiteral("no"));
                setProperty(QStringLiteral("ab-loop-b"), QStringLiteral("no"));
            }
            m_playSectionsModel->setPlayingSection(m_currentSectionsIndex);
            Q_EMIT sectionLoaded(m_currentSectionsIndex);
            return;
        }

        m_currentSection = m_playSectionsModel->currentEditItem()->getSection(playSectionsIndex);
        setPosition(m_currentSection.startTime);
        if (m_currentSection.eosMode == 4) {
            SyncHelper::instance().variables.loopTimeA = m_currentSection.startTime;
            SyncHelper::instance().variables.loopTimeB = m_currentSection.endTime;
            SyncHelper::instance().variables.loopTimeEnabled = true;
            SyncHelper::instance().variables.loopTimeDirty = true;
            setProperty(QStringLiteral("ab-loop-a"), m_currentSection.startTime);
            setProperty(QStringLiteral("ab-loop-b"), m_currentSection.endTime);
        }
        else if (SyncHelper::instance().variables.loopTimeEnabled) {
            SyncHelper::instance().variables.loopTimeA = 0;
            SyncHelper::instance().variables.loopTimeB = 0;
            SyncHelper::instance().variables.loopTimeEnabled = false;
            SyncHelper::instance().variables.loopTimeDirty = true;
            setProperty(QStringLiteral("ab-loop-a"), QStringLiteral("no"));
            setProperty(QStringLiteral("ab-loop-b"), QStringLiteral("no"));
        }
        m_currentSectionsIndex = playSectionsIndex; //Enabling new section
        m_playSectionsModel->setPlayingSection(m_currentSectionsIndex);
        Q_EMIT sectionLoaded(m_currentSectionsIndex);
    }
}

void MpvObject::loadItem(int playListIndex, bool updateLastPlayedFile) {
    if (playListIndex < 0 || playListIndex >= m_playlistModel->getPlayListSize()) {
        return;
    }

    try {
        QPointer<PlayListItem> item = m_playlistModel->getItem(playListIndex);
        if (!item) {
            qWarning() << QStringLiteral("PlayListItem pointer was null");
            return;
        }
        item->loadDetailsFromDisk();
        PlayListItemData pld = item->data();
        loadItem(pld, updateLastPlayedFile);
        m_playSectionsModel->updateCurrentEditItem(*item);
        Q_EMIT playSectionsModelChanged();
        m_playSectionsModel->setCurrentEditItemIsEdited(false);
    }
    catch (...) {
    }
}

void MpvObject::loadItem(PlayListItemData itemData, bool updateLastPlayedFile, QString flag) {
    try {
        setAudioId(-1);

        QStringList optionsList;

        if (itemData.separateOverlayFile() != QStringLiteral("")) {
            optionsList << QStringLiteral("external-file=") + itemData.separateOverlayFile();
        }

        if (itemData.separateAudioFile() != QStringLiteral("")) {
            optionsList << QStringLiteral("audio-file=") + itemData.separateAudioFile();
        } 

        if (itemData.mediaTitle() != QStringLiteral("")) {
            optionsList << QStringLiteral("force-media-title=") + itemData.mediaTitle();
        }

        QString options = QStringLiteral("");
        if (optionsList.size() > 0) {
            options = optionsList.join(QStringLiteral(","));
        }

        m_loadedFileStructure = itemData.filePath();
        QStringList newCommand = QStringList() << QStringLiteral("loadfile") << itemData.mediaFile() << flag << options;
        
        qInfo() << newCommand;

        m_currentSectionsIndex = -1;
        
        command(newCommand, true);
        
        SyncHelper::instance().variables.loadedFile = itemData.mediaFile().toStdString();
        SyncHelper::instance().variables.overlayFile = itemData.separateOverlayFile().toStdString();
        SyncHelper::instance().variables.loadFile = true;
        SyncHelper::instance().variables.overlayFileDirty = true;
        m_separateAudioFile = itemData.separateAudioFile();

        if (itemData.gridToMapOn() >= 0)
            setGridToMapOn(itemData.gridToMapOn());

        if (itemData.stereoVideo() >= 0)
            setStereoscopicMode(itemData.stereoVideo());

        if (itemData.eofMode() >= 0)
            setEofMode(itemData.eofMode());

        if (updateLastPlayedFile) {
            LocationSettings::setLastPlayedFile(itemData.filePath());
            updateRecentLoadedMediaFiles(itemData.filePath());
        }
        else {
            PlaylistSettings::setLastPlaylistIndex(m_playlistModel->getPlayingVideo());
            PlaylistSettings::self()->save();
        }
    }
    catch (...) {
    }
}

PlayListItem* MpvObject::loadMediaFileDescription(const QString& file) {
    QString fileToLoad = file;
    fileToLoad.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(fileToLoad);
    if (!fileInfo.exists())
        return nullptr;

    auto item = new PlayListItem(fileToLoad);
    item->loadDetailsFromDisk();

    return item;
}

void MpvObject::loadJSONPlayList(const QString& file, bool updateLastPlayedFile) {
    QString fileToLoad = file;
    fileToLoad.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo jsonFileInfo(fileToLoad);
    if (!jsonFileInfo.exists())
        return;

    QFile f(fileToLoad);
    f.open(QIODevice::ReadOnly);
    QByteArray fileContent = f.readAll();
    f.close();

    QJsonDocument doc = QJsonDocument::fromJson(fileContent);
    if (doc.isNull())
    {
        qDebug() << QStringLiteral("Parsing C-play playlist failed: ") << fileToLoad;
        return;
    }

    QJsonObject obj = doc.object();
    QJsonValue value = obj.value(QStringLiteral("playlist"));
    QJsonArray array = value.toArray();

    m_playlistModel->clear();
    Playlist m_playList;

    QStringList fileSearchPaths;
    fileSearchPaths.append(jsonFileInfo.absoluteDir().absolutePath());
    fileSearchPaths.append(LocationSettings::cPlayFileLocation());
    fileSearchPaths.append(LocationSettings::cPlayMediaLocation());
    fileSearchPaths.append(LocationSettings::univiewVideoLocation());

    for(const QJsonValue & v : array) {
        QJsonObject o = v.toObject();
        if (o.contains(QStringLiteral("file"))) {
            QString filePath = o.value(QStringLiteral("file")).toString();
            QString checkedFilePath = checkAndCorrectPath(filePath, fileSearchPaths);
            if (!checkedFilePath.isEmpty()) {
                QFileInfo checkedFilePathInfo(checkedFilePath);
                QString fileExt = checkedFilePathInfo.suffix();
                PlayListItem* filePtr = nullptr;

                if (fileExt == QStringLiteral("cplayfile") || fileExt == QStringLiteral("cplay_file") || fileExt == QStringLiteral("fdv")) {
                    filePtr = loadMediaFileDescription(checkedFilePath);
                }
                else {
                    filePtr = new PlayListItem(checkedFilePath);
                }

                if (filePtr != NULL) {
                    if (o.contains(QStringLiteral("on_file_end"))) {
                        QString eofMode = o.value(QStringLiteral("on_file_end")).toString();
                        if (eofMode == QStringLiteral("pause")) {
                            filePtr->setEofMode(0);
                        }
                        else if (eofMode == QStringLiteral("continue")) {
                            filePtr->setEofMode(1);
                        }
                        else if (eofMode == QStringLiteral("loop")) {
                            filePtr->setEofMode(2);
                        }
                    }

                    m_playList.append(QPointer<PlayListItem>(filePtr));
                }
                else
                    qDebug() << QStringLiteral("Parsing file failed: ") << checkedFilePath;
            }
            else
                qDebug() << QStringLiteral("Could not find file: ") << filePath;

           
        }
    }

    m_playlistModel->setPlayListName(jsonFileInfo.baseName());
    m_playlistModel->setPlayListPath(fileToLoad);

    // save playlist to disk
    m_playlistModel->setPlayList(m_playList);
    Q_EMIT playlistModelChanged();

    if (updateLastPlayedFile) {
        LocationSettings::setLastPlayedFile(fileToLoad);
        updateRecentLoadedPlaylists(fileToLoad);
    }
}

void MpvObject::loadUniviewPlaylist(const QString& file, bool updateLastPlayedFile)
{
    QString fileToLoad = file;
    fileToLoad.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileInfo(fileToLoad);
    if (!fileInfo.exists())
        return;

    QFile f(fileToLoad);
    f.open(QIODevice::ReadOnly);
    QString fileContent = QString::fromUtf8(f.readAll());
    f.close();

    QStringList playListEntries = fileContent.split(QRegularExpression(QStringLiteral("[\r\n]")), Qt::SkipEmptyParts);

    int videoItems = playListEntries.at(1).mid(14).toInt(); //"NumberOfItems="

    m_playlistModel->clear();
    Playlist m_playList;

    for (int i = 0; i < videoItems; ++i) {
        int itemStart = (i * 7) + 2;

        QString title = playListEntries.at(itemStart + 1).mid(5); //"Name="
        QString path = playListEntries.at(itemStart + 2).mid(5); //"Path="
        double startTime = playListEntries.at(itemStart + 3).mid(10).toDouble(); //"Starttime="
        double endTime = playListEntries.at(itemStart + 4).mid(8).toDouble(); //"Endtime="
        int eofMode = playListEntries.at(itemStart + 5).mid(9).toInt(); //"Loopmode="
        int transitionMode = playListEntries.at(itemStart + 6).mid(15).toInt(); //"Transitionmode="

        QFileInfo videoFileInfo(path);
        QString videoFileExt = videoFileInfo.suffix();
        PlayListItem* video;

        if (videoFileExt == QStringLiteral("fdv")) {
            video = loadMediaFileDescription(QDir::cleanPath(fileInfo.absoluteDir().absolutePath() + QDir::separator() + path));
        }
        else {
            video = new PlayListItem(path);
            video->setDuration(endTime - startTime);
        }

        video->setMediaTitle(title);

        if (title.contains(QStringLiteral("3D"))) // Assume 3D side-by-side stereo
            video->setStereoVideo(1);
        else if (title.contains(QStringLiteral("2D")))
            video->setStereoVideo(0);

        if(eofMode == 0) //Next in Uniview
            video->setEofMode(1);
        else if (eofMode == 2) //Loop in Uniview
            video->setEofMode(2);
        else // Set pause otherwise
            video->setEofMode(0);

        video->setTransitionMode(transitionMode);

        m_playList.append(QPointer<PlayListItem>(video));
    }

    m_playlistModel->setPlayListName(fileInfo.baseName());
    m_playlistModel->setPlayListPath(fileToLoad);

    // save playlist to disk
    m_playlistModel->setPlayList(m_playList);
    Q_EMIT playlistModelChanged();

    if (updateLastPlayedFile) {
        LocationSettings::setLastPlayedFile(fileToLoad);
        updateRecentLoadedPlaylists(fileToLoad);
    }
}

void MpvObject::mpvEvents(void *ctx)
{
    QMetaObject::invokeMethod(static_cast<MpvObject*>(ctx), "eventHandler", Qt::QueuedConnection);
}

void MpvObject::eventHandler()
{
    while (mpv) {
        mpv_event *event = mpv_wait_event(mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) {
            break;
        }
        switch (event->event_id) {
        case MPV_EVENT_START_FILE: {
            Q_EMIT fileStarted();
            break;
        }
        case MPV_EVENT_FILE_LOADED: {
            setLoadedAsCurrentEditItem();
            Q_EMIT fileLoaded();
            break;
        }
        case MPV_EVENT_END_FILE: {
            auto prop = (mpv_event_end_file *)event->data;
            if (prop->reason == MPV_END_FILE_REASON_EOF) {
                Q_EMIT endFile(QStringLiteral("eof"));
            } else if(prop->reason == MPV_END_FILE_REASON_ERROR) {
                Q_EMIT endFile(QStringLiteral("error"));
            }
            break;
        }
        case MPV_EVENT_VIDEO_RECONFIG: {
            // Retrieve the new video size.
            int64_t w, h;
            if (mpv_get_property(mpv, "dwidth", MPV_FORMAT_INT64, &w) >= 0 &&
                mpv_get_property(mpv, "dheight", MPV_FORMAT_INT64, &h) >= 0 &&
                w > 0 && h > 0)
            {
                m_videoWidth = (int)w;
                m_videoHeight = (int)h;
                Q_EMIT planeChanged();
            }
            break;
        }
        case MPV_EVENT_PROPERTY_CHANGE: {
            mpv_event_property *prop = (mpv_event_property *)event->data;

            if (strcmp(prop->name, "audio-device-list") == 0) {
                if (prop->format == MPV_FORMAT_NODE) {
                    updateAudioDeviceList();
                }
            } else if (strcmp(prop->name, "time-pos") == 0) {
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    double latestPosition = *reinterpret_cast<double*>(prop->data);
                    SyncHelper::instance().variables.timePosition = latestPosition;
                    SyncHelper::instance().variables.paused = pause();
                    SyncHelper::instance().variables.timeThreshold = double(PlaybackSettings::thresholdToSyncTimePosition())/1000.0;
                    sectionPositionCheck(latestPosition);
                    Q_EMIT positionChanged();
                }
            } else if (strcmp(prop->name, "media-title") == 0) {
                if (prop->format == MPV_FORMAT_STRING) {
                    Q_EMIT mediaTitleChanged();
                }
            } else if (strcmp(prop->name, "time-remaining") == 0) {
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    Q_EMIT remainingChanged();
                }
            } else if (strcmp(prop->name, "duration") == 0) {
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    Q_EMIT durationChanged();
                }
            } else if (strcmp(prop->name, "volume") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    Q_EMIT volumeChanged();
                }
            } else if (strcmp(prop->name, "pause") == 0) {
                if (prop->format == MPV_FORMAT_FLAG) {
                    const QVariant eofReached = mpv::qt::get_property(mpv, QStringLiteral("eof-reached"));
                    if(eofReached.toBool()) {
                        Q_EMIT endFile(QStringLiteral("eof"));
                    }
                    //m_lastSetPosition = position();
                    SyncHelper::instance().variables.paused = pause();
                    Q_EMIT pauseChanged();
                }
            } else if (strcmp(prop->name, "chapter") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    Q_EMIT chapterChanged();
                }
            } else if (strcmp(prop->name, "aid") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    Q_EMIT audioIdChanged();
                }
            } else if (strcmp(prop->name, "contrast") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    Q_EMIT contrastChanged();
                }
            } else if (strcmp(prop->name, "brightness") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    Q_EMIT brightnessChanged();
                }
            } else if (strcmp(prop->name, "gamma") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    Q_EMIT gammaChanged();
                }
            } else if (strcmp(prop->name, "saturation") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    Q_EMIT saturationChanged();
                }
            } else if (strcmp(prop->name, "track-list") == 0) {
                if (prop->format == MPV_FORMAT_NODE) {
                    loadTracks();
                }
            } else if (strcmp(prop->name, "video-params") == 0) {
                if (prop->format == MPV_FORMAT_NODE) {
                    const QVariant videoParams = mpv::qt::get_property(mpv, QStringLiteral("video-params"));
                    auto vm = videoParams.toMap();
                    m_videoWidth = vm[QStringLiteral("w")].toInt();
                    m_videoHeight = vm[QStringLiteral("h")].toInt();
                    Q_EMIT planeChanged();
                }
            }
            break;
        }
        default: ;
            // Ignore uninteresting or unknown events.
        }
    }
}

void MpvObject::performSurfaceTransition() {
    Q_EMIT surfaceTransitionPerformed();
}

void MpvObject::loadTracks()
{
    m_audioTracks.clear();

    const QList<QVariant> tracks = getProperty(QStringLiteral("track-list")).toList();
    int audioIndex = 0;
    for (const auto &track : tracks) {
        const auto t = track.toMap();
        if (track.toMap()[QStringLiteral("type")] == QStringLiteral("audio")) {
            auto newTrack = new Track();
            newTrack->setCodec(t[QStringLiteral("codec")].toString());
            newTrack->setType(t[QStringLiteral("type")].toString());
            newTrack->setDefaut(t[QStringLiteral("default")].toBool());
            newTrack->setDependent(t[QStringLiteral("dependent")].toBool());
            newTrack->setForced(t[QStringLiteral("forced")].toBool());
            newTrack->setId(t[QStringLiteral("id")].toLongLong());
            newTrack->setSrcId(t[QStringLiteral("src-id")].toLongLong());
            newTrack->setFfIndex(t[QStringLiteral("ff-index")].toLongLong());
            newTrack->setLang(t[QStringLiteral("lang")].toString());
            newTrack->setTitle(t[QStringLiteral("title")].toString());
            newTrack->setIndex(audioIndex);

            m_audioTracks.insert(audioIndex, newTrack);
            audioIndex++;
        }
    }
    m_audioTracksModel->setTracks(m_audioTracks);

    Q_EMIT audioTracksModelChanged();
}

void MpvObject::updatePlane() {
    int pcsbov = GridSettings::plane_Calculate_Size_Based_on_Video();
    int sm = stereoscopicMode();
    if (pcsbov == 1) { //Calculate width from video
        double ratio = double(m_videoWidth) / double(m_videoHeight);

        if (sm == 1) { //Side-by-side
            ratio *= 0.5f;
        }
        else if (sm == 2) { //Top-bottom
            ratio *= 2.0f;
        }
        else if (sm == 3) { //Top-bottom-flip
            ratio = double(m_videoHeight) / double(m_videoWidth);
            ratio *= 2.0f;
        }

        setPlaneWidth(ratio * planeHeight(), false);
    }
    else if (pcsbov == 2) { //Calculate height from video
        double ratio = double(m_videoHeight) / double(m_videoWidth);

        if (sm == 1) { //Side-by-side
            ratio *= 0.5f;
        }
        else if (sm == 2) { //Top-bottom
            ratio *= 2.0f;
        }
        else if (sm == 3) { //Top-bottom-flip
            ratio = double(m_videoWidth) / double(m_videoHeight);
            ratio *= 2.0f;
        }

        setPlaneHeight(ratio * planeWidth(), false);
    }
}

void MpvObject::updateRecentLoadedMediaFiles(QString path)
{
    QStringList recentMediaFiles = LocationSettings::recentLoadedMediaFiles();
    recentMediaFiles.push_front(path);
    recentMediaFiles.removeDuplicates();
    if (recentMediaFiles.size() > 8)
        recentMediaFiles.pop_back();
    LocationSettings::setRecentLoadedMediaFiles(recentMediaFiles);
    Q_EMIT recentMediaFilesChanged();
    LocationSettings::self()->save();
}

void MpvObject::updateRecentLoadedPlaylists(QString path)
{
    QStringList recentPlaylists = LocationSettings::recentLoadedPlaylists();
    recentPlaylists.push_front(path);
    recentPlaylists.removeDuplicates();
    if (recentPlaylists.size() > 8)
        recentPlaylists.pop_back();
    LocationSettings::setRecentLoadedPlaylists(recentPlaylists);
    Q_EMIT recentPlaylistsChanged();
    LocationSettings::self()->save();
}

void MpvObject::sectionPositionCheck(double position) {
    if (pause())
        return;

    if (m_currentSectionsIndex >= 0) {
        if (m_currentSection.eosMode == 0) { //Pause
            if (m_currentSection.endTime <= position) {
                setPause(true);
                m_currentSectionsIndex = -1;
            }
        }
        else if (m_currentSection.eosMode == 1) { // Fade out (then pause)
            if ((m_currentSection.endTime - (((double)PlaybackSettings::fadeDuration())/1000.0)) <= position) {
                Q_EMIT fadeImageDown();
                m_currentSection.eosMode = 0;
            }
        }
        else if (m_currentSection.eosMode == 3) { //Next
            if (m_currentSection.endTime <= position) {
                if (m_currentSectionsIndex + 1 < m_playSectionsModel->currentEditItem()->numberOfSections()) {
                    loadSection(m_currentSectionsIndex + 1);
                }
                else {
                    loadSection(0);
                }
            }
        }
    }
}

TracksModel *MpvObject::audioTracksModel() const
{
    return m_audioTracksModel;
}

QUrl MpvObject::getOverlayFileUrl() const
{
    return QUrl::fromLocalFile(QString::fromStdString(SyncHelper::instance().variables.overlayFile));
}

QString MpvObject::getReadableExternalConfiguration() {
    QString returnStr;

    QStringList confFiles;
    confFiles.append(QString::fromStdString(SyncHelper::instance().configuration.confAll));
    confFiles.append(QString::fromStdString(SyncHelper::instance().configuration.confMasterOnly));
    confFiles.append(QString::fromStdString(SyncHelper::instance().configuration.confNodesOnly));

    for(const QString conf : confFiles) {
        returnStr.append(conf + QStringLiteral("\n"));
        QFileInfo confFileInfo(conf);
        if (confFileInfo.exists()) {
            QFile f(conf);
            f.open(QIODevice::ReadOnly);
            returnStr.append(QString::fromUtf8(f.readAll()) + QStringLiteral("\n"));
            f.close();
        }
        else {
            returnStr.append(QStringLiteral("Configuration file was not found\n"));
        }
    }
    return returnStr;
}

void MpvObject::getYouTubePlaylist(const QString&)
{
    /*m_playlistModel->clear();

    // use youtube-dl to get the required playlist info as json
    auto ytdlProcess = new QProcess();
    ytdlProcess->setProgram("youtube-dl");
    ytdlProcess->setArguments(QStringList() << "-J" << "--flat-playlist" << path);
    ytdlProcess->start();

    QObject::connect(ytdlProcess, (void (QProcess::*)(int,QProcess::ExitStatus))&QProcess::finished,
                     this, [=](int, QProcess::ExitStatus) {
        // use the json to populate the playlist model
        using Playlist = QList<PlayListItem*>;
        Playlist m_playList;

        QString json = ytdlProcess->readAllStandardOutput();
        QJsonObject obj;
        QJsonValue entries = QJsonDocument::fromJson(json.toUtf8())["entries"];

        QString playlistFileContent;

        for (int i = 0; i < entries.toArray().size(); ++i) {
            auto url = QString("https://youtu.be/%1").arg(entries[i]["id"].toString());
            auto title = entries[i]["title"].toString();
            auto duration = entries[i]["duration"].toDouble();

            auto video = new PlayListItem(url, i, m_playlistModel);
            video->setMediaTitle(!title.isEmpty() ? title : url);
            video->setFileName(!title.isEmpty() ? title : url);

            video->setDuration(Application::formatTime(duration));
            m_playList.append(video);

            playlistFileContent += QString("%1,%2,%3\n").arg(url, title, QString::number(duration));
        }

        // save playlist to disk
        m_playlistModel->setPlayList(m_playList);

        Q_EMIT youtubePlaylistLoaded();
    });*/
}

int MpvObject::setProperty(const QString &name, const QVariant &value, bool debug)
{
    auto result = mpv::qt::set_property(mpv, name, value);
    if (debug) {
        DEBUG << mpv::qt::get_error(result);
    }
    return result;
}

QVariant MpvObject::getProperty(const QString &name, bool debug)
{
    auto result = mpv::qt::get_property(mpv, name);
    if (debug) {
        DEBUG << mpv::qt::get_error(result);
    }
    return result;
}

QVariant MpvObject::command(const QVariant &params, bool debug)
{
    auto result = mpv::qt::command(mpv, params);
    if (debug) {
        DEBUG << mpv::qt::get_error(result);
    }
    return result;
}

void MpvObject::saveTimePosition()
{
    // saving position is disabled
    if (PlaybackSettings::minDurationToSavePosition() == -1) {
        return;
    }
    // position is saved only for files longer than PlaybackSettings::minDurationToSavePosition()
    if (getProperty(QStringLiteral("duration")).toInt() < PlaybackSettings::minDurationToSavePosition() * 60) {
        return;
    }

    auto hash = md5(getProperty(QStringLiteral("path")).toString());
    auto timePosition = getProperty(QStringLiteral("time-pos"));
    auto configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    KConfig *config = new KConfig(configPath.append(QStringLiteral("/cplay/watch-later/")).append(hash));
    config->group(QStringLiteral("")).writeEntry("TimePosition", timePosition);
    config->sync();
}

double MpvObject::loadTimePosition()
{
    // saving position is disabled
    if (PlaybackSettings::minDurationToSavePosition() == -1) {
        return 0;
    }
    // position is saved only for files longer than PlaybackSettings::minDurationToSavePosition()
    // but there can be cases when there is a saved position for files lower than minDurationToSavePosition()
    // when minDurationToSavePosition() was increased after position was already saved
    if (getProperty(QStringLiteral("duration")).toInt() < PlaybackSettings::minDurationToSavePosition() * 60) {
        return 0;
    }

    auto hash = md5(getProperty(QStringLiteral("path")).toString());
    auto configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    KConfig *config = new KConfig(configPath.append(QStringLiteral("/cplay/watch-later/")).append(hash));
    int position = config->group(QStringLiteral("")).readEntry("TimePosition", QString::number(0)).toDouble();

    return position;
}

void MpvObject::resetTimePosition()
{
    auto hash = md5(getProperty(QStringLiteral("path")).toString());
    auto configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QFile f(configPath.append(QStringLiteral("/cplay/watch-later/")).append(hash));

    if (f.exists()) {
        f.remove();
    }
}

QString MpvObject::md5(const QString &str)
{
    auto md5 = QCryptographicHash::hash((str.toUtf8()), QCryptographicHash::Md5);

    return QString::fromUtf8(md5.toHex());
}
