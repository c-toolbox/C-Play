/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERSMODEL_H
#define LAYERSMODEL_H

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <layers/baselayer.h>
#include <QtQml/qqmlregistration.h>
#include <QVector>

class QTimer;

using Layers = QList<QPair<QSharedPointer<BaseLayer>, int>>;

// A single keyframe: time in milliseconds [0, timelineDuration] and alpha [0.0, 1.0]
// Rotation (X/Y/Z in degrees) and translation (X/Y/Z) are optional per-keyframe.
struct LayerKeyframe {
    int     timeMs = 0;
    float   alpha  = 0.f;
    bool    hasRotate    = false;
    float   rotateX = 0.f;
    float   rotateY = 0.f;
    float   rotateZ = 0.f;
    bool    hasTranslate = false;
    float   translateX = 0.f;
    float   translateY = 0.f;
    float   translateZ = 0.f;
};

// Per-layer timeline track: ordered list of keyframes
struct LayerTimeline {
    QVector<LayerKeyframe> keyframes;
    int outroStartMs = -1; // -1 means no split (all keyframes are intro)
};

class LayersTypeModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit LayersTypeModel(QObject *parent = nullptr);

    enum {
        textRole = Qt::UserRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

private:
    QStringList m_layerTypes;
};

class LayersModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit LayersModel(QObject *parent = nullptr);
    ~LayersModel();

    enum {
        TitleRole = Qt::UserRole,
        PathRole,
        TypeRole,
        LockedRole,
        PageRole,
        StereoRole,
        GridRole,
        StatusRole,
        VisibilityRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Layers getLayers() const;
    void setLayers(const Layers &layers);

    BaseLayer::LayerHierarchy hierarchy() const;
    void setHierarchy(BaseLayer::LayerHierarchy h);

    int numberOfLayers();
    bool needsSync();
    void setHasSynced();

    Q_INVOKABLE BaseLayer *layer(int i);
    Q_INVOKABLE QString layerTitle(int i) const;
    Q_INVOKABLE int layerIdx(std::string title);
    Q_INVOKABLE int layerStatus(int i);
    Q_INVOKABLE int minLayerStatus();
    Q_INVOKABLE int maxLayerStatus();

    Q_INVOKABLE int addLayer(QString title, int type, QString filepath, int stereoMode, int gridMode);
    Q_INVOKABLE int getLayerTypeBasedOnMime(QUrl fileUrl);
    Q_INVOKABLE int addLayerBasedOnMime(QUrl fileUrl);
    Q_INVOKABLE void removeLayer(int i);
    Q_INVOKABLE void moveLayer(int i, int t);
    Q_INVOKABLE void moveLayerTop(int i);
    Q_INVOKABLE void moveLayerUp(int i);
    Q_INVOKABLE void moveLayerDown(int i);
    Q_INVOKABLE void moveLayerBottom(int i);
    Q_INVOKABLE void updateLayer(int i);
    Q_INVOKABLE void lockLayer(int i);
    Q_INVOKABLE void unlockLayer(int i);
    Q_INVOKABLE bool isLocked(int i);
    Q_INVOKABLE void clearLayers();

    Q_PROPERTY(LayersTypeModel *layersTypeModel
        READ layersTypeModel
        NOTIFY layersTypeModelChanged)

    Q_PROPERTY(int layersVisibility
        READ getLayersVisibility
        WRITE setLayersVisibility
        NOTIFY layersVisibilityChanged)

    Q_INVOKABLE void setLayersVisibility(int value, bool propagateDown = true);
    Q_INVOKABLE int getLayersVisibility();

    Q_PROPERTY(bool layersEnabled
        READ getLayersEnabled
        WRITE setLayersEnabled
        NOTIFY layersEnabledChanged)

    Q_INVOKABLE void setLayersEnabled(bool value);
    Q_INVOKABLE bool getLayersEnabled();

    Q_PROPERTY(bool layersNeedsSave
        READ getLayersNeedsSave
        WRITE setLayersNeedsSave
        NOTIFY layersNeedsSaveChanged)

    Q_INVOKABLE void setLayersNeedsSave(bool value);
    Q_INVOKABLE bool getLayersNeedsSave();

    Q_PROPERTY(int layerToCopy
        READ getLayerToCopyIdx
        WRITE setLayerToCopyIdx
        NOTIFY layerToCopyIdxChanged)

    Q_INVOKABLE void setLayerToCopyIdx(int value);
    Q_INVOKABLE int getLayerToCopyIdx();
    BaseLayer* getLayerToCopy();
    void addCopyOfLayer(BaseLayer* srcLayer);
    void overwriteLayerProperties(BaseLayer* srcLayer, int dstLayerIdx);

    LayersTypeModel *layersTypeModel();

    Q_PROPERTY(QString layersName
        READ getLayersName
        WRITE setLayersName
        NOTIFY layersNameChanged)

    Q_INVOKABLE void setLayersName(QString name);
    Q_INVOKABLE QString getLayersName() const;
    Q_INVOKABLE QString getLayersNameShort(int maxChars) const;

    Q_PROPERTY(bool layersCanBeLocked
        READ getLayersCanBeLocked
        WRITE setLayersCanBeLocked
        NOTIFY layersCanBeLockedChanged)
    Q_INVOKABLE void setLayersCanBeLocked(bool locked);
    Q_INVOKABLE bool getLayersCanBeLocked() const;

    Q_PROPERTY(int layersLockedCount
        READ getLockedLayerCount
        NOTIFY layersModelChanged)
    Q_INVOKABLE int getLockedLayerCount() const;

    Q_INVOKABLE void setLayersPath(QString path);
    Q_INVOKABLE QString getLayersPath() const;
    Q_INVOKABLE QUrl getLayersPathAsURL() const;
    std::string getLayersAsFormattedString(size_t charsPerItem = 33) const;

    Q_INVOKABLE QString checkAndCorrectPath(const QString &filePath, const QStringList &searchPaths);
    Q_INVOKABLE QString makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider);

    void decodeFromJSON(QJsonObject &obj, const QStringList &forRelativePaths);
    void encodeToJSON(QJsonObject &obj, const QStringList &forRelativePaths);

    bool runRenderOnLayersThatShouldUpdate(bool updateRendering, bool preload);

    // ---- Timeline --------------------------------------------------------

    Q_PROPERTY(bool hasTimeline
        READ getHasTimeline
        WRITE setHasTimeline
        NOTIFY timelineChanged)

    Q_INVOKABLE bool getHasTimeline() const;
    Q_INVOKABLE void setHasTimeline(bool value);

    Q_PROPERTY(int timelineDuration
        READ getTimelineDuration
        WRITE setTimelineDuration
        NOTIFY timelineChanged)

    Q_INVOKABLE int  getTimelineDuration() const;  // milliseconds
    Q_INVOKABLE void setTimelineDuration(int ms);

    Q_PROPERTY(int timelineOutroStart
        READ getTimelineOutroStart
        WRITE setTimelineOutroStart
        NOTIFY timelineChanged)

    Q_INVOKABLE int  getTimelineOutroStart() const; // ms, -1 = no split
    Q_INVOKABLE void setTimelineOutroStart(int ms);

    // Keyframe access from QML
    // Returns QVariantList of maps { "timeMs": int, "alpha": real }
    Q_INVOKABLE QVariantList getKeyframes(int layerIdx) const;

    // Set the full keyframe list for a layer (list of maps as above)
    Q_INVOKABLE void setKeyframes(int layerIdx, const QVariantList &keyframes);

    Q_INVOKABLE void addKeyframe(int layerIdx, int timeMs, float alpha);
    Q_INVOKABLE void addKeyframe(int layerIdx, int timeMs, float alpha,
                                 bool hasRotate,
                                 float rotateX, float rotateY, float rotateZ,
                                 bool hasTranslate,
                                 float translateX, float translateY, float translateZ);
    Q_INVOKABLE void removeKeyframe(int layerIdx, int keyframeIdx);
    Q_INVOKABLE void updateKeyframe(int layerIdx, int keyframeIdx, int timeMs, float alpha);
    Q_INVOKABLE void updateKeyframe(int layerIdx, int keyframeIdx, int timeMs, float alpha,
                                    bool hasRotate,
                                    float rotateX, float rotateY, float rotateZ,
                                    bool hasTranslate,
                                    float translateX, float translateY, float translateZ);
    Q_INVOKABLE bool layerHasKeyframes(int layerIdx) const;

    // Evaluate interpolated alpha for a given layer at a given time
    float evaluateAlphaAt(int layerIdx, int timeMs) const;
    // Evaluate interpolated rotation; returns false if no rotation keyframes exist
    bool evaluateRotateAt(int layerIdx, int timeMs, float &rx, float &ry, float &rz) const;
    // Evaluate interpolated translation; returns false if no translation keyframes exist
    bool evaluateTranslateAt(int layerIdx, int timeMs, float &tx, float &ty, float &tz) const;

    // Apply interpolated alpha to all layers at the given timeline position
    // and emit dataChanged so model views (LayersItemCompact) update instantly.
    Q_INVOKABLE void applyTimelineAt(int timeMs);

    // Apply the outro section at outroTimeMs (0 = start of outro, outroStartMs..duration range).
    // targetAlphaPerLayer: per-layer end-state alpha as determined by SlideVisibilityModel (0..1).
    // If targetAlphaPerLayer is empty, the outro keyframes' own alpha values are used directly.
    Q_INVOKABLE void applyTimelineOutroAt(int outroTimeMs, const QVector<float> &targetAlphaPerLayer);

    // Helper: does this model have an outro section defined?
    Q_INVOKABLE bool hasOutro() const;
    // Duration of the intro section (0..outroStartMs), or full duration if no split.
    Q_INVOKABLE int  introDuration() const;
    // Duration of the outro section (outroStartMs..timelineDuration), or 0 if no split.
    Q_INVOKABLE int  outroDuration() const;

    // ---- Timeline playback (per-slide, independent) -------------------------

    Q_INVOKABLE void startTimeline();
    Q_INVOKABLE void startTimelineFrom(int startMs);
    Q_INVOKABLE void startTimelineReverse();
    Q_INVOKABLE void startTimelineIntro();
    Q_INVOKABLE void startTimelineOutro();
    Q_INVOKABLE void stopTimeline();
    Q_INVOKABLE void stopOutro();
    Q_INVOKABLE void stopAllTimelines();

    Q_INVOKABLE bool timelineRunning() const;
    Q_INVOKABLE bool timelineReversed() const;
    Q_INVOKABLE int  timelinePositionMs() const;
    Q_INVOKABLE bool outroRunning() const;

    Q_INVOKABLE bool hasTimelineKeyframes() const;

    Q_INVOKABLE void jumpToIntroStart();
    Q_INVOKABLE void jumpToOutroStart();

    // Returns the timeline progress [0,100] if timeline keyframes exist, or -1.
    Q_INVOKABLE int  timelineVisibility() const;

    // -----------------------------------------------------------------------

