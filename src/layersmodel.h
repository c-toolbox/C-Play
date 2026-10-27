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
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <animation/layereffects.h>
#include <layers/baselayer.h>
#include <QtQml/qqmlregistration.h>
#include <QSet>
#include <QVector>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>

class QTimer;

using Layers = QList<QPair<std::shared_ptr<BaseLayer>, int>>;

// Owns a layer created on the master. Layer destructors release OpenGL objects and mpv render
// contexts, and the last reference may be dropped on the GUI thread, so the deleter runs
// inside a ScopedSharedGLContext (see application.h).
std::shared_ptr<BaseLayer> makeLayerPtr(BaseLayer *layer);

// A single keyframe at timeMs. Alpha is always keyed; the other groups are optional and
// interpolate only between keyframes that have them. Values are absolute layer values.
// easing shapes the segment from this keyframe to the next one of the same group.
struct LayerKeyframe {
    int     timeMs = 0;
    float   alpha  = 0.f;
    int     easing = 0;
    bool    hasRotate    = false;
    float   rotateX = 0.f;
    float   rotateY = 0.f;
    float   rotateZ = 0.f;
    bool    hasTranslate = false;
    float   translateX = 0.f;
    float   translateY = 0.f;
    float   translateZ = 0.f;
    bool    hasScale = false;
    float   scale = 1.f;
    bool    hasPlane = false;
    float   planeAzimuth = 0.f;
    float   planeElevation = 0.f;
    float   planeRoll = 0.f;
    float   planeDistance = 0.f;
    float   planeHorizontal = 0.f;
    float   planeVertical = 0.f;
};

// Per-layer timeline track: ordered keyframes plus entrance/exit effects.
struct LayerTimeline {
    QVector<LayerKeyframe> keyframes;
    QVector<cplay::anim::LayerEffect> effects;
};

class LayersModel;

// A layer owned by an earlier slide that is still visible in this slide
// (see BaseLayer::keepVisibilityForNumSlides). It is listed, ordered and can be
// animated in this slide, but it is saved, synced and loaded by its owner.
struct LayerReference {
    std::shared_ptr<BaseLayer> layer;
    QPointer<LayersModel> owner;
    // How many slides back the owner is (1 = previous slide).
    int slideOffset = 0;
    // Index of the layer in the owner's own layer list.
    int ownerLayerIdx = -1;
};

// What LayersModel::takeLayer() removed, so LayersModel::restoreLayer() can put it back.
struct RemovedLayer {
    // Row the layer was listed in.
    int row = -1;
    // An own layer, as saved in a presentation (with absolute paths).
    QJsonObject data;
    // A referenced layer, whose keep-visibility span was shortened to end before this slide.
    std::weak_ptr<BaseLayer> reference;
    QPointer<LayersModel> referenceOwner;
    int previousKeepVisibility = 0;

    bool isValid() const { return !data.isEmpty() || !reference.expired(); }
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
        VisibilityRole,
        NdiOutputRole,
        ExistOnMasterOnlyRole,
        NodeStreamOutputRole,
        IsReferenceRole,
        ReferenceSourceRole,
        HierarchyRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    // Rows of the model are the slide's render order (top first): its own layers plus
    // references to layers of earlier slides. Q_INVOKABLE index arguments are rows.
    // getLayers(), numberOfLayers(), layerShared() and layerIdx() only cover the layers
    // this slide owns, indexed in their own order.
    Layers getLayers() const;
    void setLayers(const Layers &layers);

    BaseLayer::LayerHierarchy hierarchy() const;
    void setHierarchy(BaseLayer::LayerHierarchy h);

    int numberOfLayers();
    bool needsSync();
    void setHasSynced();

    Q_INVOKABLE BaseLayer *layer(int i);
    std::shared_ptr<BaseLayer> layerShared(int i);
    // Like layer(i) (a row: own layer or reference), but keeps the layer alive.
    std::shared_ptr<BaseLayer> rowLayerShared(int i);

