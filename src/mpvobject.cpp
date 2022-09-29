/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "_debug.h"
#include "mpvobject.h"
#include "application.h"
#include "audiosettings.h"
#include "generalsettings.h"
#include "playbacksettings.h"
#include "videosettings.h"
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
    obj->window()->resetOpenGLState();

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

    obj->window()->resetOpenGLState();
}

QOpenGLFramebufferObject * MpvRenderer::createFramebufferObject(const QSize &size)
{
    // init mpv_gl:
    if (!obj->mpv_gl)
    {
        mpv_opengl_init_params gl_init_params{get_proc_address_mpv, nullptr, nullptr};
        mpv_render_param params[]{
            {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
            {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };

        if (mpv_render_context_create(&obj->mpv_gl, obj->mpv, params) < 0)
            throw std::runtime_error("failed to initialize mpv GL context");
        mpv_render_context_set_update_callback(obj->mpv_gl, on_mpv_redraw, obj);
        emit obj->ready();
    }

    return QQuickFramebufferObject::Renderer::createFramebufferObject(size);
}

MpvObject::MpvObject(QQuickItem * parent)
    : QQuickFramebufferObject(parent)
    , mpv{mpv_create()}
    , mpv_gl(nullptr)
    , m_audioTracksModel(new TracksModel)
    , m_subtitleTracksModel(new TracksModel)
    , m_playlistModel(new PlayListModel)
{
    if (!mpv)
        throw std::runtime_error("could not create mpv context");

    //setProperty("terminal", "yes");
    //setProperty("msg-level", "all=v");

    QString hwdec = PlaybackSettings::useHWDecoding() ? PlaybackSettings::hWDecoding() : "no";
    setProperty("hwdec", hwdec);

    m_rotationSpeed = VideoSettings::surfaceRotationSpeed();
    m_radius = VideoSettings::surfaceRadius();
    m_fov = VideoSettings::surfaceFov();
    m_angle = VideoSettings::surfaceAngle();
    m_rotate = QVector3D(VideoSettings::surfaceRotateX(), VideoSettings::surfaceRotateY(), VideoSettings::surfaceRotateZ());
    m_translate = QVector3D(VideoSettings::surfaceTranslateX(), VideoSettings::surfaceTranslateY(), VideoSettings::surfaceTranslateZ());
    m_surfaceTransistionOnGoing = false;

    SyncHelper::instance().variables.radius = m_radius;
    SyncHelper::instance().variables.fov = m_fov;
    SyncHelper::instance().variables.angle = m_angle;
    SyncHelper::instance().variables.rotateX = m_rotate.x();
    SyncHelper::instance().variables.rotateY = m_rotate.y();
    SyncHelper::instance().variables.rotateZ = m_rotate.z();
    SyncHelper::instance().variables.translateX = m_translate.x();
    SyncHelper::instance().variables.translateY = m_translate.y();
    SyncHelper::instance().variables.translateZ = m_translate.z();

    QString loadAudioInVidFolder = AudioSettings::loadAudioFileInVideoFolder() ? "all" : "no";
    setProperty("audio-file-auto", loadAudioInVidFolder);
    setProperty("screenshot-template", VideoSettings::screenshotTemplate());
    setProperty("sub-auto", "exact");
    setProperty("volume-max", "100");
    setProperty("keep-open", "yes");

    setStereoscopicVideo(PlaybackSettings::stereoModeOnStartup());
    setGridToMapOn(PlaybackSettings::gridToMapOn());

    mpv::qt::load_configurations(mpv, QStringLiteral("./data/mpv-conf.json"));
    mpv::qt::load_configurations(mpv, QStringLiteral("./data/mpv-conf-master.json"));

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

    if (mpv_initialize(mpv) < 0)
        throw std::runtime_error("could not initialize mpv context");

    mpv_set_wakeup_callback(mpv, MpvObject::mpvEvents, this);

    connect(this, &MpvObject::fileLoaded,
            this, &MpvObject::loadTracks);

    connect(this, &MpvObject::positionChanged, this, [this]() {
        int pos = getProperty("time-pos").toInt();
        double duration = getProperty("duration").toDouble();
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
}

QVariantList MpvObject::audioDevices() const
{
    return m_audioDevices;
}

void MpvObject::setAudioDevices(QVariantList devices)
{
    m_audioDevices = devices;
}

QString MpvObject::mediaTitle()
{
    return getProperty("media-title").toString();
}
double MpvObject::position()
{
    return getProperty("time-pos").toDouble();
}

void MpvObject::setPosition(double value)
{
    SyncHelper::instance().variables.timePosition = value;
    SyncHelper::instance().variables.timeDirty = true;
    m_lastSetPosition = value;
    SyncHelper::instance().variables.timeThreshold = double(PlaybackSettings::thresholdToSyncTimePosition())/1000.0;
    if (value == position()) {
        return;
    }
    setProperty("time-pos", value);
    emit positionChanged();
}

double MpvObject::remaining()
{
    return getProperty("time-remaining").toDouble();
}

double MpvObject::duration()
{
    return getProperty("duration").toDouble();
}

bool MpvObject::pause()
{
    return getProperty("pause").toBool();
}

void MpvObject::setPause(bool value)
{
    SyncHelper::instance().variables.paused = value;
    if (value == pause()) {
        return;
    }
    setProperty("pause", value);
    emit pauseChanged();
}

void MpvObject::togglePlayPause()
{
    SyncHelper::instance().variables.paused = !pause();
    setProperty("pause", SyncHelper::instance().variables.paused);
    emit pauseChanged();
}

int MpvObject::volume()
{
    return getProperty("volume").toInt();
}

void MpvObject::setVolume(int value)
{
    if (value == volume()) {
        return;
    }
    setProperty("volume", value);
    emit volumeChanged();
}

int MpvObject::chapter()
{
    return getProperty("chapter").toInt();
}

void MpvObject::setChapter(int value)
{
    if (value == chapter()) {
        return;
    }
    setProperty("chapter", value);
    emit chapterChanged();
}

int MpvObject::audioId()
{
    return getProperty("aid").toInt();
}

void MpvObject::setAudioId(int value)
{
    if (value == audioId()) {
        return;
    }
    setProperty("aid", value);
    emit audioIdChanged();
}

int MpvObject::subtitleId()
{
    return getProperty("sid").toInt();
}

void MpvObject::setSubtitleId(int value)
{
    if (value == subtitleId()) {
        return;
    }
    setProperty("sid", value);
    emit subtitleIdChanged();
}

int MpvObject::secondarySubtitleId()
{
    return getProperty("secondary-sid").toInt();
}

void MpvObject::setSecondarySubtitleId(int value)
{
    if (value == secondarySubtitleId()) {
        return;
    }
    setProperty("secondary-sid", value);
    emit secondarySubtitleIdChanged();
}

int MpvObject::contrast()
{
    return getProperty("contrast").toInt();
}

void MpvObject::setContrast(int value)
{
    SyncHelper::instance().variables.contrast = value;
    if (value == contrast()) {
        return;
    }
    setProperty("contrast", value);
    emit contrastChanged();
}

int MpvObject::brightness()
{
    return getProperty("brightness").toInt();
}

void MpvObject::setBrightness(int value)
{
    SyncHelper::instance().variables.brightness = value;
    if (value == brightness()) {
        return;
    }
    setProperty("brightness", value);
    emit brightnessChanged();
}

int MpvObject::gamma()
{
    return getProperty("gamma").toInt();
}

void MpvObject::setGamma(int value)
{
    SyncHelper::instance().variables.gamma = value;
    if (value == gamma()) {
        return;
    }
    setProperty("gamma", value);
    emit gammaChanged();
}

int MpvObject::saturation()
{
    return getProperty("saturation").toInt();
}

void MpvObject::setSaturation(int value)
{
    SyncHelper::instance().variables.saturation = value;
    if (value == saturation()) {
        return;
    }
    setProperty("saturation", value);
    emit saturationChanged();
}

bool MpvObject::hwDecoding()
{
    if (getProperty("hwdec") == "yes") {
        return true;
    } else {
        return false;
    }
}

void MpvObject::setHWDecoding(bool value)
{
    if (value) {
        setProperty("hwdec", "yes");
    } else  {
        setProperty("hwdec", "no");
    }
    emit hwDecodingChanged();
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
    emit watchPercentageChanged();
}

int MpvObject::stereoscopicVideo()
{
    return SyncHelper::instance().variables.stereoscopicMode;
}

void MpvObject::setStereoscopicVideo(int value)
{
    SyncHelper::instance().variables.stereoscopicMode = value;
    emit stereoscopicVideoChanged();
}

bool MpvObject::syncVideo()
{
    return SyncHelper::instance().variables.syncOn;
}

void MpvObject::setSyncVideo(bool value)
{
    SyncHelper::instance().variables.syncOn = value;
    emit syncVideoChanged();
}

int MpvObject::visibility()
{
    return int(SyncHelper::instance().variables.alpha*100.f);
}

void MpvObject::setVisibility(int value)
{
    SyncHelper::instance().variables.alpha = float(value)/100.f;
    emit visibilityChanged();
}

int MpvObject::loopMode() {
    return SyncHelper::instance().variables.loopMode;
}

void MpvObject::setLoopMode(int value) {
    SyncHelper::instance().variables.loopMode = value;

    if (value == 0) { //Continue
        setProperty("loop-file", "no");
        setProperty("keep-open", "no");
    }
    else if (value == 1) { //Pause (1)
        setProperty("loop-file", "no");
        setProperty("keep-open", "yes");
    }
    else { //Loop
        setProperty("loop-file", "inf");
        setProperty("keep-open", "yes");
    }
    emit loopModeChanged();
}

int MpvObject::gridToMapOn()
{
    return SyncHelper::instance().variables.gridToMapOn;
}

void MpvObject::setGridToMapOn(int value)
{
    if (SyncHelper::instance().variables.gridToMapOn != value) {
        SyncHelper::instance().variables.gridToMapOn = value;
        emit gridToMapOnChanged();
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
    emit rotationSpeedChanged();
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
    emit radiusChanged();
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
    emit fovChanged();
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
    emit angleChanged();
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
    emit rotateChanged();
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
    emit translateChanged();
}

bool MpvObject::surfaceTransistionOnGoing() {
    return m_surfaceTransistionOnGoing;
}

void MpvObject::setSurfaceTransistionOnGoing(bool value) {
    m_surfaceTransistionOnGoing = value;
    emit surfaceTransistionOnGoingChanged();
}

QVariant MpvObject::getAudioDeviceList()
{
  return getProperty("audio-device-list");
}

void MpvObject::updateAudioDeviceList()
{
  bool preferredDeviceFound = false;
  bool savedFirstDevice = false;
  QString firstDevice;
  QVariantList audioDevicesList;
  QVariant list = getAudioDeviceList();
  for(const QVariant& d : list.toList())
  {
    Q_ASSERT(d.type() == QVariant::Map);
    QVariantMap dmap = d.toMap();

    if(!preferredDeviceFound){
        QString device = dmap["name"].toString();
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
            setProperty("audio-device", AudioSettings::preferredAudioOutputDevice());
          }
          else{
            //AudioSettings::setPreferredAudioOutputDevice(firstDevice);
          }
      }
      else if(AudioSettings::useAudioDriver()){
          setProperty("ao", AudioSettings::preferredAudioOutputDriver());
      }
  }

  emit audioDevicesChanged();
}

QQuickFramebufferObject::Renderer *MpvObject::createRenderer() const
{
    window()->setPersistentOpenGLContext(true);
    window()->setPersistentSceneGraph(true);
    return new MpvRenderer(const_cast<MpvObject *>(this));
}

QString MpvObject::getFileContent(const QString& file)
{
    QString fileToLoad = file;
    fileToLoad.replace("file:///", "");
    qInfo() << "getFileContent: " << fileToLoad;
    QFile f(fileToLoad);
    f.open(QIODevice::ReadOnly);
    QString content = f.readAll();
    f.close();
    return content;
}

void MpvObject::loadFile(const QString &file, bool updateLastPlayedFile)
{
    QFileInfo fi(file);
    QString ext = fi.suffix();

    if (ext == "fdv") {
        PlayListItem* videoFile = loadUniviewFDV(file);
        if(videoFile)
            loadItem(videoFile->data());
    }
    else if (ext == "playlist") {
        loadUniviewPlaylist(file);
    }
    else if (ext == "cplay_file") {
        PlayListItem* videoFile = loadJSONPlayfile(file);
        if (videoFile)
            loadItem(videoFile->data());
    }
    else if (ext == "cplay_playlist") {
        loadJSONPlayList(file);
    }
    else {
        command(QStringList() << "loadfile" << file, true);
        SyncHelper::instance().variables.loadedFile = file.toStdString();
        SyncHelper::instance().variables.overlayFile = "";
        SyncHelper::instance().variables.loadFile = true;
    }

    if (updateLastPlayedFile) {
        GeneralSettings::setLastPlayedFile(file);
        GeneralSettings::self()->save();
    } else {
        GeneralSettings::setLastPlaylistIndex(m_playlistModel->getPlayingVideo());
        GeneralSettings::self()->save();
    }
}

void MpvObject::loadItem(int playListIndex, bool updateLastPlayedFile) {
    try {
        QPointer<PlayListItem> item = m_playlistModel->getItem(playListIndex);
        if (!item) {
            qWarning() << "PlayListItem pointer was null";
            return;
        }
        PlayListItemData d = item->data();
        loadItem(d, updateLastPlayedFile);
    }
    catch (...) {
    }
}

void MpvObject::loadItem(PlayListItemData itemData, bool updateLastPlayedFile, QString flag) {
    try {
        QStringList optionsList;

        if (itemData.separateOverlayFile() != "") {
            optionsList << "external-file=" + itemData.separateOverlayFile();
        }

        if (itemData.separateAudioFile() != "") {
            optionsList << "audio-file=" + itemData.separateAudioFile();
        }

        QString options = "";
        if (optionsList.size() > 0) {
            options = optionsList.join(",");
        }

        QStringList newCommand = QStringList() << "loadfile" << itemData.filePath() << flag << options;
        
        qInfo() << newCommand;
        
        command(newCommand, true);

        if (itemData.separateOverlayFile() != "") {
            setProperty("lavfi-complex", "[vid1][vid2]overlay@myoverlay[vo]");
        }
        else {
            setProperty("lavfi-complex", "");
        }
        
        SyncHelper::instance().variables.loadedFile = itemData.filePath().toStdString();
        SyncHelper::instance().variables.overlayFile = itemData.separateOverlayFile().toStdString();
        SyncHelper::instance().variables.loadFile = true;

        if (itemData.gridToMapOn() >= 0)
            setGridToMapOn(itemData.gridToMapOn());

        if (itemData.stereoVideo() >= 0)
            setStereoscopicVideo(itemData.stereoVideo());

        if (updateLastPlayedFile) {
            GeneralSettings::setLastPlayedFile(itemData.filePath());
            GeneralSettings::self()->save();
        }
        else {
            GeneralSettings::setLastPlaylistIndex(m_playlistModel->getPlayingVideo());
            GeneralSettings::self()->save();
        }
    }
    catch (...) {
    }
}

PlayListItem* MpvObject::loadUniviewFDV(const QString& file)
{
    QString fileContent = getFileContent(file);
    QStringList fileEntries = fileContent.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);
    qInfo() << "File Content: " << fileEntries;

    QString fileMainPath = GeneralSettings::univiewVideoLocation();

    if (!fileEntries.isEmpty()) {
        QString videoFileRelatiePath = fileEntries.at(1).mid(6); //"Video="
        QString videoFile = videoFileRelatiePath;
        if(!fileMainPath.isEmpty())
            videoFile = QDir::cleanPath(fileMainPath + QDir::separator() + videoFileRelatiePath);

        QString audioFileRelatiePath = fileEntries.at(2).mid(6); //"Audio="
        QString audioFile = audioFileRelatiePath;
        if (!fileMainPath.isEmpty())
            audioFile = QDir::cleanPath(fileMainPath + QDir::separator() + audioFileRelatiePath);

        QString title = fileEntries.at(3).mid(6); //"Title="
        double duration = fileEntries.at(4).mid(9).toDouble(); //"Duration="

        auto video = new PlayListItem(videoFile);
        video->setSeparateAudioFile(audioFile);
        video->setMediaTitle(title);
        video->setFileName(title);
        video->setDuration(Application::formatTime(duration));
        video->setGridToMapOn(0); //Assume Pre-split with FDV files
        
        if (title.contains("3D")) // Assume 3D side-by-side stereo
            video->setStereoVideo(1);
        else if (title.contains("2D"))
            video->setStereoVideo(0);

        return video;
    }
    else {
        auto video = new PlayListItem(file);
        return video;
    }
}

void MpvObject::loadUniviewPlaylist(const QString& file)
{
    QString fileContent = getFileContent(file);
    QStringList playListEntries = fileContent.split(QRegExp("[\r\n]"), QString::SkipEmptyParts);

    int videoItems = playListEntries.at(1).mid(14).toInt(); //"NumberOfItems="

    m_playlistModel->clear();
    Playlist m_playList;

    QString fileToLoad = file;
    fileToLoad.replace("file:///", "");
    QFileInfo fileInfo(fileToLoad);

    for (int i = 0; i < videoItems; ++i) {
        int itemStart = (i * 7) + 2;

        QString title = playListEntries.at(itemStart + 1).mid(5); //"Name="
        QString path = playListEntries.at(itemStart + 2).mid(5); //"Path="
        double startTime = playListEntries.at(itemStart + 3).mid(10).toDouble(); //"Starttime="
        double endTime = playListEntries.at(itemStart + 4).mid(8).toDouble(); //"Endtime="
        int loopMode = playListEntries.at(itemStart + 5).mid(9).toInt(); //"Loopmode="
        int transitionMode = playListEntries.at(itemStart + 6).mid(15).toInt(); //"Transitionmode="

        QFileInfo videoFileInfo(path);
        QString videoFileExt = videoFileInfo.suffix();
        PlayListItem* video;

        if (videoFileExt == "fdv") {
            video = loadUniviewFDV(QDir::cleanPath(fileInfo.absoluteDir().absolutePath() + QDir::separator() + path));
        }
        else {
            video = new PlayListItem(path);
            video->setDuration(Application::formatTime(endTime - startTime));
        }
        
        video->setMediaTitle(title);

        if (title.contains("3D")) // Assume 3D side-by-side stereo
            video->setStereoVideo(1);
        else if (title.contains("2D"))
            video->setStereoVideo(0);

        video->setStartTime(startTime);
        video->setEndTime(endTime);
        video->setLoopMode(loopMode);
        video->setTransitionMode(transitionMode);

        m_playList.append(QPointer<PlayListItem>(video));
    }

    // save playlist to disk
    m_playlistModel->setPlayList(m_playList);
    emit playlistModelChanged();
}

PlayListItem* MpvObject::loadJSONPlayfile(const QString& file) {
    QString fileToLoad = file;
    fileToLoad.replace("file:///", "");
    QFileInfo jsonFileInfo(fileToLoad);
    if (!jsonFileInfo.exists())
        return NULL;

    QString fileContent = getFileContent(file);
    QJsonDocument doc = QJsonDocument::fromJson(fileContent.toUtf8());
    if (doc.isNull())
    {
        qDebug() << "Parsing cplay_file failed: " << fileToLoad;
        return NULL;
    }

    QJsonObject obj = doc.object();

    auto item = new PlayListItem(fileToLoad);

    if (obj.contains("video")) {
        QString videoFile = obj.value("video").toString();
        QFileInfo videoFileInfo(videoFile);
        if (videoFileInfo.exists())
            item->setFilePath(videoFile);
        else if(videoFileInfo.isRelative()){ //Check first if next to JSON file
            QString videoFileNextToJSONfile = QDir::cleanPath(jsonFileInfo.absoluteDir().absolutePath() + QDir::separator() + videoFile);
            QFileInfo videoFileInfoNextToJsonFile(videoFileNextToJSONfile);
            if (videoFileInfoNextToJsonFile.exists())
                item->setFilePath(videoFileNextToJSONfile);
            else { // Check if stored in Uniview video folder
                QString fileUniviewMainPath = GeneralSettings::univiewVideoLocation();
                QString videoFileAbsoluteUniview = QDir::cleanPath(fileUniviewMainPath + QDir::separator() + videoFile);
                QFileInfo videoFileInfoUniview(videoFileAbsoluteUniview);
                if (videoFileInfoUniview.exists())
                    item->setFilePath(videoFileAbsoluteUniview);
            }
        }
    }

    if (obj.contains("overlay")) {
        QString overlayFile = obj.value("overlay").toString();
        QFileInfo overlayFileInfo(overlayFile);
        if (overlayFileInfo.exists())
            item->setSeparateOverlayFile(overlayFile);
        else if (overlayFileInfo.isRelative()) { //Check first if next to JSON file
            QString overlayFileNextToJSONfile = QDir::cleanPath(jsonFileInfo.absoluteDir().absolutePath() + QDir::separator() + overlayFile);
            QFileInfo overlayFileInfoNextToJsonFile(overlayFileNextToJSONfile);
            if (overlayFileInfoNextToJsonFile.exists())
                item->setSeparateOverlayFile(overlayFileNextToJSONfile);
            else { // Check if stored in Uniview video folder
                QString fileUniviewMainPath = GeneralSettings::univiewVideoLocation();
                QString overlayFileAbsoluteUniview = QDir::cleanPath(fileUniviewMainPath + QDir::separator() + overlayFile);
                QFileInfo overlayFileInfoUniview(overlayFileAbsoluteUniview);
                if (overlayFileInfoUniview.exists())
                    item->setSeparateOverlayFile(overlayFileAbsoluteUniview);
            }
        }
    }

    if (obj.contains("audio")) {
        QString audioFile = obj.value("audio").toString();
        QFileInfo audioFileInfo(audioFile);
        if (audioFileInfo.exists())
            item->setSeparateAudioFile(audioFile);
        else if (audioFileInfo.isRelative()) { //Check first if next to JSON file
            QString audioFileNextToJSONfile = QDir::cleanPath(jsonFileInfo.absoluteDir().absolutePath() + QDir::separator() + audioFile);
            QFileInfo audioFileInfoNextToJsonFile(audioFileNextToJSONfile);
            if (audioFileInfoNextToJsonFile.exists())
                item->setSeparateAudioFile(audioFileNextToJSONfile);
            else { // Check if stored in Uniview video folder
                QString fileUniviewMainPath = GeneralSettings::univiewVideoLocation();
                QString audioFileAbsoluteUniview = QDir::cleanPath(fileUniviewMainPath + QDir::separator() + audioFile);
                QFileInfo audioFileInfoUniview(audioFileAbsoluteUniview);
                if (audioFileInfoUniview.exists())
                    item->setSeparateAudioFile(audioFileAbsoluteUniview);
            }
        }
    }

    if (obj.contains("title")) {
        QString title = obj.value("title").toString();
        item->setMediaTitle(title);
        item->setFileName(title);
    }

    if (obj.contains("duration")) {
        double duration = obj.value("duration").toDouble();
        item->setDuration(Application::formatTime(duration));
    }

    if (obj.contains("grid")) {
        QString grid = obj.value("grid").toString();
        if (grid == "pre-split") {
            item->setGridToMapOn(0);
        }
        else if (grid == "dome") {
            item->setGridToMapOn(1);
        }
        else if (grid == "sphere") {
            item->setGridToMapOn(2);
        }
        else if (grid == "sphere-eqr") {
            item->setGridToMapOn(2);
        }
        else if (grid == "sphere-eac") {
            item->setGridToMapOn(3);
        }
    }

    if (obj.contains("stereoscopic")) {
        QString stereo = obj.value("stereoscopic").toString();
        if (stereo == "no") {
            item->setStereoVideo(0);
        }
        else if (stereo == "mono") {
            item->setStereoVideo(0);
        }
        else if (stereo == "yes") { //Assume side-by-side if yes
            item->setStereoVideo(1);
        }
        else if (stereo == "sbs") {
            item->setStereoVideo(1);
        }
        else if (stereo == "tb") {
            item->setStereoVideo(2);
        }
    }

    return item;
}

void MpvObject::loadJSONPlayList(const QString& file) {
    QString fileToLoad = file;
    fileToLoad.replace("file:///", "");
    QFileInfo jsonFileInfo(fileToLoad);
    if (!jsonFileInfo.exists())
        return;

    QString fileContent = getFileContent(file);
    QJsonDocument doc = QJsonDocument::fromJson(fileContent.toUtf8());
    if (doc.isNull())
    {
        qDebug() << "Parsing cplay_playlist failed: " << fileToLoad;
        return;
    }

    QJsonObject obj = doc.object();
    QJsonValue value = obj.value("playlist");
    QJsonArray array = value.toArray();

    m_playlistModel->clear();
    Playlist m_playList;

    foreach(const QJsonValue & v, array) {
        QJsonObject o = v.toObject();
        if (o.contains("file")) {
            QString filePath = o.value("file").toString();
            QFileInfo filePathInfo(filePath);
            if (filePathInfo.isRelative()) { //Check first if next to JSON file
                QString videoFileNextToJSONfile = QDir::cleanPath(jsonFileInfo.absoluteDir().absolutePath() + QDir::separator() + filePath);
                QFileInfo videoFileInfoNextToJsonFile(videoFileNextToJSONfile);
                if (videoFileInfoNextToJsonFile.exists())
                    filePath = videoFileNextToJSONfile;
                else { // Check if stored in Uniview video folder
                    QString fileUniviewMainPath = GeneralSettings::univiewVideoLocation();
                    QString videoFileAbsoluteUniview = QDir::cleanPath(fileUniviewMainPath + QDir::separator() + filePath);
                    QFileInfo videoFileInfoUniview(videoFileAbsoluteUniview);
                    if (videoFileInfoUniview.exists())
                        filePath = videoFileAbsoluteUniview;
                }
            }

            QString videoFileExt = filePathInfo.suffix();
            PlayListItem* video;

            if (videoFileExt == "cplay_file") {
                video = loadJSONPlayfile(filePath);
            }
            else if (videoFileExt == "fdv") {
                video = loadUniviewFDV(filePath);
            }
            else {
                video = new PlayListItem(filePath);
            }

            if (video != NULL) {
                if (o.contains("on_file_end")) {
                    QString loopMode = o.value("on_file_end").toString();
                    if (loopMode == "continue") {
                        video->setLoopMode(0);
                    }
                    else if (loopMode == "pause") {
                        video->setLoopMode(1);
                    }
                    else if (loopMode == "loop") {
                        video->setLoopMode(2);
                    }
                }

                m_playList.append(QPointer<PlayListItem>(video));
            }
            else
                qDebug() << "Parsing file failed: " << filePath;
        }
    }

    // save playlist to disk
    m_playlistModel->setPlayList(m_playList);
    emit playlistModelChanged();
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
            emit fileStarted();
            break;
        }
        case MPV_EVENT_FILE_LOADED: {
            emit fileLoaded();
            break;
        }
        case MPV_EVENT_END_FILE: {
            auto prop = (mpv_event_end_file *)event->data;
            if (prop->reason == MPV_END_FILE_REASON_EOF) {
                emit endFile("eof");
            } else if(prop->reason == MPV_END_FILE_REASON_ERROR) {
                emit endFile("error");
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
                    double latestPosition = position();
                    SyncHelper::instance().variables.timePosition = latestPosition;
                    SyncHelper::instance().variables.paused = pause();
                    if(PlaybackSettings::intervalToSetPosition() > 0){
                        if(abs(latestPosition - m_lastSetPosition)*1000 > PlaybackSettings::intervalToSetPosition()){
                            m_lastSetPosition = SyncHelper::instance().variables.timePosition;
                            SyncHelper::instance().variables.timeDirty = true;
                        }
                        else{
                            SyncHelper::instance().variables.timeThreshold = double(PlaybackSettings::thresholdToSyncTimePosition())/1000.0;
                        }
                    }
                    else{
                        SyncHelper::instance().variables.timeThreshold = double(PlaybackSettings::thresholdToSyncTimePosition())/1000.0;
                    }
                    emit positionChanged();
                }
            } else if (strcmp(prop->name, "media-title") == 0) {
                if (prop->format == MPV_FORMAT_STRING) {
                    emit mediaTitleChanged();
                }
            } else if (strcmp(prop->name, "time-remaining") == 0) {
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    emit remainingChanged();
                }
            } else if (strcmp(prop->name, "duration") == 0) {
                if (prop->format == MPV_FORMAT_DOUBLE) {
                    emit durationChanged();
                }
            } else if (strcmp(prop->name, "volume") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    emit volumeChanged();
                }
            } else if (strcmp(prop->name, "pause") == 0) {
                if (prop->format == MPV_FORMAT_FLAG) {
                    //m_lastSetPosition = position();
                    SyncHelper::instance().variables.paused = pause();
                    emit pauseChanged();
                }
            } else if (strcmp(prop->name, "chapter") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    emit chapterChanged();
                }
            } else if (strcmp(prop->name, "aid") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    emit audioIdChanged();
                }
            } else if (strcmp(prop->name, "sid") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    emit subtitleIdChanged();
                }
            } else if (strcmp(prop->name, "secondary-sid") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    emit secondarySubtitleIdChanged();
                }
            } else if (strcmp(prop->name, "contrast") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    emit contrastChanged();
                }
            } else if (strcmp(prop->name, "brightness") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    emit brightnessChanged();
                }
            } else if (strcmp(prop->name, "gamma") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    emit gammaChanged();
                }
            } else if (strcmp(prop->name, "saturation") == 0) {
                if (prop->format == MPV_FORMAT_INT64) {
                    emit saturationChanged();
                }
            } else if (strcmp(prop->name, "track-list") == 0) {
                if (prop->format == MPV_FORMAT_NODE) {
                    loadTracks();
                }
            }
            break;
        }
        default: ;
            // Ignore uninteresting or unknown events.
        }
    }
}