Q_SIGNALS:
    void layersModelChanged();
    void layersTypeModelChanged();
    void layersVisibilityChanged();
    void layersEnabledChanged(bool);
    void layersNeedsSaveChanged(bool);
    void layersNameChanged();
    void layersCanBeLockedChanged();
    void layerToCopyIdxChanged();
    void timelineChanged();
    void timelineStarted();
    void timelineStopped();
    void timelinePositionChanged(int posMs);
    void outroStarted();
    void outroStopped();

private:
    void setNeedSync();
    void ensureTimelineSizeMatchesLayers();

    Layers m_layers;
    LayersTypeModel *m_layerTypeModel;
    BaseLayer::LayerHierarchy m_layerHierachy;
    int m_layersVisibility = 0;
    bool m_layersEnabled = true;
    int m_layerToCopyIdx = -1;
    bool m_layersCanBeLocked = false;
    bool m_layersNeedsSave = false;
    bool m_needSync;
    int m_syncIteration;
    QString m_layersName;
    QString m_layersPath;

    // Timeline data
    bool m_hasTimeline = false;
    int  m_timelineDuration = 5000; // ms
    int  m_timelineOutroStart = -1; // ms, -1 = no split
    QVector<LayerTimeline> m_layerTimelines;

    // Timeline playback state (per-slide)
    void onTimelineTick();
    void ensureTimerRunning();
    void stopTimerIfIdle();

    QTimer*        m_timelineTimer = nullptr;
    QElapsedTimer  m_timelineElapsed;
    int            m_timelinePauseOffset = 0;
    bool           m_timelineRunning     = false;
    bool           m_timelineReversed    = false;
    int            m_timelinePositionMs  = 0;

    QElapsedTimer  m_outroElapsed;
    bool           m_outroRunning        = false;
    QVector<float> m_outroTargetAlpha;
};

#endif // LAYERSMODEL_H