    // ---- References / render order ---------------------------------------

    // Set by SlidesModel whenever the slide structure or keep-visibility spans change.
    void setReferencedLayers(const QVector<LayerReference> &refs);
    Q_INVOKABLE bool isReference(int i) const;
    // "<slide number>. <slide name>" of the slide owning a referenced row, empty for own layers.
    Q_INVOKABLE QString referenceSource(int i) const;
    Q_INVOKABLE int numberOfReferences() const;
    bool containsLayer(const BaseLayer *layer) const;
    int rowOfLayer(const BaseLayer *layer) const;
    // Status of an own layer (see layerStatus), -1 if not owned.
    int ownLayerStatus(const BaseLayer *layer) const;
    // Every layer of the slide (own and referenced), top of the render order first.
    std::vector<std::shared_ptr<BaseLayer>> renderOrderShared() const;
    // Refreshes the row showing the layer, safe to call from any thread.
    void updateLayerRow(const BaseLayer *layer);

    // True when this slide animates the layer: effects, or keyframes on a timeline slide.
    bool hasTrackForLayer(const BaseLayer *layer) const;
    // Gives a referenced layer without a track a fade-out over the outro (this slide is
    // the last one showing it). Returns true when a track was created.
    bool addReferenceOutroFade(const BaseLayer *layer);
    // Drops a referenced layer's track if it is still the unedited fade from addReferenceOutroFade.
    bool removeReferenceOutroFade(const BaseLayer *layer);
    // Drops stored reference animations of layers that no longer exist in any slide.
    void pruneReferenceTracks(const QSet<const BaseLayer *> &aliveLayers);

    // Layers the timeline of this slide must leave alone because another slide that
    // also contains them is live (the triggered slide drives shared layers).
    // isLive: this slide is the triggered one.
    void setTimelineSuppressedLayers(const QSet<const BaseLayer *> &layers, bool isLive);

    // ---- Move effects across slides ---------------------------------------

    // Plane poses referenced layers enter this slide with, as left by Move effects of
    // earlier slides (set by SlidesModel). Layers without an entry use their saved values.
    void setEntryPoses(std::unordered_map<const BaseLayer *, cplay::anim::PlanePose> poses);
    // Pose after this slide's entrance Move effects. False when the layer neither enters
    // with a carried pose nor moves in this slide.
    bool finalMovePose(const BaseLayer *layer, cplay::anim::PlanePose &pose) const;
    // Places referenced layers this slide does not animate where earlier slides left them.
    void applyCarriedPoses();
    // Plane pose a layer has right before the given effect starts (map of the 6 plane values).
    Q_INVOKABLE QVariantMap poseBeforeEffect(int layerIdx, int effectIdx) const;
    // Stores the layer's currently shown plane pose as the start or end of a Move effect.
    Q_INVOKABLE void captureMovePose(int layerIdx, int effectIdx, bool end);
    Q_INVOKABLE bool effectPresetIsMove(int preset) const;

    Q_INVOKABLE QString layerTitle(int i) const;
    Q_INVOKABLE int layerVisibility(int i) const;
    Q_INVOKABLE int layerIdx(std::string title);
    Q_INVOKABLE int layerStatus(int i) const;
    Q_INVOKABLE void setLayerStatus(int i, int status);
    Q_INVOKABLE int minLayerStatus();
    Q_INVOKABLE int maxLayerStatus();