void MpvObject::performSurfaceTransistion() {
    emit surfaceTransistionPerformed();
}

void MpvObject::loadTracks()
{
    m_subtitleTracks.clear();
    m_audioTracks.clear();

    auto none = new Track();
    none->setId(0);
    none->setTitle("None");
    m_subtitleTracks.insert(0, none);

    const QList<QVariant> tracks = getProperty("track-list").toList();
    int subIndex = 1;
    int audioIndex = 0;
    for (const auto &track : tracks) {
        const auto t = track.toMap();
        if (track.toMap()["type"] == "sub") {
            auto track = new Track();
            track->setCodec(t["codec"].toString());
            track->setType(t["type"].toString());
            track->setDefaut(t["default"].toBool());
            track->setDependent(t["dependent"].toBool());
            track->setForced(t["forced"].toBool());
            track->setId(t["id"].toLongLong());
            track->setSrcId(t["src-id"].toLongLong());
            track->setFfIndex(t["ff-index"].toLongLong());
            track->setLang(t["lang"].toString());
            track->setTitle(t["title"].toString());
            track->setIndex(subIndex);

            m_subtitleTracks.insert(subIndex, track);
            subIndex++;
        }
        if (track.toMap()["type"] == "audio") {
            auto track = new Track();
            track->setCodec(t["codec"].toString());
            track->setType(t["type"].toString());
            track->setDefaut(t["default"].toBool());
            track->setDependent(t["dependent"].toBool());
            track->setForced(t["forced"].toBool());
            track->setId(t["id"].toLongLong());
            track->setSrcId(t["src-id"].toLongLong());
            track->setFfIndex(t["ff-index"].toLongLong());
            track->setLang(t["lang"].toString());
            track->setTitle(t["title"].toString());
            track->setIndex(audioIndex);

            m_audioTracks.insert(audioIndex, track);
            audioIndex++;
        }
    }
    m_subtitleTracksModel->setTracks(m_subtitleTracks);
    m_audioTracksModel->setTracks(m_audioTracks);

    emit audioTracksModelChanged();
    emit subtitleTracksModelChanged();
}

