/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MPVOBJECT_H
#define MPVOBJECT_H

#include <QQuickFramebufferObject>
#include <QVector3D>

#include "playlistitem.h"
#include "playlistmodel.h"
#include "qthelper.h"
#include "tracksmodel.h"
#include <client.h>
#include <render_gl.h>

class MpvView;
class MpvRenderer;
class Track;

class MpvObject : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

public:
    Q_PROPERTY(TracksModel *audioTracksModel READ audioTracksModel NOTIFY audioTracksModelChanged)
    Q_PROPERTY(TracksModel *subtitleTracksModel READ subtitleTracksModel NOTIFY subtitleTracksModelChanged)

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

    Q_PROPERTY(bool autoPlay
                   READ autoPlay
                       WRITE setAutoPlay
                           NOTIFY autoPlayChanged)

    Q_PROPERTY(int volume
                   READ volume
                       WRITE setVolume
                           NOTIFY volumeChanged)

    Q_PROPERTY(bool mute
                    READ mute
                       WRITE setMute
                            NOTIFY muteChanged)

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

    Q_PROPERTY(int stereoscopicMode
                   READ stereoscopicMode
                       WRITE setStereoscopicMode
                           NOTIFY stereoscopicModeChanged)

    Q_PROPERTY(bool syncVolumeVisibilityFading
                   READ syncVolumeVisibilityFading
                       WRITE setSyncVolumeVisibilityFading
                           NOTIFY syncVolumeVisibilityFadingChanged)

    Q_PROPERTY(int visibility
                   READ visibility
                       WRITE setVisibility
                           NOTIFY visibilityChanged)

    Q_PROPERTY(int eofMode
                   READ eofMode
                       WRITE setEofMode
                           WRITE setEofMode
                               NOTIFY eofModeChanged)

    Q_PROPERTY(int gridToMapOn
                   READ gridToMapOn
                       WRITE setGridToMapOn
                           NOTIFY gridToMapOnChanged)

    Q_PROPERTY(double rotationSpeed
                   MEMBER m_rotationSpeed
                       READ rotationSpeed
                           WRITE setRotationSpeed
                               NOTIFY rotationSpeedChanged)

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

    Q_PROPERTY(double angle
                   MEMBER m_angle
                       READ angle
                           WRITE setAngle
                               NOTIFY angleChanged)

    Q_PROPERTY(QVector3D rotate
                   MEMBER m_rotate
                       READ rotate
                           WRITE setRotate
                               NOTIFY rotateChanged)

    Q_PROPERTY(QVector3D translate
                   MEMBER m_translate
                       READ translate
                           WRITE setTranslate
                               NOTIFY translateChanged)

    Q_PROPERTY(double planeWidth
                   MEMBER m_planeWidth
                       READ planeWidth
                           WRITE setPlaneWidth
                               NOTIFY planeChanged)

    Q_PROPERTY(double planeHeight
                   MEMBER m_planeHeight
                       READ planeHeight
                           WRITE setPlaneHeight
                               NOTIFY planeChanged)

    Q_PROPERTY(double planeElevation
                   MEMBER m_planeElevation
                       READ planeElevation
                           WRITE setPlaneElevation
                               NOTIFY planeChanged)

    Q_PROPERTY(double planeDistance
                   MEMBER m_planeDistance
                       READ planeDistance
                           WRITE setPlaneDistance
                               NOTIFY planeChanged)

    Q_PROPERTY(int planeConsiderAspectRatio
                   MEMBER m_planeConsiderAspectRatio
                       READ planeConsiderAspectRatio
                           WRITE setPlaneConsiderAspectRatio
                               NOTIFY planeChanged)

    Q_PROPERTY(int surfaceTransitionTime
                   MEMBER m_surfaceTransitionTime
                       READ surfaceTransitionTime
                           WRITE setSurfaceTransitionTime
                               NOTIFY surfaceTransitionTimeChanged)

    Q_PROPERTY(bool surfaceTransitionOnGoing
                   MEMBER m_surfaceTransitionOnGoing
                       READ surfaceTransitionOnGoing
                           WRITE setSurfaceTransitionOnGoing
                               NOTIFY surfaceTransitionOnGoingChanged)

    Q_PROPERTY(PlayListModel *playlistModel
                   READ playlistModel
                       WRITE setPlaylistModel
                           NOTIFY playlistModelChanged)

    PlayListModel *playlistModel();
    void setPlaylistModel(PlayListModel *model);

    Q_PROPERTY(PlaySectionsModel *playSectionsModel
                   READ playSectionsModel
                       WRITE setPlaySectionsModel
                           NOTIFY playSectionsModelChanged)

    PlaySectionsModel *playSectionsModel();
    void setPlaySectionsModel(PlaySectionsModel *model);

    Q_PROPERTY(QVariantList audioDevices
                   READ audioDevices
                       WRITE setAudioDevices
                           NOTIFY audioDevicesChanged)

    QVariantList audioDevices() const;
    void setAudioDevices(QVariantList devices);

    Q_PROPERTY(QStringList recentMediaFiles
                   READ recentMediaFiles
                       WRITE setRecentMediaFiles
                           NOTIFY recentMediaFilesChanged)

    QStringList recentMediaFiles() const;
    void setRecentMediaFiles(QStringList list);

    Q_PROPERTY(QStringList recentPlaylists
                   READ recentPlaylists
                       WRITE setRecentPlaylists
                           NOTIFY recentPlaylistsChanged)

    QStringList recentPlaylists() const;
    void setRecentPlaylists(QStringList list);

    QString mediaTitle();
    QString separateAudioFile();

    double position();
    void setPosition(double value);

    double remaining();
    double duration();

    bool pause();
    void setPause(bool value);

    bool autoPlay();
    void setAutoPlay(bool value);

    double speed();
    void setSpeed(double factor);

    int volume();
    void setVolume(int value);

    bool mute();
    void setMute(bool value);

    int chapter();
    void setChapter(int value);

    int audioId();
    void setAudioId(int value);

    int subtitleId();
    void setSubtitleId(int value);

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

    bool syncVolumeVisibilityFading();
    void setSyncVolumeVisibilityFading(bool value);

    int visibility();
    void setVisibility(int value);

    int eofMode();
    void setEofMode(int value);

    int stereoscopicMode();
    void setStereoscopicMode(int value);

    int gridToMapOn();
    void setGridToMapOn(int value);

    double rotationSpeed();
    void setRotationSpeed(double value);

    double radius();
    void setRadius(double value);

    double fov();
    void setFov(double value);

    double angle();
    void setAngle(double value);

    QVector3D rotate();
    void setRotate(QVector3D value);

    QVector3D translate();
    void setTranslate(QVector3D value);

    double planeWidth();
    void setPlaneWidth(double value, bool updatePlane = true);

    double planeHeight();
    void setPlaneHeight(double value, bool updatePlane = true);

    double planeElevation();
    void setPlaneElevation(double value);

    double planeDistance();
    void setPlaneDistance(double value);

    int planeConsiderAspectRatio();
    void setPlaneConsiderAspectRatio(int value);

    int surfaceTransitionTime();
    void setSurfaceTransitionTime(int value);

    bool surfaceTransitionOnGoing();
    void setSurfaceTransitionOnGoing(bool value);

    QVariant getAudioDeviceList();
    void updateAudioDeviceList();

    MpvObject(QQuickItem *parent = 0);
    virtual ~MpvObject();

    TracksModel *audioTracksModel() const;
    TracksModel* subtitleTracksModel() const;

    PlayListModel *getPlayListModel() const;
    PlaySectionsModel *getPlaySectionsModel() const;

    Q_INVOKABLE void updateAudioOutput();
    Q_INVOKABLE void enableAudioOnNodes(bool enabled);
    Q_INVOKABLE void setLoadAudioInVidFolder(bool enabled);
    Q_INVOKABLE QString checkAndCorrectPath(const QString &filePath, const QStringList &searchPaths);
    Q_INVOKABLE void loadFile(const QString &file, bool updateLastPlayedFile = true);
    Q_INVOKABLE void addFileToPlaylist(const QString &file);
    Q_INVOKABLE void clearPlaylist();
    Q_INVOKABLE void setLoadedAsCurrentEditItem();
    Q_INVOKABLE void loadSection(int playSectionsIndex);
    Q_INVOKABLE void loadItem(int playListIndex, bool updateLastPlayedFile = true);
    Q_INVOKABLE QUrl getOverlayFileUrl() const;
    Q_INVOKABLE QString getReadableExternalConfiguration();
    Q_INVOKABLE QVariant command(const QVariant &params, bool debug = false);
    Q_INVOKABLE QVariant getProperty(const QString &name, bool debug = false);
    Q_INVOKABLE int setProperty(const QString &name, const QVariant &value, bool debug = false);
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void clearRecentMediaFilelist();
    Q_INVOKABLE void clearRecentPlaylist();
    Q_INVOKABLE void performRewind();
    Q_INVOKABLE void seek(int timeInSec);
    Q_INVOKABLE static void mpvEvents(void *ctx);
    Q_INVOKABLE void eventHandler();
    Q_INVOKABLE void performSurfaceTransition();
    Q_INVOKABLE void setSubtitleFont(const QString& subFont);
    Q_INVOKABLE void setSubtitleFontSize(int subSize);
    Q_INVOKABLE void setSubtitleTextureSize(int width, int height);
    Q_INVOKABLE void setSubtitleAlignment(int alignment);
    Q_INVOKABLE QString setSubtitleColor(const QColor& subColor);
    Q_INVOKABLE void setSubtitleRelativePlaneElevation(double value);
    Q_INVOKABLE void setSubtitleRelativePlaneDistance(double value);