    Q_INVOKABLE int addLayer(QString title, int type, QString filepath, int stereoMode, int gridMode);
    Q_INVOKABLE int addTcpLayer(const QString &title, const QString &commandId);
    // Add a REST layer with its request fully specified up front (used by the C-Lux editor
    // to attach scene actions to slides). Returns the new layer index or -1 on failure.
    Q_INVOKABLE int addRestLayer(QString title, QString url, int method, QString parameters, bool ignoreStatus);
    Q_INVOKABLE int getLayerTypeBasedOnMime(QUrl fileUrl);
    Q_INVOKABLE int addLayerBasedOnMime(QUrl fileUrl);
    Q_INVOKABLE void removeLayer(int i);
    // Removes the layer in row i like removeLayer() and returns what is needed to undo it
    // (invalid when nothing was removed, e.g. a locked layer).
    RemovedLayer takeLayer(int i);
    // Puts back a layer removed by takeLayer(). Returns false when that is no longer possible.
    bool restoreLayer(const RemovedLayer &removed);
    Q_INVOKABLE void moveLayer(int i, int t);
    Q_INVOKABLE void moveLayerTop(int i);
    Q_INVOKABLE void moveLayerUp(int i);
    Q_INVOKABLE void moveLayerDown(int i);
    Q_INVOKABLE void moveLayerBottom(int i);
    // Moves row from[k] to row to[k], in order, holding the layers lock for the whole batch.
    Q_INVOKABLE void moveLayers(const QList<int> &from, const QList<int> &to);
    Q_INVOKABLE void updateLayer(int i);
    Q_INVOKABLE void lockLayer(int i);
    Q_INVOKABLE void unlockLayer(int i);
    Q_INVOKABLE bool isLocked(int i);
    // BaseLayer::LayerHierarchy of the layer in row i (0 = back, 1 = front), -1 if none.
    Q_INVOKABLE int layerHierarchy(int i) const;
    Q_INVOKABLE void setLayerHierarchy(int i, int h);
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

    // A skipped slide is passed over by next/previous. A skipped master slide is not
    // rendered and its layers get zero opacity (restored when no longer skipped).
    Q_PROPERTY(bool slideSkipped
        READ slideSkipped
        WRITE setSlideSkipped
        NOTIFY slideSkippedChanged)

    bool slideSkipped() const;
    void setSlideSkipped(bool value);

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
    std::shared_ptr<BaseLayer> getLayerToCopy();
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

    // Presenter notes of the slide, shown and edited in the presenter view and saved in the presentation.
    Q_PROPERTY(QString notes
        READ notes
        WRITE setNotes
        NOTIFY notesChanged)
    QString notes() const;
    void setNotes(const QString &notes);

    // JPEG preview of the slide for the presenter view, saved in the presentation.
    QByteArray thumbnail() const;
    void setThumbnail(const QByteArray &jpeg);
    // Changes whenever the thumbnail changes, so clients know when to fetch it again. 0 = never set.
    int thumbnailRevision() const;

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
    // One own layer of the "layers" array of a saved slide. Decoding appends the layer and
    // returns its row, -1 if it could not be created.
    QJsonObject encodeLayerToJSON(const std::shared_ptr<BaseLayer> &layer, const QStringList &forRelativePaths);
    int decodeLayerFromJSON(const QJsonObject &o, const QStringList &forRelativePaths);

    bool runRenderOnLayersThatShouldUpdate(bool updateRendering, bool preload);

    // ---- Timeline --------------------------------------------------------

    // Enables the keyframe timeline and its own intro/outro durations. Entrance and exit
    // effects do not need it: on a regular slide they play alongside the slide fade.
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

    // Sets the easing of one keyframe (segment towards the next keyframe).
    Q_INVOKABLE void setKeyframeEasing(int layerIdx, int keyframeIdx, int easing);
    // Sets optional scale/plane groups of one keyframe from a map (see getKeyframes()).
    Q_INVOKABLE void updateKeyframeExtras(int layerIdx, int keyframeIdx, const QVariantMap &values);

    // ---- Entrance / exit effects ------------------------------------------

