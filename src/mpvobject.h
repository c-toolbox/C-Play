/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MPVOBJECT_H
#define MPVOBJECT_H

#include <QtQuick/QQuickFramebufferObject>

#include <client.h>
#include <render_gl.h>
#include "qthelper.h"
#include "playlistmodel.h"
#include "tracksmodel.h"

class MpvRenderer;
class Track;

class MpvObject : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(TracksModel* audioTracksModel READ audioTracksModel NOTIFY audioTracksModelChanged)
    Q_PROPERTY(TracksModel* subtitleTracksModel READ subtitleTracksModel NOTIFY subtitleTracksModelChanged)

    Q_PROPERTY(QString mediaTitle
               READ mediaTitle
               NOTIFY mediaTitleChanged)

    Q_PROPERTY(double position
               READ position
               WRITE setPosition
               NOTIFY positionChanged)

    Q_PROPERTY(double duration
               READ duration
               NOTIFY durationChanged)

    Q_PROPERTY(double remaining
               READ remaining
               NOTIFY remainingChanged)

    Q_PROPERTY(bool pause
               READ pause
               WRITE setPause
               NOTIFY pauseChanged)

    Q_PROPERTY(int volume
               READ volume
               WRITE setVolume
               NOTIFY volumeChanged)

    Q_PROPERTY(int chapter
               READ chapter
               WRITE setChapter
               NOTIFY chapterChanged)

    Q_PROPERTY(int audioId
               READ audioId
               WRITE setAudioId
               NOTIFY audioIdChanged)

    Q_PROPERTY(int subtitleId
               READ subtitleId
               WRITE setSubtitleId
               NOTIFY subtitleIdChanged)

    Q_PROPERTY(int secondarySubtitleId
               READ secondarySubtitleId
               WRITE setSecondarySubtitleId
               NOTIFY secondarySubtitleIdChanged)

    Q_PROPERTY(int contrast
               READ contrast
               WRITE setContrast
               NOTIFY contrastChanged)

    Q_PROPERTY(int brightness
               READ brightness
               WRITE setBrightness
               NOTIFY brightnessChanged)

    Q_PROPERTY(int gamma
               READ gamma
               WRITE setGamma
               NOTIFY gammaChanged)

    Q_PROPERTY(int saturation
               READ saturation
               WRITE setSaturation
               NOTIFY saturationChanged)

    Q_PROPERTY(double watchPercentage
               MEMBER m_watchPercentage
               READ watchPercentage
               WRITE setWatchPercentage
               NOTIFY watchPercentageChanged)

    Q_PROPERTY(bool hwDecoding
               READ hwDecoding
               WRITE setHWDecoding
               NOTIFY hwDecodingChanged)

    Q_PROPERTY(bool stereoscopicVideo
               READ stereoscopicVideo
               WRITE setStereoscopicVideo
               NOTIFY stereoscopicVideoChanged)

    Q_PROPERTY(bool syncVideo
            READ syncVideo
            WRITE setSyncVideo
            NOTIFY syncVideoChanged)

    Q_PROPERTY(int visibility
               READ visibility
               WRITE setVisibility
               NOTIFY visibilityChanged)

    Q_PROPERTY(int gridToMapOn
               READ gridToMapOn
               WRITE setGridToMapOn
               NOTIFY gridToMapOnChanged)

    Q_PROPERTY(double radius
               MEMBER m_radius
               READ radius
               WRITE setRadius
               NOTIFY radiusChanged)

    Q_PROPERTY(double fov
               MEMBER m_fov
               READ fov
               WRITE setFov
               NOTIFY fovChanged)

    Q_PROPERTY(double rotateX
               MEMBER m_rotateX
               READ rotateX
               WRITE setRotateX
               NOTIFY rotateXChanged)

    Q_PROPERTY(double rotateY
               MEMBER m_rotateY
               READ rotateY
               WRITE setRotateY
               NOTIFY rotateYChanged)

    Q_PROPERTY(double rotateZ
               MEMBER m_rotateZ
               READ rotateZ
               WRITE setRotateZ
               NOTIFY rotateZChanged)

    Q_PROPERTY(PlayListModel* playlistModel
               READ playlistModel
               WRITE setPlaylistModel
               NOTIFY playlistModelChanged)

    PlayListModel *playlistModel();
    void setPlaylistModel(PlayListModel *model);

    Q_PROPERTY(QVariantList audioDevices
               READ audioDevices
               WRITE setAudioDevices
               NOTIFY audioDevicesChanged)

    QVariantList audioDevices() const;
    void setAudioDevices(QVariantList devices);

    QString mediaTitle();

    double position();
    void setPosition(double value);

    double remaining();
    double duration();

    bool pause();
    void setPause(bool value);
    
    int volume();
    void setVolume(int value);

    int chapter();
    void setChapter(int value);

    int audioId();
    void setAudioId(int value);

    int subtitleId();
    void setSubtitleId(int value);

    int secondarySubtitleId();
    void setSecondarySubtitleId(int value);

    int contrast();
    void setContrast(int value);

    int brightness();
    void setBrightness(int value);

    int gamma();
    void setGamma(int value);

    int saturation();
    void setSaturation(int value);

    double watchPercentage();
    void setWatchPercentage(double value);

    bool hwDecoding();
    void setHWDecoding(bool value);

    bool stereoscopicVideo();
    void setStereoscopicVideo(bool value);

    bool syncVideo();
    void setSyncVideo(bool value);

    int visibility();
    void setVisibility(int value);

    int gridToMapOn();
    void setGridToMapOn(int value);

    double radius();
    void setRadius(double value);

    double fov();
    void setFov(double value);

    int rotateX();
    void setRotateX(int value);

    int rotateY();
    void setRotateY(int value);

    int rotateZ();
    void setRotateZ(int value);

    QVariant getAudioDeviceList();
    void updateAudioDeviceList();

    mpv_handle *mpv;
    mpv_render_context *mpv_gl;

    friend class MpvRenderer;

