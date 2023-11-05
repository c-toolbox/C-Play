/*
 * SPDX-FileCopyrightText: 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MPVOBJECT_H
#define MPVOBJECT_H

#include <QtQuick/QQuickFramebufferObject>
#include <QVector3D>

#include <client.h>
#include <render_gl.h>
#include "qthelper.h"
#include "playlistmodel.h"
#include "playlistitem.h"
#include "tracksmodel.h"

class MpvRenderer;
class Track;

class MpvObject : public QQuickFramebufferObject
{
    Q_OBJECT
public:
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

    Q_PROPERTY(int stereoscopicVideo
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

    Q_PROPERTY(int loopMode
               READ loopMode
               WRITE setLoopMode
               NOTIFY loopModeChanged)

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

    Q_PROPERTY(bool surfaceTransistionOnGoing
               MEMBER m_surfaceTransistionOnGoing
               READ surfaceTransistionOnGoing
               WRITE setSurfaceTransistionOnGoing
               NOTIFY surfaceTransistionOnGoingChanged)

    Q_PROPERTY(PlayListModel* playlistModel
               READ playlistModel
               WRITE setPlaylistModel
               NOTIFY playlistModelChanged)

    PlayListModel *playlistModel();
    void setPlaylistModel(PlayListModel *model);

    Q_PROPERTY(PlaySectionsModel* playSectionsModel
        READ playSectionsModel
        WRITE setPlaySectionsModel
        NOTIFY playSectionsModelChanged)

    PlaySectionsModel* playSectionsModel();
    void setPlaySectionsModel(PlaySectionsModel* model);

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

    int stereoscopicVideo();
    void setStereoscopicVideo(int value);

    bool syncVideo();
    void setSyncVideo(bool value);

    int visibility();
    void setVisibility(int value);

    int loopMode();
    void setLoopMode(int value);

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

    bool surfaceTransistionOnGoing();
    void setSurfaceTransistionOnGoing(bool value);

    QVariant getAudioDeviceList();
    void updateAudioDeviceList();

    PlayListItem* loadMediaFileDescription(const QString& file);
    void loadJSONPlayList(const QString& file, bool updateLastPlayedFile = true);
    void loadUniviewPlaylist(const QString& file, bool updateLastPlayedFile = true);

    void loadItem(PlayListItemData itemData, bool updateLastPlayedFile = true, QString flag = "replace");

    mpv_handle *mpv;
    mpv_render_context *mpv_gl;

    friend class MpvRenderer;

public:
    MpvObject(QQuickItem * parent = 0);
    virtual ~MpvObject();
    virtual Renderer *createRenderer() const;

    PlayListModel* getPlayListModel() const;
    PlaySectionsModel* getPlaySectionsModel() const;

    Q_INVOKABLE QString checkAndCorrectPath(const QString& filePath, const QStringList& searchPaths);
    Q_INVOKABLE void loadFile(const QString &file, bool updateLastPlayedFile = true);
    Q_INVOKABLE void addFileToPlaylist(const QString& file);
    Q_INVOKABLE void clearPlaylist();
    Q_INVOKABLE void setLoadedAsCurrentEditItem();
    Q_INVOKABLE void setCurrentEditItemFromPlaylist(int playListIndex);
    Q_INVOKABLE void loadSection(int playSectionsIndex);
    Q_INVOKABLE void loadItem(int playListIndex, bool updateLastPlayedFile = true);
    Q_INVOKABLE void getYouTubePlaylist(const QString &path);
    Q_INVOKABLE QVariant command(const QVariant &params, bool debug = false);
    Q_INVOKABLE QVariant getProperty(const QString &name, bool debug = false);
    Q_INVOKABLE int setProperty(const QString &name, const QVariant &value, bool debug = false);
    Q_INVOKABLE void saveTimePosition();
    Q_INVOKABLE double loadTimePosition();
    Q_INVOKABLE void resetTimePosition();
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void clearRecentMediaFilelist();
    Q_INVOKABLE void clearRecentPlaylist();

public slots:
    static void mpvEvents(void *ctx);
    void eventHandler();
    void performSurfaceTransistion();

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
    void loopModeChanged();
    void gridToMapOnChanged();
    void resetOrientation();
    void rotationSpeedChanged();
    void radiusChanged();
    void fovChanged();
    void angleChanged();
    void rotateChanged();
    void translateChanged();
    void planeChanged();
    void surfaceTransistionOnGoingChanged();
    void playlistModelChanged();
    void playSectionsModelChanged();
    void audioDevicesChanged();
    void recentPlaylistsChanged();
    void recentMediaFilesChanged();
    void youtubePlaylistLoaded();
    void surfaceTransistionPerformed();
    void fadeVolumeDown();
    void fadeVolumeUp();
    void fadeImageDown();
    void fadeImageUp();

private:
    void sectionPositionCheck(double position);
    TracksModel *audioTracksModel() const;
    TracksModel *subtitleTracksModel() const;
    TracksModel *m_audioTracksModel;
    TracksModel *m_subtitleTracksModel;
    QMap<int, Track*> m_subtitleTracks;
    QMap<int, Track*> m_audioTracks;
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
    bool m_surfaceTransistionOnGoing;
    double m_lastSetPosition;
    PlayListModel *m_playlistModel;
    PlaySectionsModel* m_playSectionsModel;
    int m_currentSectionsIndex;
    PlayListItemData::Section m_currentSection;
    QString m_loadedFileStructure;
    QString m_separateAudioFile;
    double m_startTime;
    double m_endTime;
    QVariantList m_audioDevices;
    int m_videoWidth;
    int m_videoHeight;

    void loadTracks();
    void updatePlane();
    void updateRecentLoadedMediaFiles(QString path);
    void updateRecentLoadedPlaylists(QString path);
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