TracksModel *MpvObject::subtitleTracksModel() const
{
    return m_subtitleTracksModel;
}

TracksModel *MpvObject::audioTracksModel() const
{
    return m_audioTracksModel;
}

void MpvObject::getYouTubePlaylist(const QString &path)
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

        emit youtubePlaylistLoaded();
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
    if (getProperty("duration").toInt() < PlaybackSettings::minDurationToSavePosition() * 60) {
        return;
    }

    auto hash = md5(getProperty("path").toString());
    auto timePosition = getProperty("time-pos");
    auto configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    KConfig *config = new KConfig(configPath.append("/georgefb/watch-later/").append(hash));
    config->group("").writeEntry("TimePosition", timePosition);
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
    if (getProperty("duration").toInt() < PlaybackSettings::minDurationToSavePosition() * 60) {
        return 0;
    }

    auto hash = md5(getProperty("path").toString());
    auto configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    KConfig *config = new KConfig(configPath.append("/georgefb/watch-later/").append(hash));
    int position = config->group("").readEntry("TimePosition", QString::number(0)).toDouble();

    return position;
}

void MpvObject::resetTimePosition()
{
    auto hash = md5(getProperty("path").toString());
    auto configPath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    QFile f(configPath.append("/georgefb/watch-later/").append(hash));

    if (f.exists()) {
        f.remove();
    }
}

QString MpvObject::md5(const QString &str)
{
    auto md5 = QCryptographicHash::hash((str.toUtf8()), QCryptographicHash::Md5);

    return QString(md5.toHex());
}