Q_SIGNALS:
    void mediaTitleChanged();
    void positionChanged();
    void durationChanged();
    void remainingChanged();
    void speedChanged();
    void volumeChanged();
    void volumeUpdate(int);
    void muteChanged();
    void pauseChanged();
    void autoPlayChanged();
    void chapterChanged();
    void audioIdChanged();
    void subtitleIdChanged();
    void contrastChanged();
    void brightnessChanged();
    void gammaChanged();
    void saturationChanged();
    void fileStarted();
    void fileLoaded();
    void sectionLoaded(int);
    void endFile(QString reason);
    void watchPercentageChanged();
    void ready();
    void audioTracksModelChanged();
    void subtitleTracksModelChanged();
    void hwDecodingChanged();
    void stereoscopicModeChanged();
    void syncVolumeVisibilityFadingChanged();
    void visibilityChanged();
    void eofModeChanged();
    void gridToMapOnChanged();
    void resetOrientation();
    void rotationSpeedChanged();
    void radiusChanged();
    void fovChanged();
    void angleChanged();
    void rotateChanged();
    void translateChanged();
    void planeChanged();
    void surfaceTransitionTimeChanged();
    void surfaceTransitionOnGoingChanged();
    void playlistModelChanged();
    void playSectionsModelChanged();
    void audioDevicesChanged();
    void audioOutputChanged();
    void recentPlaylistsChanged();
    void recentMediaFilesChanged();
    void youtubePlaylistLoaded();
    void surfaceTransitionPerformed();
    void fadeVolumeDown();
    void fadeVolumeUp();
    void fadeImageDown();
    void fadeImageUp();
    void rewind();
    void fadeDownTheRewind();