    // Effects of one layer: maps with order, phase, preset, trigger, delayMs, durationMs,
    // easing, direction, feather, amount plus scheduled startMs/endMs (relative to phase start).
    Q_INVOKABLE QVariantList getEffects(int layerIdx) const;
    // All effects of the slide for a phase, in playback order, with layerIdx/effectIdx/stepNumber.
    Q_INVOKABLE QVariantList getSlideEffects(int phase) const;
    Q_INVOKABLE int addEffect(int layerIdx, int phase, int preset);
    Q_INVOKABLE void updateEffect(int layerIdx, int effectIdx, const QVariantMap &values);
    Q_INVOKABLE void removeEffect(int layerIdx, int effectIdx);
    // Moves an effect to position newPos within the ordered list of its phase.
    Q_INVOKABLE void moveEffect(int layerIdx, int effectIdx, int newPos);
    Q_INVOKABLE bool layerHasEffects(int layerIdx) const;
    Q_INVOKABLE bool layerSupportsAnimation(int layerIdx) const;
    // Plays the timeline from the start of the given effect.
    Q_INVOKABLE void previewEffect(int layerIdx, int effectIdx);
    // Bakes an effect into keyframes is not possible for masks; returns false then.
    Q_INVOKABLE bool convertEffectToKeyframes(int layerIdx, int effectIdx);

    Q_INVOKABLE QStringList effectPresetNames() const;
    Q_INVOKABLE int effectPresetCategory(int preset) const;
    Q_INVOKABLE bool effectPresetUsesDirection(int preset) const;
    Q_INVOKABLE bool effectPresetUsesFeather(int preset) const;
    Q_INVOKABLE QStringList effectDirectionNames() const;
    Q_INVOKABLE QStringList easingNames() const;
    // Eased value of t in [0,1] for drawing easing curves in the UI.
    Q_INVOKABLE double easingValue(int easing, double t) const;

    // ---- Post process effects ----------------------------------------------

    // Texture effects always applied to the layer, in every slide (a property of the layer,
    // saved by the slide owning it). The map holds one bool per effect (chromaKey, lumaKey,
    // colorCorrection, invert, tint, blur, sharpen, pixelate, vignette) and their parameters;
    // colors are "#rrggbb" strings.
    Q_INVOKABLE QVariantMap getPostEffects(int layerIdx) const;
    Q_INVOKABLE void updatePostEffects(int layerIdx, const QVariantMap &values);
    // Restores the parameters of one effect (named by its bool) to their defaults.
    Q_INVOKABLE void resetPostEffect(int layerIdx, const QString &effect);
    Q_INVOKABLE void clearPostEffects(int layerIdx);
    Q_INVOKABLE bool layerHasPostEffects(int layerIdx) const;

    // ---- On-click build steps (intro only) --------------------------------

    // Start times (ms) of each on-click step within the intro.
    Q_INVOKABLE QVariantList introStepStarts() const;
    // True while the intro is paused at an on-click step waiting for "next".
    Q_INVOKABLE bool hasPendingAnimationStep() const;
    Q_INVOKABLE bool playNextAnimationStep();
    Q_INVOKABLE bool playPreviousAnimationStep();

    // ---- Layer animation (one layer shown or hidden on its own) ------------

    // Plays the layer's entrance (show) or exit effects by themselves, as when the layer is
    // faded in or out by hand. Works in every slide, the master slide included. Returns
    // false when the layer has no effects in that phase, so the caller fades it normally.
    Q_INVOKABLE bool playLayerAnimation(int layerIdx, bool show);
    Q_INVOKABLE bool layerAnimationRunning(int layerIdx) const;
    // Shows the layer at visibility value (0..100) of its entrance or exit, as when its
    // visibility slider is dragged. The phase is picked when a scrub starts (a fully shown
    // layer scrubs its exit, any other its entrance; a running animation keeps its own) and
    // kept until the scrub reaches 0 or 100. Returns false when that phase has no effects,
    // so the caller sets the opacity instead.
    Q_INVOKABLE bool scrubLayerAnimation(int layerIdx, int value);
    Q_INVOKABLE void stopLayerAnimations();
    // Shows (fades up) or hides (fades down) the layer as double-clicking it does: plays its
    // entrance or exit effects when it has them, otherwise fades its opacity over the fade
    // duration. subLayerIdx >= 0 fades that sublayer's opacity instead. Returns false when
    // the layer is not in this slide.
    bool fadeLayer(const BaseLayer *layer, bool show, int subLayerIdx = -1);
    // fadeLayer() for each of the rows that has a visibility. Returns true when any fade started.
    Q_INVOKABLE bool fadeLayers(const QList<int> &rows, bool show);
    // True when any of the rows with a visibility can be faded in (show) or out.
    Q_INVOKABLE bool canFadeLayers(const QList<int> &rows, bool show) const;