public:
    MpvObject(QQuickItem * parent = 0);
    virtual ~MpvObject();
    virtual Renderer *createRenderer() const;

    Q_INVOKABLE void loadFile(const QString &file, bool updateLastPlayedFile = true);
    Q_INVOKABLE void getYouTubePlaylist(const QString &path);
    Q_INVOKABLE QVariant command(const QVariant &params);
    Q_INVOKABLE QVariant getProperty(const QString &name, bool debug = false);
    Q_INVOKABLE int setProperty(const QString &name, const QVariant &value, bool debug = false);
    Q_INVOKABLE void saveTimePosition();
    Q_INVOKABLE double loadTimePosition();
    Q_INVOKABLE void resetTimePosition();

public slots:
    static void mpvEvents(void *ctx);
    void eventHandler();

signals:
    void mediaTitleChanged();
    void positionChanged();
    void durationChanged();
    void remainingChanged();
    void volumeChanged();
    void pauseChanged();
    void chapterChanged();
    void audioIdChanged();
    void subtitleIdChanged();
    void secondarySubtitleIdChanged();
    void contrastChanged();
    void brightnessChanged();
    void gammaChanged();
    void saturationChanged();
    void fileStarted();
    void fileLoaded();
    void endFile(QString reason);
    void watchPercentageChanged();
    void ready();
    void audioTracksModelChanged();
    void subtitleTracksModelChanged();
    void hwDecodingChanged();
    void stereoscopicVideoChanged();
    void syncVideoChanged();
    void visibilityChanged();
    void gridToMapOnChanged();
    void radiusChanged();
    void fovChanged();
    void rotateXChanged();
    void rotateYChanged();
    void rotateZChanged();
    void playlistModelChanged();
    void audioDevicesChanged();
    void youtubePlaylistLoaded();

private:
    TracksModel *audioTracksModel() const;
    TracksModel *subtitleTracksModel() const;
    TracksModel *m_audioTracksModel;
    TracksModel *m_subtitleTracksModel;
    QMap<int, Track*> m_subtitleTracks;
    QMap<int, Track*> m_audioTracks;
    QList<int> m_secondsWatched;
    double m_watchPercentage;
    double m_radius;
    double m_fov;
    int m_rotateX;
    int m_rotateY;
    int m_rotateZ;
    double m_lastSetPosition;
    PlayListModel *m_playlistModel;
    QString m_file;
    QVariantList m_audioDevices;

    void loadTracks();
    QString md5(const QString &str);
};

class MpvRenderer : public QQuickFramebufferObject::Renderer
{
public:
    MpvRenderer(MpvObject *new_obj);
    ~MpvRenderer() = default;

    MpvObject *obj;

    // This function is called when a new FBO is needed.
    // This happens on the initial frame.
    QOpenGLFramebufferObject * createFramebufferObject(const QSize &size);

    void render();
};

#endif // MPVOBJECT_H