private:
    PlayListItem *loadMediaFileDescription(const QString &file);
    void loadJSONPlayList(const QString &file, bool updateLastPlayedFile = true);
    void loadUniviewPlaylist(const QString &file, bool updateLastPlayedFile = true);

    void loadItem(PlayListItemData itemData, bool updateLastPlayedFile = true, QString flag = QStringLiteral("replace"));

    mpv_handle *mpv;
    mpv_render_context *mpv_gl;
    QOpenGLFramebufferObject* mpv_fbo;
    std::vector<MpvView*> mpv_views;

    friend class MpvRenderer;
    friend class MpvView;

    void addView(MpvView* view);
    void removeView(MpvView* view);

    void sectionPositionCheck(double position);

    TracksModel *m_audioTracksModel;
    std::vector<Track> m_audioTracks;

    TracksModel *m_subtitleTracksModel;
    std::vector<Track> m_subtitleTracks;

    QList<int> m_secondsWatched;
    double m_watchPercentage;
    double m_rotationSpeed;
    double m_radius;
    double m_fov;
    double m_angle;
    QVector3D m_rotate;
    QVector3D m_translate;
    double m_planeWidth;
    double m_planeHeight;
    double m_planeElevation;
    double m_planeDistance;
    int m_planeConsiderAspectRatio;
    int m_surfaceTransitionTime;
    bool m_surfaceTransitionOnGoing;
    double m_lastSetPosition;
    PlayListModel *m_playlistModel;
    PlaySectionsModel *m_playSectionsModel;
    int m_currentSectionsIndex;
    PlayListItemData::Section m_currentSection;
    QString m_loadedFileStructure;
    QString m_separateAudioFile;
    double m_startTime;
    double m_endTime;
    QVariantList m_audioDevices;
    int m_videoWidth;
    int m_videoHeight;
    bool m_syncVolumeVisibilityFading;
    bool m_autoPlay;

    void loadTracks();
    void updatePlane();
    void updateRecentLoadedMediaFiles(QString path);
    void updateRecentLoadedPlaylists(QString path);
    QString md5(const QString &str);
};

class MpvView : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(MpvObject* mpvObject READ mpvObject WRITE setMpvObject NOTIFY mpvObjectChanged)

public:
    MpvView(QQuickItem* parent = 0);
    ~MpvView() = default;
    Renderer* createRenderer() const override;

    MpvObject* mpvObject() const;
    void setMpvObject(MpvObject* mpv);

Q_SIGNALS:
    void mpvObjectChanged();

private:
    friend class MpvRenderer;

    QOpenGLFramebufferObject* fbo;
    MpvObject* obj;

};

class MpvRenderer : public QQuickFramebufferObject::Renderer {
public:
    MpvRenderer(MpvView* new_view);
    ~MpvRenderer() = default;

    MpvView* view;

    // This function is called when a new FBO is needed.
    // This happens on the initial frame.
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size);

    void render();
};

#endif // MPVOBJECT_H