    // True when the layer's alpha is driven by the animation (keyframes or entrance effects).
    bool isLayerTimelineDriven(const BaseLayer *layer) const;

    // Evaluate interpolated alpha for a given layer at a given time
    float evaluateAlphaAt(int layerIdx, int timeMs) const;

    // Apply interpolated alpha to all layers at the given timeline position
    // and emit dataChanged so model views (LayersItemCompact) update instantly.
    Q_INVOKABLE void applyTimelineAt(int timeMs, bool skipReferences = false);

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
    Q_INVOKABLE void startTimelineOutro(int fromOutroMs = 0);
    Q_INVOKABLE void stopTimeline();
    Q_INVOKABLE void stopOutro();
    Q_INVOKABLE void stopAllTimelines();

    Q_INVOKABLE bool timelineRunning() const;
    Q_INVOKABLE bool timelineReversed() const;
    Q_INVOKABLE int  timelinePositionMs() const;
    Q_INVOKABLE bool outroRunning() const;

    // True when triggering the slide plays its animation (timeline keyframes or effects)
    // instead of the plain slide fade.
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
    void slideSkippedChanged();
    void layerHierarchyChanged(int ownIdx);
    void layersNeedsSaveChanged(bool);
    void layersNameChanged();
    void notesChanged();
    void thumbnailChanged();
    void layersCanBeLockedChanged();
    void layerToCopyIdxChanged();
    void timelineChanged();
    void timelineStarted();
    void timelineStopped();
    void timelinePositionChanged(int posMs);
    void outroStarted();
    void outroStopped();
    void animationStepChanged();
    // A layer animation started with playLayerAnimation() ended (layerIdx is its row).
    void layerAnimationFinished(int layerIdx);
    // Progress of a running layer animation as visibility: 0..100 for an entrance, 100..0 for an exit.
    void layerAnimationProgressChanged(int layerIdx, int progress);
    // Post effects of the layer in row layerIdx changed.
    void postEffectsChanged(int layerIdx);
    // A referenced layer's keep-visibility span was shortened from this slide.
    void layerVisibilitySpanChanged(BaseLayer *layer);

private:
    void setNeedSync();

public:
    // Marks every node-streaming layer for a full re-sync (see BaseLayer::markNodeStreamLayerForResync).
    void markAllLayersNeedSync();

private:
    BaseLayer *rowLayer(int row) const;
    int ownIndexOfRow(int row) const;
    int ownIndexOf(const BaseLayer *layer) const;
    int referenceIndexOf(const BaseLayer *layer) const;
    // Row where a newly added own layer goes: below the last own layer, above references.
    int rowForNewOwnLayer() const;
    // Reorders m_layers to follow the own layers' order in m_order.
    void syncOwnLayersToOrder();
    void applyPendingRenderOrder();
    void applyPendingReferenceTracks();
    bool isTimelineSuppressed(const BaseLayer *layer) const;
    // Hides referenced layers that are not shown by the live slide (used when this slide is left).
    void hideUnsuppressedReferences(bool evenWhenLive);
    // Marks the slide owning the layer in row layerIdx (this one or, for a reference, its owner) as changed.
    void markLayerNeedsSave(int layerIdx);
    QJsonObject encodeTrack(const LayerTimeline &track) const;
    void decodeTrack(const QJsonObject &o, LayerTimeline &track) const;

