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

    m_radius = VideoSettings::domeRadius();
    m_fov = VideoSettings::domeFov();
    m_rotateX = VideoSettings::domeRotateX();
    m_rotateY = VideoSettings::domeRotateY();
    m_rotateZ = VideoSettings::domeRotateZ();

    SyncHelper::instance().variables.radius = m_radius;
    SyncHelper::instance().variables.fov = m_fov;
    SyncHelper::instance().variables.rotateX = m_rotateX;
    SyncHelper::instance().variables.rotateY = m_rotateY;
    SyncHelper::instance().variables.rotateZ = m_rotateZ;

    QString loadAudioInVidFolder = AudioSettings::loadAudioFileInVideoFolder() ? "all" : "no";
    setProperty("audio-file-auto", loadAudioInVidFolder);
    setProperty("screenshot-template", VideoSettings::screenshotTemplate());
    setProperty("sub-auto", "exact");
    setProperty("volume-max", "100");

    SyncHelper::instance().variables.sbs3DVideo = PlaybackSettings::stereoModeOnStartup();
    SyncHelper::instance().variables.gridToMapOn = PlaybackSettings::gridToMapOn();

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

bool MpvObject::stereoscopicVideo()
{
    return SyncHelper::instance().variables.sbs3DVideo;
}

void MpvObject::setStereoscopicVideo(bool value)
{
    SyncHelper::instance().variables.sbs3DVideo = value;
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

int MpvObject::gridToMapOn()
{
    return SyncHelper::instance().variables.gridToMapOn;
}

void MpvObject::setGridToMapOn(int value)
{
    SyncHelper::instance().variables.gridToMapOn = value;
    emit gridToMapOnChanged();
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

int MpvObject::rotateX()
{
    return m_rotateX;
}

void MpvObject::setRotateX(int value)
{
    SyncHelper::instance().variables.rotateX = value;
    if (m_rotateX == value) {
        return;
    }
    m_rotateX = value;
    emit rotateXChanged();
}

int MpvObject::rotateY()
{
    return m_rotateY;
}

void MpvObject::setRotateY(int value)
{
    SyncHelper::instance().variables.rotateY = value;
    if (m_rotateY == value) {
        return;
    }
    m_rotateY = value;
    emit rotateYChanged();
}

int MpvObject::rotateZ()
{
    return m_rotateZ;
}

void MpvObject::setRotateZ(int value)
{
    SyncHelper::instance().variables.rotateZ = value;
    if (m_rotateZ == value) {
        return;
    }
    m_rotateZ = value;
    emit rotateZChanged();
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

void MpvObject::loadFile(const QString &file, bool updateLastPlayedFile)
{
    command(QStringList() << "loadfile" << file);
    SyncHelper::instance().variables.loadedFile = file.toStdString();

    /*if(AudioSettings::loadAudioFileInVideoFolder()){
        QString filePath = file;
        filePath.replace("file:///", "");
        QDir directory = QFileInfo(filePath).absoluteDir();
        QFileInfoList audioFiles = directory.entryInfoList(QStringList() << "*.wav" << "*.mp3", QDir::Files);
        foreach(QFileInfo audioFile, audioFiles) {
            command(QStringList() << "audio-add" << audioFile.absoluteFilePath());
        }
    }*/

    if (updateLastPlayedFile) {
        GeneralSettings::setLastPlayedFile(file);
        GeneralSettings::self()->save();
    } else {
        GeneralSettings::setLastPlaylistIndex(m_playlistModel->getPlayingVideo());
        GeneralSettings::self()->save();
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
                    SyncHelper::instance().variables.timePosition = position();
                    if(PlaybackSettings::intervalToSetPosition() > 0){
                        if(SyncHelper::instance().variables.timePosition > m_lastSetPosition + PlaybackSettings::intervalToSetPosition()){
                            m_lastSetPosition = SyncHelper::instance().variables.timePosition;
                            SyncHelper::instance().variables.timeThreshold = 0.0;
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
                    m_lastSetPosition = position();
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
    m_playlistModel->clear();

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
    });
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

QVariant MpvObject::command(const QVariant &params)
{
    return mpv::qt::command(mpv, params);
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