    void ensureTimelineSizeMatchesLayers();
    // Default slide fade as Fade effects spanning the intro and/or the outro.
    void addDefaultFadeEffects(LayerTimeline &track, bool intro, bool outro) const;
    bool isDefaultIntroFade(const cplay::anim::LayerEffect &e) const;
    bool isDefaultOutroFade(const cplay::anim::LayerEffect &e) const;
    // Drops the default fades a timeline slide gave its layers (used when the timeline is turned off).
    void removeDefaultFadeEffects();
    // Gives a layer added by the user to a timeline slide the default fade effects.
    void addDefaultFadeForNewLayer(int layerIdx);
    LayerTimeline *timelineAt(int layerIdx);
    const LayerTimeline *timelineAt(int layerIdx) const;
    // Evaluates keyframes and effects of one layer at absolute timeline time.
    cplay::anim::LayerAnimState evaluateAnimState(int layerIdx, int timeMs) const;
    struct ScheduledEffect;
    // Applies the layer's non-move effects of a schedule at localMs (time in the phase).
    void applyScheduledEffects(const BaseLayer *layer, const LayerTimeline &track,
                               const QVector<ScheduledEffect> &schedule, int localMs, bool intro,
                               cplay::anim::LayerAnimState &state) const;
    // Runs the layer's Move effects of a schedule up to localMs, starting from pose.
    // Returns false when stopBefore was reached.
    bool runMoveEffects(const BaseLayer *layer, const LayerTimeline &track,
                        const QVector<ScheduledEffect> &schedule, int localMs,
                        cplay::anim::PlanePose &pose, bool &moved,
                        const cplay::anim::LayerEffect *stopBefore) const;
    // Render offsets that show the layer at pose.
    void setPoseOffsets(const BaseLayer *layer, const cplay::anim::PlanePose &pose,
                        cplay::anim::LayerAnimState &state) const;
    cplay::anim::PlanePose entryPose(const BaseLayer *layer) const;
    // Plane pose from Move effects at timeline time; false when nothing moves the layer.
    // stopBefore: evaluate only the Move effects scheduled before this one.
    bool evaluateMovePose(const BaseLayer *layer, int timeMs, cplay::anim::PlanePose &pose,
                          const cplay::anim::LayerEffect *stopBefore = nullptr) const;
    void rebuildEffectSchedule();
    void fitTimelineToEffects();
    // Without a timeline, intro and outro last the slide fade, or longer when effects need it.
    void fitEffectsOnlyTiming();
    // Playback runs on timeline slides and on slides with effects.
    bool animationsActive() const;
    bool hasAnyEffects() const;
    // Keyframes only animate on timeline slides.
    const QVector<LayerKeyframe> &activeKeyframes(const LayerTimeline &track) const;
    // Fade-in / fade-out time of layers this slide does not animate.
    int plainFadeInMs() const;
    int plainFadeOutMs() const;
    int introStopAfter(int startMs) const;
    void clearAllAnimStates();
    void guessGridModeForDraggedLayer(int layerIdx, BaseLayer *layer);

    Layers m_layers;
    QVector<LayerReference> m_refs;
    // Render order of all rows (own and referenced layers), top first.
    QVector<const BaseLayer *> m_order;
    // Saved order and reference animations waiting for the references to be resolved
    // after a presentation load (references are only known once every slide is loaded).
    QJsonArray m_pendingRenderOrder;
    QJsonArray m_pendingReferenceTracks;
    // Layers that had a reference track at some point; their track survives while the
    // layer exists, so shrinking and re-extending a keep-visibility span keeps animations.
    std::unordered_map<const BaseLayer *, std::weak_ptr<BaseLayer>> m_referenceTrackLayers;
    QSet<const BaseLayer *> m_timelineSuppressed;
    std::unordered_map<const BaseLayer *, cplay::anim::PlanePose> m_entryPoses;
    bool m_isLiveSlide = false;
    QSet<BaseLayer *> m_draggedLayersAwaitingGridGuess;
    // Guards m_layers / m_draggedLayersAwaitingGridGuess against the render thread
    // (SlidesQtItemRenderer::update -> runRenderOnLayersThatShouldUpdate), which
    // iterates them while the GUI thread adds/removes layers via drag-and-drop.
    // Without this, m_layers.push_back() reallocating the QList dangles the render
    // thread's reference (use-after-free) and concurrent QSet access corrupts the
    // hash, which is what makes adding several layers at once stall and crash.
    mutable std::recursive_mutex m_layersMutex;
    LayersTypeModel *m_layerTypeModel;
    BaseLayer::LayerHierarchy m_layerHierachy;
    int m_layersVisibility = 0;
    bool m_layersEnabled = true;
    // Read by the render thread.
    std::atomic_bool m_slideSkipped{false};
    // Opacity of the master layers before the master was skipped.
    std::vector<std::pair<std::weak_ptr<BaseLayer>, float>> m_skippedAlphas;
    int m_layerToCopyIdx = -1;
    bool m_layersCanBeLocked = false;
    bool m_layersNeedsSave = false;
    bool m_decodingLayers = false;
    bool m_needSync;
    int m_syncIteration;
    QString m_layersName;
    QString m_layersPath;
    QString m_notes;
    QByteArray m_thumbnail;
    int m_thumbnailRevision = 0;

    // Timeline data
    bool m_hasTimeline = false;
    int  m_timelineDuration = 5000; // ms
    int  m_timelineOutroStart = -1; // ms, -1 = no split
    // Keyed by layer so timelines follow layers when they are moved or removed.
    std::unordered_map<const BaseLayer *, LayerTimeline> m_layerTimelines;

    struct ScheduledEffect {
        const BaseLayer *layer = nullptr;
        int effectIdx = -1;
        int startMs = 0;
        int endMs = 0;
        int step = 0;
    };
    QVector<ScheduledEffect> m_introSchedule;
    // A layer playing its entrance or exit on its own (see playLayerAnimation).
    struct LayerPlay {
        const BaseLayer *layer = nullptr;
        bool show = true;
        QVector<ScheduledEffect> schedule;
        int spanMs = 0;
        cplay::anim::PlanePose startPose;
        bool moved = false;
        QElapsedTimer elapsed;
        // Held by the visibility slider at scrubMs instead of running.
        bool scrubbing = false;
        int scrubMs = 0;
        int scrubValue = 0;
    };
    QVector<LayerPlay> m_layerPlays;
    cplay::anim::LayerAnimState evaluateLayerPlay(const LayerPlay &play, int ms) const;
    // The layer's effects of one phase, timed among themselves. False when there are none.
    bool buildLayerPlay(int layerIdx, bool show, LayerPlay &play) const;
    int layerPlayMs(const LayerPlay &play) const;
    bool hasTimedLayerPlays() const;
    // Visibility shown for a layer while it animates on its own (see above), -1 when it does not.
    int layerPlayProgress(const BaseLayer *layer) const;
    void advanceLayerPlays();
    // A layer (or one of its sublayers) fading its opacity on its own (see fadeLayer).
    struct LayerFade {
        const BaseLayer *layer = nullptr;
        int subLayerIdx = -1;
        float from = 0.f;
        float to = 0.f;
        int durationMs = 0;
        QElapsedTimer elapsed;
    };
    QVector<LayerFade> m_layerFades;
    BaseLayer *layerFadeTarget(const LayerFade &fade, int &row) const;
    void advanceLayerFades();
    QVector<ScheduledEffect> m_outroSchedule;
    QVector<int> m_introStepStarts;
    int m_introEffectsSpan = 0;
    int m_outroEffectsSpan = 0;
    int m_timelineStopAtMs = -1;

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
    int            m_outroOffsetMs       = 0;
    bool           m_outroRunning        = false;
    QVector<float> m_outroTargetAlpha;
};

#endif // LAYERSMODEL_H
