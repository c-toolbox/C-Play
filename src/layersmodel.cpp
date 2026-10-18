/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layersmodel.h"
#include "gridsettings.h"
#include "imagesettings.h"
#include "locationsettings.h"
#include "playbacksettings.h"
#include "presentationsettings.h"
#include "subtitlesettings.h"
#ifdef NDI_SUPPORT
#include <ndi/ndilayer.h>
#endif
#ifdef PDF_SUPPORT
#include <layers/pdflayer.h>
#endif
#ifdef SPOUT_SUPPORT
#include <layers/spoutlayer.h>
#endif
#ifdef DIRECTSHOW_SUPPORT
#include <layers/directshowlayer.h>
#endif
#ifdef CAPTURE_SUPPORT
#include <capture/capturelayer.h>
#endif
#include <layers/textlayer.h>
#include <layers/mpvlayer.h>
#include <layers/streamlayer.h>
#include <layers/controllayer.h>
#include <layers/restlayer.h>
#include <layers/tcplayer.h>
#ifdef WEBRTC_LAYER
#include <webrtc/webrtclayer.h>
#endif
#ifdef MULTI_VIDEO_LAYER
#include <layers/multivideolayer.h>
#include <QColor>
#include <QFile>
#endif
#include "httpclientmodel.h"
#include "application.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QScopedValueRollback>
#include <QQuickView>
#include <QQmlEngine>
#include <QMimeDatabase>
#include <QTimer>
#include <QThread>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace {

using cplay::anim::PostEffects;

// Map key of each post effect's bool, its flag and the keys of its parameters.
struct PostEffectInfo {
    const char *key;
    uint32_t flag;
    std::vector<const char *> params;
};

const std::array<PostEffectInfo, 9> &postEffectInfos() {
    using namespace cplay::anim;
    static const std::array<PostEffectInfo, 9> infos = {{
        { "chromaKey", PostChromaKey, { "keyColor", "keySimilarity", "keySmoothness", "keySpill" } },
        { "lumaKey", PostLumaKey, { "lumaThreshold", "lumaSmoothness", "lumaInvert" } },
        { "colorCorrection", PostColorCorrection, { "brightness", "contrast", "saturation", "gamma", "hue" } },
        { "invert", PostInvert, {} },
        { "tint", PostTint, { "tintColor", "tintAmount" } },
        { "blur", PostBlur, { "blurRadius" } },
        { "sharpen", PostSharpen, { "sharpenAmount" } },
        { "pixelate", PostPixelate, { "pixelSize" } },
        { "vignette", PostVignette, { "vignetteRadius", "vignetteSoftness", "vignetteAmount", "vignetteMode" } }
    }};
    return infos;
}

QString colorToString(const glm::vec3 &c) {
    return QColor::fromRgbF(c.r, c.g, c.b).name(QColor::HexRgb);
}

QVariantMap postEffectsToMap(const PostEffects &fx) {
    QVariantMap m;
    for (const PostEffectInfo &info : postEffectInfos())
        m[QString::fromLatin1(info.key)] = (fx.enabled & info.flag) != 0;
    m[QStringLiteral("keyColor")] = colorToString(fx.keyColor);
    m[QStringLiteral("keySimilarity")] = fx.keySimilarity;
    m[QStringLiteral("keySmoothness")] = fx.keySmoothness;
    m[QStringLiteral("keySpill")] = fx.keySpill;
    m[QStringLiteral("lumaThreshold")] = fx.lumaThreshold;
    m[QStringLiteral("lumaSmoothness")] = fx.lumaSmoothness;
    m[QStringLiteral("lumaInvert")] = fx.lumaInvert;
    m[QStringLiteral("brightness")] = fx.brightness;
    m[QStringLiteral("contrast")] = fx.contrast;
    m[QStringLiteral("saturation")] = fx.saturation;
    m[QStringLiteral("gamma")] = fx.gamma;
    m[QStringLiteral("hue")] = fx.hue;
    m[QStringLiteral("tintColor")] = colorToString(fx.tintColor);
    m[QStringLiteral("tintAmount")] = fx.tintAmount;
    m[QStringLiteral("blurRadius")] = fx.blurRadius;
    m[QStringLiteral("sharpenAmount")] = fx.sharpenAmount;
    m[QStringLiteral("pixelSize")] = fx.pixelSize;
    m[QStringLiteral("vignetteRadius")] = fx.vignetteRadius;
    m[QStringLiteral("vignetteSoftness")] = fx.vignetteSoftness;
    m[QStringLiteral("vignetteAmount")] = fx.vignetteAmount;
    m[QStringLiteral("vignetteMode")] = fx.vignetteMode;
    return m;
}

// Applies the keys present in values (as from postEffectsToMap, partial maps allowed).
void applyPostEffectsMap(PostEffects &fx, const QVariantMap &values) {
    for (const PostEffectInfo &info : postEffectInfos()) {
        const QString k = QString::fromLatin1(info.key);
        if (values.contains(k)) {
            if (values.value(k).toBool())
                fx.enabled |= info.flag;
            else
                fx.enabled &= ~info.flag;
        }
    }
    auto readFloat = [&values](const char *key, float &out, float lo, float hi) {
        const QString k = QString::fromLatin1(key);
        if (values.contains(k))
            out = std::clamp(static_cast<float>(values.value(k).toDouble()), lo, hi);
    };
    auto readColor = [&values](const char *key, glm::vec3 &out) {
        const QString k = QString::fromLatin1(key);
        if (!values.contains(k))
            return;
        const QVariant v = values.value(k);
        const QColor c = v.typeId() == QMetaType::QColor ? v.value<QColor>() : QColor::fromString(v.toString());
        if (c.isValid())
            out = glm::vec3(c.redF(), c.greenF(), c.blueF());
    };
    readColor("keyColor", fx.keyColor);
    readFloat("keySimilarity", fx.keySimilarity, 0.f, 1.f);
    readFloat("keySmoothness", fx.keySmoothness, 0.f, 1.f);
    readFloat("keySpill", fx.keySpill, 0.f, 1.f);
    readFloat("lumaThreshold", fx.lumaThreshold, 0.f, 1.f);
    readFloat("lumaSmoothness", fx.lumaSmoothness, 0.f, 1.f);
    if (values.contains(QStringLiteral("lumaInvert")))
        fx.lumaInvert = values.value(QStringLiteral("lumaInvert")).toBool();
    readFloat("brightness", fx.brightness, -1.f, 1.f);
    readFloat("contrast", fx.contrast, 0.f, 3.f);
    readFloat("saturation", fx.saturation, 0.f, 3.f);
    readFloat("gamma", fx.gamma, 0.1f, 4.f);
    readFloat("hue", fx.hue, -180.f, 180.f);
    readColor("tintColor", fx.tintColor);
    readFloat("tintAmount", fx.tintAmount, 0.f, 1.f);
    readFloat("blurRadius", fx.blurRadius, 0.f, 64.f);
    readFloat("sharpenAmount", fx.sharpenAmount, 0.f, 3.f);
    readFloat("pixelSize", fx.pixelSize, 1.f, 256.f);
    readFloat("vignetteRadius", fx.vignetteRadius, 0.f, 1.5f);
    readFloat("vignetteSoftness", fx.vignetteSoftness, 0.f, 1.5f);
    readFloat("vignetteAmount", fx.vignetteAmount, 0.f, 1.f);
    if (values.contains(QStringLiteral("vignetteMode")))
        fx.vignetteMode = std::clamp(values.value(QStringLiteral("vignetteMode")).toInt(), 0, 1);
}

}

static void *get_proc_address_qopengl_v1(void* ctx, const char *name) {
    Q_UNUSED(ctx)

    QOpenGLContext *glctx = QOpenGLContext::globalShareContext();
    if (!glctx)
        return nullptr;

    return reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name)));
}

static void* get_proc_address_qopengl_v2(const char* name, void* ctx) {
    Q_UNUSED(ctx)

        QOpenGLContext* glctx = QOpenGLContext::globalShareContext();
    if (!glctx)
        return nullptr;

    return reinterpret_cast<void*>(glctx->getProcAddress(QByteArray(name)));
}

LayersModel::LayersModel(QObject *parent)
    : QAbstractListModel(parent),
    m_layerTypeModel(new LayersTypeModel(this)),
    m_layerHierachy(BaseLayer::LayerHierarchy::FRONT),
    m_needSync(false),
    m_syncIteration(0),
    m_layersName(QStringLiteral("Untitled")),
    m_layersPath(QStringLiteral("")) {
    // Owned by C++ (QSharedPointer in SlidesModel); prevent the QML engine from deleting it.
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);
    m_timelineTimer = new QTimer(this);
    m_timelineTimer->setInterval(16); // ~60 fps ticks
    connect(m_timelineTimer, &QTimer::timeout, this, &LayersModel::onTimelineTick);
}

LayersModel::~LayersModel() {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    m_order.clear();
    m_refs.clear();
    m_layers.clear();
}

int LayersModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_order.size();
}

QVariant LayersModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_order.empty())
        return QVariant();

    const int row = index.row();
    if (row < 0 || row >= m_order.size())
        return QVariant();

    const BaseLayer *layerItem = rowLayer(row);

    if(layerItem == nullptr)
        return QVariant();

    int stereoVideo = layerItem->stereoMode();
    int gridToMapOn = layerItem->gridMode();

    switch (role) {
    case TitleRole:
        return QVariant(QString::fromStdString(layerItem->title()));
    case PathRole:
        if (layerItem->type() == BaseLayer::TCP) {
            const auto *tcp = static_cast<const TcpLayer *>(layerItem);
            return QString::fromStdString(tcp->commandId());
        }
        if (layerItem->type() == BaseLayer::CONTROL) {
            const ControlLayer* controlLayer = static_cast<const ControlLayer*>(layerItem);
            return QVariant(QString::fromStdString(controlLayer->operation()) + QStringLiteral(":") + QString::fromStdString(controlLayer->parameter()));
        }
        if (layerItem->type() == BaseLayer::REST) {
            const RestLayer* restLayer = static_cast<const RestLayer*>(layerItem);
            return QVariant(QString::fromStdString(restLayer->url()));
        }
        return QVariant(QString::fromStdString(layerItem->filepath()));
    case TypeRole:
        return QVariant(QString::fromStdString(layerItem->typeName()));
    case LockedRole:
        return QVariant(layerItem->isLocked());
    case PageRole:
#ifdef PDF_SUPPORT
        if (layerItem->type() == BaseLayer::PDF){
            const PdfLayer* pdfLayer = static_cast<const PdfLayer*>(layerItem);
            return QVariant(QStringLiteral(" ") + QString::number(pdfLayer->page())
                + QStringLiteral("/") + QString::number(pdfLayer->numPages())  + QStringLiteral(" "));
        }
#endif
        return QVariant(QStringLiteral(""));
    case StereoRole:
        if (stereoVideo == 0) {
            return QVariant(QStringLiteral("2D"));
        } else if (stereoVideo > 0) {
            return QVariant(QStringLiteral("3D"));
        } else {
            return QVariant(QStringLiteral(""));
        }
    case GridRole:
        if (gridToMapOn == 0) {
            return QVariant(QStringLiteral("Split"));
        } else if (gridToMapOn == 1) {
            return QVariant(QStringLiteral("Flat"));
        } else if (gridToMapOn == 2) {
            return QVariant(QStringLiteral("Dome"));
        } else if (gridToMapOn == 3 || gridToMapOn == 4) {
            return QVariant(QStringLiteral("Sphere"));
        } else {
            return QVariant(QStringLiteral(""));
        }
    case StatusRole:
        return QVariant(layerStatus(index.row()));
    case VisibilityRole: {
        // An animating layer shows how far its entrance or exit has played, not its opacity.
        const int progress = layerPlayProgress(layerItem);
        return QVariant(progress >= 0 ? progress : static_cast<int>(layerItem->alpha() * 100.f));
    }
    case NdiOutputRole:
        // The upload icon also covers node streaming over NDI: the layer is then
        // published as an NDI source on the master too.
        return QVariant(layerItem->ndiOutputEnabled()
                        || (layerItem->nodeStreamOutputEnabled() && layerItem->nodeStreamUseNdiEffective()));
    case ExistOnMasterOnlyRole:
        return QVariant(layerItem->existOnMasterOnly());
    case NodeStreamOutputRole:
        return QVariant(layerItem->nodeStreamOutputEnabled());
    case IsReferenceRole:
        return QVariant(isReference(row));
    case ReferenceSourceRole:
        return QVariant(referenceSource(row));
    case HierarchyRole:
        return QVariant(static_cast<int>(layerItem->hierarchy()));
    }

    return QVariant();
}

QHash<int, QByteArray> LayersModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[TitleRole] = "title";
    roles[PathRole] = "filepath";
    roles[TypeRole] = "type";
    roles[LockedRole] = "locked";
    roles[PageRole] = "page";
    roles[StereoRole] = "stereoVideo";
    roles[GridRole] = "gridToMapOn";
    roles[StatusRole] = "status";
    roles[VisibilityRole] = "visibility";
    roles[NdiOutputRole] = "ndiOutput";
    roles[ExistOnMasterOnlyRole] = "existOnMasterOnly";
    roles[NodeStreamOutputRole] = "nodeStreamOutput";
    roles[IsReferenceRole] = "isReference";
    roles[ReferenceSourceRole] = "referenceSource";
    roles[HierarchyRole] = "hierarchy";
    return roles;
}

Layers LayersModel::getLayers() const {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    return m_layers;
}

void LayersModel::setLayers(const Layers &layers) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    beginResetModel();
    m_draggedLayersAwaitingGridGuess.clear();
    m_layers = layers;
    m_order.clear();
    for (const auto &l : m_layers)
        m_order.append(l.first.get());
    for (const auto &r : m_refs)
        m_order.append(r.layer.get());
    endResetModel();
}

BaseLayer::LayerHierarchy LayersModel::hierarchy() const {
    return m_layerHierachy;
}

void LayersModel::setHierarchy(BaseLayer::LayerHierarchy h) {
    m_layerHierachy = h;

    for (auto l : m_layers) {
        if (l.first)
            l.first->setHierarchy(h);
    }

    setNeedSync();
}

int LayersModel::numberOfLayers() {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    return static_cast<int>(m_layers.size());
}

bool LayersModel::needsSync() {
    return m_needSync;
}

void LayersModel::setHasSynced() {
    if (m_syncIteration > 0) {
        m_syncIteration--;
    }
    else {
        m_needSync = false;
    }
}

BaseLayer *LayersModel::layer(int i) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    return rowLayer(i);
}

std::shared_ptr<BaseLayer> LayersModel::layerShared(int i) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (i >= 0 && m_layers.size() > i)
        return m_layers[i].first;
    else
        return std::shared_ptr<BaseLayer>();
}

// ---- References / render order ----------------------------------------------

BaseLayer *LayersModel::rowLayer(int row) const {
    if (row < 0 || row >= m_order.size())
        return nullptr;
    return const_cast<BaseLayer *>(m_order[row]);
}

int LayersModel::ownIndexOf(const BaseLayer *layer) const {
    if (!layer)
        return -1;
    for (int i = 0; i < m_layers.size(); ++i) {
        if (m_layers[i].first.get() == layer)
            return i;
    }
    return -1;
}

int LayersModel::ownIndexOfRow(int row) const {
    return ownIndexOf(rowLayer(row));
}

int LayersModel::referenceIndexOf(const BaseLayer *layer) const {
    if (!layer)
        return -1;
    for (int i = 0; i < m_refs.size(); ++i) {
        if (m_refs[i].layer.get() == layer)
            return i;
    }
    return -1;
}

int LayersModel::rowOfLayer(const BaseLayer *layer) const {
    return layer ? static_cast<int>(m_order.indexOf(layer)) : -1;
}

bool LayersModel::containsLayer(const BaseLayer *layer) const {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    return rowOfLayer(layer) >= 0;
}

int LayersModel::ownLayerStatus(const BaseLayer *layer) const {
    const int own = ownIndexOf(layer);
    return own >= 0 ? m_layers[own].second : -1;
}

int LayersModel::rowForNewOwnLayer() const {
    for (int row = m_order.size() - 1; row >= 0; --row) {
        if (ownIndexOf(m_order[row]) >= 0)
            return row + 1;
    }
    return 0;
}

void LayersModel::syncOwnLayersToOrder() {
    Layers sorted;
    sorted.reserve(m_layers.size());
    for (const BaseLayer *l : m_order) {
        const int own = ownIndexOf(l);
        if (own >= 0)
            sorted.push_back(m_layers[own]);
    }
    if (sorted.size() == m_layers.size())
        m_layers = sorted;
}

bool LayersModel::isReference(int i) const {
    return referenceIndexOf(rowLayer(i)) >= 0;
}

QString LayersModel::referenceSource(int i) const {
    const int ref = referenceIndexOf(rowLayer(i));
    if (ref < 0 || !m_refs[ref].owner)
        return QString();
    return m_refs[ref].owner->getLayersName();
}

int LayersModel::numberOfReferences() const {
    return m_refs.size();
}

std::vector<std::shared_ptr<BaseLayer>> LayersModel::renderOrderShared() const {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    std::vector<std::shared_ptr<BaseLayer>> result;
    result.reserve(m_order.size());
    for (const BaseLayer *l : m_order) {
        const int own = ownIndexOf(l);
        if (own >= 0) {
            result.push_back(m_layers[own].first);
            continue;
        }
        const int ref = referenceIndexOf(l);
        if (ref >= 0)
            result.push_back(m_refs[ref].layer);
    }
    return result;
}

void LayersModel::updateLayerRow(const BaseLayer *layer) {
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, layer]() { updateLayerRow(layer); }, Qt::QueuedConnection);
        return;
    }
    const int row = rowOfLayer(layer);
    if (row >= 0)
        Q_EMIT dataChanged(index(row, 0), index(row, 0));
}

void LayersModel::setReferencedLayers(const QVector<LayerReference> &refs) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);

    bool same = refs.size() == m_refs.size();
    for (int i = 0; same && i < refs.size(); ++i) {
        same = refs[i].layer == m_refs[i].layer && refs[i].owner == m_refs[i].owner
            && refs[i].slideOffset == m_refs[i].slideOffset && refs[i].ownerLayerIdx == m_refs[i].ownerLayerIdx;
    }
    if (same && m_pendingRenderOrder.isEmpty() && m_pendingReferenceTracks.isEmpty())
        return;

    // Keep the order of rows that remain, drop vanished references and put new ones on
    // top (nearest earlier slide first).
    QVector<const BaseLayer *> kept;
    for (const BaseLayer *l : m_order) {
        if (ownIndexOf(l) >= 0) {
            kept.append(l);
            continue;
        }
        for (const auto &r : refs) {
            if (r.layer.get() == l) {
                kept.append(l);
                break;
            }
        }
    }
    QVector<const BaseLayer *> newOrder;
    for (const auto &r : refs) {
        if (r.layer && !kept.contains(r.layer.get()) && !newOrder.contains(r.layer.get()))
            newOrder.append(r.layer.get());
    }
    newOrder.append(kept);

    beginResetModel();
    m_refs = refs;
    m_order = newOrder;
    for (const auto &r : m_refs) {
        if (r.layer)
            m_referenceTrackLayers[r.layer.get()] = r.layer;
    }
    applyPendingRenderOrder();
    applyPendingReferenceTracks();
    ensureTimelineSizeMatchesLayers();
    endResetModel();

    rebuildEffectSchedule();
    setNeedSync();
    Q_EMIT timelineChanged();
}

void LayersModel::applyPendingRenderOrder() {
    if (m_pendingRenderOrder.isEmpty())
        return;

    // Wait until every saved reference can be resolved.
    QVector<const BaseLayer *> saved;
    for (const auto &v : m_pendingRenderOrder) {
        const QString key = v.toString();
        const BaseLayer *layer = nullptr;
        if (key.startsWith(QLatin1Char('L'))) {
            bool ok = false;
            const int own = key.mid(1).toInt(&ok);
            if (ok && own >= 0 && own < m_layers.size())
                layer = m_layers[own].first.get();
        }
        else if (key.startsWith(QLatin1Char('R'))) {
            const QStringList parts = key.mid(1).split(QLatin1Char(':'));
            if (parts.size() == 2) {
                const int offset = parts[0].toInt();
                const int ownerIdx = parts[1].toInt();
                bool found = false;
                for (const auto &r : m_refs) {
                    if (r.slideOffset == offset && r.ownerLayerIdx == ownerIdx) {
                        layer = r.layer.get();
                        found = true;
                        break;
                    }
                }
                if (!found)
                    return;
            }
        }
        if (layer && !saved.contains(layer))
            saved.append(layer);
    }

    // Saved rows first in saved order, then anything not mentioned keeps its relative place.
    QVector<const BaseLayer *> newOrder = saved;
    for (const BaseLayer *l : m_order) {
        if (!newOrder.contains(l))
            newOrder.append(l);
    }
    if (newOrder.size() == m_order.size())
        m_order = newOrder;
    syncOwnLayersToOrder();
    m_pendingRenderOrder = QJsonArray();
}

void LayersModel::applyPendingReferenceTracks() {
    if (m_pendingReferenceTracks.isEmpty())
        return;

    QJsonArray unresolved;
    for (const auto &v : m_pendingReferenceTracks) {
        const QJsonObject o = v.toObject();
        const int offset = o.value(QStringLiteral("slideOffset")).toInt(-1);
        const int ownerIdx = o.value(QStringLiteral("layerIndex")).toInt(-1);
        const LayerReference *match = nullptr;
        for (const auto &r : m_refs) {
            if (r.slideOffset == offset && r.ownerLayerIdx == ownerIdx) {
                match = &r;
                break;
            }
        }
        if (!match || !match->layer) {
            unresolved.push_back(o);
            continue;
        }
        m_referenceTrackLayers[match->layer.get()] = match->layer;
        decodeTrack(o, m_layerTimelines[match->layer.get()]);
    }
    // References appear once all slides are loaded; anything still unresolved then
    // points at a layer that no longer reaches this slide and is dropped.
    m_pendingReferenceTracks = m_refs.isEmpty() ? unresolved : QJsonArray();
}

void LayersModel::setTimelineSuppressedLayers(const QSet<const BaseLayer *> &layers, bool isLive) {
    m_timelineSuppressed = layers;
    m_isLiveSlide = isLive;
}

bool LayersModel::isTimelineSuppressed(const BaseLayer *layer) const {
    return m_timelineSuppressed.contains(layer);
}

void LayersModel::hideUnsuppressedReferences(bool evenWhenLive) {
    if (m_isLiveSlide && !evenWhenLive)
        return;
    for (int row = 0; row < m_order.size(); ++row) {
        if (ownIndexOfRow(row) >= 0)
            continue;
        BaseLayer *layer = rowLayer(row);
        if (!layer || isTimelineSuppressed(layer))
            continue;
        layer->setAlpha(0.f);
        if (isLayerTimelineDriven(layer))
            layer->clearAnimState();
        updateLayer(row);
    }
}

namespace {
bool isPlainAlphaKeyframe(const LayerKeyframe &kf, int timeMs, float alpha) {
    return kf.timeMs == timeMs && kf.alpha == alpha && kf.easing == 0 && !kf.hasRotate
        && !kf.hasTranslate && !kf.hasScale && !kf.hasPlane;
}
}

bool LayersModel::addReferenceOutroFade(const BaseLayer *layer) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (!m_hasTimeline || !hasOutro() || referenceIndexOf(layer) < 0)
        return false;
    // A stored track (also one kept from an earlier span) is the user's; leave it.
    auto it = m_layerTimelines.find(layer);
    if (it != m_layerTimelines.end() && (!it->second.keyframes.isEmpty() || !it->second.effects.isEmpty()))
        return false;

    LayerTimeline &track = m_layerTimelines[layer];
    if (layer->hasTexture()) {
        addDefaultFadeEffects(track, false, true);
    }
    else {
        LayerKeyframe kfStart;
        kfStart.timeMs = 0;
        kfStart.alpha = 1.f;
        LayerKeyframe kfOutro;
        kfOutro.timeMs = m_timelineOutroStart;
        kfOutro.alpha = 1.f;
        LayerKeyframe kfEnd;
        kfEnd.timeMs = m_timelineDuration;
        kfEnd.alpha = 0.f;
        track.keyframes = { kfStart, kfOutro, kfEnd };
    }

    rebuildEffectSchedule();
    const int row = rowOfLayer(layer);
    if (row >= 0)
        Q_EMIT dataChanged(index(row, 0), index(row, 0));
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
    return true;
}

bool LayersModel::removeReferenceOutroFade(const BaseLayer *layer) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (!layer || ownIndexOf(layer) >= 0)
        return false;
    auto it = m_layerTimelines.find(layer);
    if (it == m_layerTimelines.end())
        return false;
    const auto &kfs = it->second.keyframes;
    const auto &effects = it->second.effects;
    const bool defaultEffect = kfs.isEmpty() && effects.size() == 1 && isDefaultOutroFade(effects[0]);
    // Older presentations stored the default fade as keyframes.
    const bool defaultKeyframes = effects.isEmpty() && kfs.size() == 3
        && isPlainAlphaKeyframe(kfs[0], 0, 1.f)
        && isPlainAlphaKeyframe(kfs[1], m_timelineOutroStart, 1.f)
        && isPlainAlphaKeyframe(kfs[2], m_timelineDuration, 0.f);
    if (!defaultEffect && !defaultKeyframes)
        return false;

    m_layerTimelines.erase(it);
    rebuildEffectSchedule();
    const int row = rowOfLayer(layer);
    if (row >= 0)
        Q_EMIT dataChanged(index(row, 0), index(row, 0));
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
    return true;
}

bool LayersModel::hasTrackForLayer(const BaseLayer *layer) const {
    if (!layer)
        return false;
    auto it = m_layerTimelines.find(layer);
    return it != m_layerTimelines.end() && (!activeKeyframes(it->second).isEmpty() || !it->second.effects.isEmpty());
}

void LayersModel::pruneReferenceTracks(const QSet<const BaseLayer *> &aliveLayers) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    for (auto it = m_referenceTrackLayers.begin(); it != m_referenceTrackLayers.end();) {
        if (it->second.expired() || !aliveLayers.contains(it->first)) {
            m_layerTimelines.erase(it->first);
            it = m_referenceTrackLayers.erase(it);
        }
        else {
            ++it;
        }
    }
}

QString LayersModel::layerTitle(int i) const {
    if (const BaseLayer *l = rowLayer(i))
        return QString::fromStdString(l->title());
    return QString();
}

int LayersModel::layerVisibility(int i) const {
    if (const BaseLayer *l = rowLayer(i)) {
        const int progress = layerPlayProgress(l);
        return progress >= 0 ? progress : static_cast<int>(l->alpha() * 100.f);
    }
    return 0;
}

int LayersModel::layerIdx(std::string title) {
    // Returning first with correct name
    QString titleLowCase = QString::fromStdString(title).toLower();
    for (int i = 0; i < m_layers.size(); i++) {
        if (!m_layers[i].first)
            continue;
        if (QString::fromStdString(m_layers[i].first->title()).toLower() == titleLowCase) {
            return i;
        }
    }
    return -1;
}

int LayersModel::layerStatus(int i) const {
    const int own = ownIndexOfRow(i);
    if (own >= 0)
        return m_layers[own].second;
    const int ref = referenceIndexOf(rowLayer(i));
    if (ref >= 0 && m_refs[ref].owner)
        return m_refs[ref].owner->ownLayerStatus(m_refs[ref].layer.get());
    return -1;
}

void LayersModel::setLayerStatus(int i, int status) {
    const int own = ownIndexOfRow(i);
    if (own >= 0) {
        m_layers[own].second = status;
        Q_EMIT dataChanged(index(i, 0), index(i, 0));
    }
}

int LayersModel::minLayerStatus() {
    if (m_layers.isEmpty())
        return -1;

    int minStatus = -1;
    for (int i = 0; i < m_layers.size(); i++) {
        if (!m_layers[i].first)
            continue;
        if (m_layers[i].first->type() == BaseLayer::CONTROL || m_layers[i].first->type() == BaseLayer::REST || m_layers[i].first->type() == BaseLayer::TCP)
            continue;
        if (minStatus == -1)
            minStatus = m_layers[i].second;
        else
            minStatus = std::min(minStatus, m_layers[i].second);
    }
    return minStatus;
}

int LayersModel::maxLayerStatus() {
    if (m_layers.isEmpty())
        return -1;

    int maxStatus = -1;
    for (int i = 0; i < m_layers.size(); i++) {
        if (!m_layers[i].first)
            continue;
        if (m_layers[i].first->type() == BaseLayer::CONTROL || m_layers[i].first->type() == BaseLayer::REST || m_layers[i].first->type() == BaseLayer::TCP)
            continue;
        if (maxStatus == -1)
            maxStatus = m_layers[i].second;
        else
            maxStatus = std::max(maxStatus, m_layers[i].second);
    }
    return maxStatus;
}

int LayersModel::addLayer(QString title, int type, QString filepath, int stereoMode, int gridMode) {
    // Serialize against the render thread, which iterates m_layers every frame
    // (SlidesQtItemRenderer::update -> runRenderOnLayersThatShouldUpdate). Adding
    // several layers via drag-and-drop used to realloc the QList underneath the
    // render thread, dangling its reference and crashing the app.
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    // Create new layer
    BaseLayer *newLayer = BaseLayer::createLayer(true, type, get_proc_address_qopengl_v1, get_proc_address_qopengl_v2, title.toStdString());

    if (newLayer) {
        newLayer->setHierarchy(hierarchy());
        newLayer->setTitle(title.toStdString());
        if (type == BaseLayer::CONTROL) {
            ControlLayer* newControlLayer = static_cast<ControlLayer*>(newLayer);
            // filepath holds "operation:parameter"
            int sepIdx = filepath.indexOf(QStringLiteral(":"));
            if (sepIdx >= 0) {
                newControlLayer->setOperation(filepath.left(sepIdx).toStdString());
                newControlLayer->setParameter(filepath.mid(sepIdx + 1).toStdString());
            }
            else {
                newControlLayer->setOperation(filepath.toStdString());
            }
        }
#ifdef TEXT_LAYER
        else if (type == BaseLayer::TEXT) {
            TextLayer* newTextLayer = static_cast<TextLayer*>(newLayer);
            newTextLayer->setFont(SubtitleSettings::subtitleFontFamily().toStdString());
            newTextLayer->setFontSize(SubtitleSettings::subtitleFontSize());
            newTextLayer->setTextureSize(SubtitleSettings::subtitleTextureWidth(), SubtitleSettings::subtitleTextureHeight());
            newTextLayer->setAlignment(SubtitleSettings::subtitleAlignment());
            QColor textColor(SubtitleSettings::subtitleColor());
            newTextLayer->setColor(textColor.name().toStdString(), textColor.redF(), textColor.greenF(), textColor.blueF());
            newTextLayer->setText(filepath.toStdString());
        }
#endif
        else if (newLayer->type() == BaseLayer::TCP) {
            auto *tcp = static_cast<TcpLayer *>(newLayer);
            tcp->setCommandId(filepath.toStdString());
            if (Application::isCreated())
                tcp->setManager(Application::instance().tcpControlManager());
        }
        else if (newLayer->type() == BaseLayer::REST && Application::isCreated()) {
            RestLayer* newRestLayer = static_cast<RestLayer*>(newLayer);
            newRestLayer->setHttpClientModel(Application::instance().httpClientModel());
            newRestLayer->setUrl(filepath.toStdString());
        }
#ifdef MULTI_VIDEO_LAYER
        else if (newLayer->type() == BaseLayer::MULTIVIDEO) {
            // filepath is the path to the composition JSON
            newLayer->setFilePath(filepath.toStdString());
            // Read JSON content and store it for sync to nodes
            QFile f(filepath);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const std::string jsonContent = f.readAll().toStdString();
                f.close();
                static_cast<MultiVideoLayer*>(newLayer)->setCompositionJson(jsonContent);
            }
        }
#endif
#ifdef DIRECTSHOW_SUPPORT
        else if (type == BaseLayer::DIRECTSHOW) {
            // filepath holds "videoDevice|audioDevice" when capture devices were chosen in the UI,
            // or a plain media file path otherwise. '|' cannot occur in NTFS file names, so the
            // two forms are unambiguous.
            DirectShowLayer* newDirectShowLayer = static_cast<DirectShowLayer*>(newLayer);
            const int sepIdx = filepath.indexOf(QChar::fromLatin1('|'));
            if (sepIdx >= 0) {
                newDirectShowLayer->setCaptureDevices(filepath.left(sepIdx).toStdString(),
                                                      filepath.mid(sepIdx + 1).toStdString());
            }
            newLayer->setFilePath(filepath.toStdString());
        }
#endif
        else {
            newLayer->setFilePath(filepath.toStdString());
        }
        newLayer->setStereoMode(static_cast<uint8_t>(stereoMode));
        newLayer->setGridMode(static_cast<uint8_t>(gridMode));
        newLayer->setAlpha(static_cast<float>(PresentationSettings::defaultLayerVisibility()) * 0.01f);
        newLayer->setPlaneElevation(GridSettings::plane_Elevation_Degrees());
        newLayer->setPlaneDistance(GridSettings::plane_Distance_CM());
        newLayer->setPlaneSize(glm::vec2(GridSettings::plane_Width_CM(), GridSettings::plane_Height_CM()), 
            static_cast<uint8_t>(GridSettings::plane_Calculate_Size_Based_on_Video()));
        // Default the no-signal image on when one is configured for this layer type.
        bool useNoSignalImage = false;
#ifdef CAPTURE_SUPPORT
        useNoSignalImage |= type == BaseLayer::CAPTURE && !ImageSettings::noSignalImageForCapture().isEmpty();
#endif
#ifdef DIRECTSHOW_SUPPORT
        useNoSignalImage |= type == BaseLayer::DIRECTSHOW && !ImageSettings::noSignalImageForDirectShow().isEmpty();
#endif
        if (useNoSignalImage)
            newLayer->setUseNoSignalImage(true);
        newLayer->initialize();

        const int layerIdx = rowForNewOwnLayer();
        int initialStatus = (newLayer->type() == BaseLayer::REST || newLayer->type() == BaseLayer::TCP) ? -1 : 0;
        beginInsertRows(QModelIndex(), layerIdx, layerIdx);
        m_layers.push_back(QPair(std::shared_ptr<BaseLayer>(newLayer), initialStatus));
        m_order.insert(layerIdx, newLayer);
        ensureTimelineSizeMatchesLayers();
        endInsertRows();
        addDefaultFadeForNewLayer(layerIdx);

        // Set up status callback for REST layers
        if (newLayer->type() == BaseLayer::REST) {
            RestLayer* restLayer = static_cast<RestLayer*>(newLayer);
            // Resolve the row at delivery time: layers may have been moved or removed since.
            restLayer->setStatusCallback([this, newLayer](int status) {
                QMetaObject::invokeMethod(this, [this, newLayer, status]() {
                    const int row = rowOfLayer(newLayer);
                    if (row >= 0)
                        setLayerStatus(row, status);
                }, Qt::QueuedConnection);
            });
        }

        setLayersNeedsSave(true);
        setNeedSync();

        Q_EMIT layersModelChanged();

        return layerIdx;
    }

    return -1;
}

int LayersModel::addRestLayer(QString title, QString url, int method, QString parameters, bool ignoreStatus) {
    const int layerIdx = addLayer(title, BaseLayer::REST, url, 0, 0);
    if (layerIdx < 0) {
        return -1;
    }

    RestLayer* restLayer = dynamic_cast<RestLayer*>(rowLayer(layerIdx));
    if (!restLayer)
        return -1;
    restLayer->setMethod(method);
    restLayer->setParameters(parameters.toStdString());
    restLayer->setIgnoreStatus(ignoreStatus);

    setLayersNeedsSave(true);
    return layerIdx;
}

int LayersModel::addTcpLayer(const QString &title, const QString &commandId) {
    return addLayer(title, BaseLayer::TCP, commandId, 0, 0);
}

int LayersModel::getLayerTypeBasedOnMime(QUrl fileUrl) {
    QMimeDatabase db;
    QMimeType type = db.mimeTypeForUrl(fileUrl);
    QString typeName = type.name();

    if (typeName.startsWith(QStringLiteral("video/"))) {
        return BaseLayer::LayerType::VIDEO;
    }
    else if (typeName.startsWith(QStringLiteral("audio/"))) {
        return BaseLayer::LayerType::AUDIO;
    }
    else if (typeName == QStringLiteral("image/png")
        || typeName == QStringLiteral("image/jpeg")
        || typeName == QStringLiteral("image/tga")) {
        return BaseLayer::LayerType::IMAGE;
    }
    else if (typeName == QStringLiteral("application/pdf")) {
#ifdef PDF_SUPPORT
        return BaseLayer::LayerType::PDF;
#endif // PDF_SUPPORT
    }
    return BaseLayer::LayerType::INVALID;
}

int LayersModel::addLayerBasedOnMime(QUrl fileUrl) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    int type = getLayerTypeBasedOnMime(fileUrl);
    if (type == BaseLayer::LayerType::INVALID)
        return -1;

    QString filePath = fileUrl.toLocalFile();
    if (filePath.isEmpty())
        return -1;

    QFileInfo fileInfo(filePath);
    const int layerIdx = addLayer(fileInfo.baseName(), type, filePath, PresentationSettings::defaultStereoModeForLayers(), PresentationSettings::defaultGridModeForLayers());
    if (layerIdx >= 0
        && PresentationSettings::guessGridModeOnDragAndDrop()
        && (type == BaseLayer::LayerType::IMAGE || type == BaseLayer::LayerType::VIDEO)) {
        BaseLayer *layer = rowLayer(layerIdx);
        layer->setShouldPreLoad(true);
        m_draggedLayersAwaitingGridGuess.insert(layer);
    }

    return layerIdx;
}

void LayersModel::removeLayer(int i) {
    takeLayer(i);
}

RemovedLayer LayersModel::takeLayer(int i) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    RemovedLayer removed;
    removed.row = i;
    const int ref = referenceIndexOf(rowLayer(i));
    if (ref >= 0) {
        // A reference is removed by ending the owner's visibility span before this slide.
        // SlidesModel rebuilds the references of all slides on layersModelChanged.
        const LayerReference r = m_refs[ref];
        if (r.layer && r.owner && !(r.owner->getLayersCanBeLocked() && r.layer->isLocked())) {
            removed.reference = r.layer;
            removed.referenceOwner = r.owner;
            removed.previousKeepVisibility = r.layer->keepVisibilityForNumSlides();
            r.layer->setKeepVisibilityForNumSlides(std::max(0, r.slideOffset - 1));
            r.owner->setLayersNeedsSave(true);
            Q_EMIT layersModelChanged();
            Q_EMIT layerVisibilitySpanChanged(r.layer.get());
        }
        return removed;
    }

    const int own = ownIndexOfRow(i);
    if (own < 0 || !m_layers.at(own).first)
        return removed;

    if (getLayersCanBeLocked() && m_layers.at(own).first->isLocked())
        return removed;

    // No relative paths, so the snapshot does not depend on where the presentation is saved.
    removed.data = encodeLayerToJSON(m_layers[own].first, QStringList());

    m_layers[own].first->setEnabled(false);
    m_draggedLayersAwaitingGridGuess.remove(m_layers[own].first.get());
    m_layerTimelines.erase(m_layers[own].first.get());

    beginRemoveRows(QModelIndex(), i, i);
    m_order.removeAt(i);
    m_layers.removeAt(own);
    endRemoveRows();
    rebuildEffectSchedule();
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
    return removed;
}

bool LayersModel::restoreLayer(const RemovedLayer &removed) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (const std::shared_ptr<BaseLayer> layer = removed.reference.lock()) {
        if (!removed.referenceOwner)
            return false;
        // Extending the owner's span again brings the reference back (see takeLayer).
        layer->setKeepVisibilityForNumSlides(removed.previousKeepVisibility);
        removed.referenceOwner->setLayersNeedsSave(true);
        Q_EMIT layersModelChanged();
        Q_EMIT layerVisibilitySpanChanged(layer.get());
        return true;
    }
    if (removed.data.isEmpty())
        return false;

    int row = -1;
    {
        // Decoded like a loaded layer: its saved animation track, not a default fade.
        const QScopedValueRollback<bool> decoding(m_decodingLayers, true);
        row = decodeLayerFromJSON(removed.data, QStringList());
    }
    if (row < 0)
        return false;
    if (removed.row >= 0 && removed.row < m_order.size() && removed.row != row)
        moveLayer(row, removed.row);

    rebuildEffectSchedule();
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
    return true;
}

void LayersModel::moveLayer(int i, int t) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (i < 0 || t < 0 || i >= m_order.size() || t >= m_order.size() || i == t) {
        Q_EMIT dataChanged(index(i, 0), index(t, 0));
        Q_EMIT layersModelChanged();
        return;
    }

    int destinationRow = (t > i) ? (t + 1) : t;

    if (!beginMoveRows(QModelIndex(), i, i, QModelIndex(), destinationRow)) {
        Q_EMIT dataChanged(index(i, 0), index(t, 0));
        Q_EMIT layersModelChanged();
        return;
    }

    m_order.move(i, t);
    syncOwnLayersToOrder();
    endMoveRows();

    Q_EMIT dataChanged(index(t, 0), index(t, 0));
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerTop(int i) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (i < 1 || i >= m_order.size())
        return;
    if (!beginMoveRows(QModelIndex(), i, i, QModelIndex(), 0))
        return;
    m_order.move(i, 0);
    syncOwnLayersToOrder();
    endMoveRows();
    for (int j = 0; j < i; j++)
        updateLayer(j);
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerUp(int i) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (i < 1 || i >= m_order.size())
        return;
    if (!beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1))
        return;
    m_order.move(i, i - 1);
    syncOwnLayersToOrder();
    endMoveRows();
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerDown(int i) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (i < 0 || i >= (m_order.size() - 1))
        return;
    if (!beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i))
        return;
    m_order.move(i, i + 1);
    syncOwnLayersToOrder();
    endMoveRows();
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerBottom(int i) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (i < 0 || i >= (m_order.size() - 1))
        return;
    if (!beginMoveRows(QModelIndex(), i, i, QModelIndex(), m_order.size()))
        return;
    m_order.move(i, m_order.size() - 1);
    syncOwnLayersToOrder();
    endMoveRows();
    for (int j = i; j < m_order.size(); j++)
        updateLayer(j);
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::updateLayer(int i) {
    if (i < 0 || i >= m_order.size())
        return;

    // Also called from the render thread (layer status changes and the grid-mode
    // guess for dragged-and-dropped layers). Emitting model signals straight into a
    // QML ListView from that thread is not allowed and was a source of stalls and
    // crashes when several layers were dropped at once, so bounce the notification
    // onto the thread the model lives on.
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, i]() { updateLayer(i); }, Qt::QueuedConnection);
        return;
    }

    Q_EMIT dataChanged(index(i, 0), index(i, 0));
}

void LayersModel::lockLayer(int i) {
    const int own = ownIndexOfRow(i);
    if (own < 0 || !m_layers[own].first)
        return;

    m_layers[own].first->setIsLocked(true);
    updateLayer(i);
    setLayersNeedsSave(true);
}

void LayersModel::unlockLayer(int i) {
    const int own = ownIndexOfRow(i);
    if (own < 0 || !m_layers[own].first)
        return;

    m_layers[own].first->setIsLocked(false);
    updateLayer(i);
    setLayersNeedsSave(true);
}

bool LayersModel::isLocked(int i) {
    const int own = ownIndexOfRow(i);
    if (!getLayersCanBeLocked() || own < 0 || !m_layers[own].first)
        return false;

    return m_layers[own].first->isLocked();
}

int LayersModel::layerHierarchy(int i) const {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    const BaseLayer *l = rowLayer(i);
    return l ? static_cast<int>(l->hierarchy()) : -1;
}

void LayersModel::setLayerHierarchy(int i, int h) {
    if (h != BaseLayer::LayerHierarchy::BACK && h != BaseLayer::LayerHierarchy::FRONT)
        return;
    {
        std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
        const int own = ownIndexOfRow(i);
        if (own < 0 || !m_layers[own].first || m_layers[own].first->hierarchy() == h)
            return;
        m_layers[own].first->setHierarchy(static_cast<BaseLayer::LayerHierarchy>(h));
    }
    setNeedSync();
    updateLayer(i);
    setLayersNeedsSave(true);
}

void LayersModel::clearLayers() {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    // References are kept: they belong to earlier slides.
    beginResetModel();
    if (getLayersCanBeLocked()) {
        //Keep locked layers (in their order), remove unlocked layers
        Layers locked;
        const Layers snapshot = m_layers; // setEnabled callbacks may modify m_layers
        for (const auto &entry : snapshot) {
            if (entry.first && entry.first->isLocked()) {
                locked.push_back(entry);
            }
            else if (entry.first) {
                entry.first->setEnabled(false);
                m_draggedLayersAwaitingGridGuess.remove(entry.first.get());
            }
        }
        m_layers = locked;

        if (m_layers.isEmpty())
            setLayersNeedsSave(false);
        else
            setLayersNeedsSave(true);
    }
    else {
        const Layers snapshot = m_layers; // setEnabled callbacks may modify m_layers
        for (const auto &entry : snapshot) {
            if (entry.first)
                entry.first->setEnabled(false);
        }
        m_layers.clear();
        m_draggedLayersAwaitingGridGuess.clear();
        setLayersNeedsSave(false);
    }
    m_order.clear();
    for (const auto &l : m_layers)
        m_order.append(l.first.get());
    for (const auto &r : m_refs)
        m_order.append(r.layer.get());
    endResetModel();

    ensureTimelineSizeMatchesLayers();
    rebuildEffectSchedule();
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::setLayersVisibility(int value, bool propagateDown) {
    m_layersVisibility = value;
    // Manual visibility shows layers as they are, without a stale animation pose.
    if (propagateDown && !m_timelineRunning && !m_outroRunning) {
        stopLayerAnimations();
        clearAllAnimStates();
    }
    for (int i = 0; i < m_layers.size(); i++) {
        if (!m_layers[i].first)
            continue;
        if (propagateDown) {
            m_layers[i].first->setAlpha(static_cast<float>(value) * 0.01f);
        }
    }
    // Also refreshes referenced rows, whose visibility follows the slide transitions.
    for (int i = 0; i < m_order.size(); i++)
        updateLayer(i);
    Q_EMIT layersVisibilityChanged();
    setNeedSync();
}

int LayersModel::getLayersVisibility() {
    return m_layersVisibility;
}

void LayersModel::setLayersEnabled(bool value) {
    m_layersEnabled = value;
    Q_EMIT layersEnabledChanged(value);
}

bool LayersModel::getLayersEnabled() {
    return m_layersEnabled;
}

void LayersModel::setLayersNeedsSave(bool value) {
    // Also reached from the render thread via guessGridModeForDraggedLayer() for
    // dragged-and-dropped layers. The signal is connected straight into QML, so
    // emit it from the thread the model lives on instead.
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, [this, value]() { setLayersNeedsSave(value); }, Qt::QueuedConnection);
        return;
    }

    m_layersNeedsSave = value;
    Q_EMIT layersNeedsSaveChanged(value);
}

bool LayersModel::getLayersNeedsSave() {
    return m_layersNeedsSave;
}

void LayersModel::setLayerToCopyIdx(int value) {
    m_layerToCopyIdx = value;
}

int LayersModel::getLayerToCopyIdx() {
    return m_layerToCopyIdx;
}

std::shared_ptr<BaseLayer> LayersModel::getLayerToCopy() {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    const int own = ownIndexOfRow(m_layerToCopyIdx);
    if (own >= 0 && m_layers[own].first)
        return m_layers[own].first;
    const int ref = referenceIndexOf(rowLayer(m_layerToCopyIdx));
    if (ref >= 0)
        return m_refs[ref].layer;

    return nullptr;
}

void LayersModel::addCopyOfLayer(BaseLayer* srcLayer) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    if (srcLayer == nullptr)
        return;

    // Create new layer
    BaseLayer* newLayer = BaseLayer::createLayer(true, srcLayer->type(), get_proc_address_qopengl_v1, get_proc_address_qopengl_v2, srcLayer->title());

    if (newLayer) {
        newLayer->setTitle(srcLayer->title());

        std::vector<std::byte> data;
        srcLayer->encodeFull(data);

        unsigned int pos = 0;
        newLayer->decodeFull(data, pos);

        newLayer->setHierarchy(hierarchy());
        newLayer->initialize();

        const int layerIdx = rowForNewOwnLayer();
        beginInsertRows(QModelIndex(), layerIdx, layerIdx);
        m_layers.push_back(QPair(std::shared_ptr<BaseLayer>(newLayer), 0));
        m_order.insert(layerIdx, newLayer);
        ensureTimelineSizeMatchesLayers();
        endInsertRows();
        addDefaultFadeForNewLayer(layerIdx);

        setLayersNeedsSave(true);
        setNeedSync();

        Q_EMIT layersModelChanged();
    }
}

void LayersModel::overwriteLayerProperties(BaseLayer* srcLayer, int dstLayerIdx) {
    if (srcLayer == nullptr)
        return;

    BaseLayer *dstLayer = rowLayer(dstLayerIdx);
    if (!dstLayer || dstLayer == srcLayer)
        return;

    std::vector<std::byte> data;
    srcLayer->encodeBaseProperties(data);

    if(dstLayer->type() == srcLayer->type())
        srcLayer->encodeTypeProperties(data);

    unsigned int pos = 0;
    dstLayer->decodeBaseProperties(data, pos);

    if (dstLayer->type() == srcLayer->type())
        dstLayer->decodeTypeProperties(data, pos);

    updateLayer(dstLayerIdx);
    setLayersNeedsSave(true);
}

LayersTypeModel *LayersModel::layersTypeModel() {
    return m_layerTypeModel;
}

void LayersModel::setLayersName(QString name) {
    m_layersName = name;
    Q_EMIT layersNameChanged();
}

QString LayersModel::getLayersName() const {
    return m_layersName;
}

QString LayersModel::getLayersNameShort(int maxChars) const {
    if (maxChars <= 0)
        return QString();

    const int countChars = m_layersName.size();
    if (countChars <= maxChars) {
        return m_layersName;
    }
    else if (maxChars <= 4) {
        return m_layersName.left(maxChars);
    }
    else {
        QString shortendLayersName = m_layersName;
        shortendLayersName.erase(shortendLayersName.end() - (countChars - maxChars + 4), shortendLayersName.end());
        shortendLayersName.push_back(QStringLiteral("... "));
        return shortendLayersName;
    }
}

void LayersModel::setLayersCanBeLocked(bool locked) {
    m_layersCanBeLocked = locked;
    Q_EMIT layersCanBeLockedChanged();
}

bool LayersModel::getLayersCanBeLocked() const {
    if ((m_layerHierachy == BaseLayer::LayerHierarchy::BACK
        && PresentationSettings::masterSlideCanHaveLockableLayers())
        ||  (m_layerHierachy == BaseLayer::LayerHierarchy::FRONT
        && PresentationSettings::customSlidesCanHaveLockableLayers())) {
        return m_layersCanBeLocked;
    }
    else {
        return false;
    }
}

int LayersModel::getLockedLayerCount() const {
    int count = 0;
    for (int i = 0; i < m_layers.size(); i++) {
        if (m_layers[i].first && m_layers[i].first->isLocked()) {
            count++;
        }
    }
    return count;
}

void LayersModel::setLayersPath(QString path) {
    m_layersPath = path;
}

QString LayersModel::getLayersPath() const {
    return m_layersPath;
}

QUrl LayersModel::getLayersPathAsURL() const {
    return QUrl(QStringLiteral("file:///") + m_layersPath);
}

std::string LayersModel::getLayersAsFormattedString(size_t charsPerItem) const {
    std::string fullItemList = "";
    for (int i = 0; i < m_layers.size(); i++) {
        std::string title = std::to_string(i + 1) + ". ";

        if (m_layers[i].first)
            title += m_layers[i].first->title();

        size_t countChars = title.size();
        if (countChars < charsPerItem) {
            title.insert(title.end(), charsPerItem - countChars, ' ');
        }
        else if (charsPerItem > 4 && countChars >= charsPerItem) {
            title.erase(title.end() - (countChars - charsPerItem + 4), title.end());
            title.insert(title.end(), 3, '.');
            title.insert(title.end(), 1, ' ');
        }

        fullItemList += title;
        if (i < m_layers.size() - 1)
            fullItemList += "\n";
    }
    return fullItemList;
}

QString LayersModel::checkAndCorrectPath(const QString &filePath, const QStringList &searchPaths) {
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

        // Maybe from network share?
        QString sharefilePath = filePath;
        sharefilePath.replace(QStringLiteral("file://"), QStringLiteral("\\\\"));
        fileInfo = QFileInfo(sharefilePath);
        if (fileInfo.exists())
            return sharefilePath;
    }
    return QStringLiteral("");
}

QString LayersModel::makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider) {
    // Assuming filePath is absolute
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            return foundDir.relativeFilePath(filePath);
        }
    }
    return filePath;
}

int LayersModel::decodeLayerFromJSON(const QJsonObject &o, const QStringList &forRelativePaths) {
    if (o.contains(QStringLiteral("type"))) {
        int type = 0;
        QString typeStr = o.value(QStringLiteral("type")).toString();
        for (int i = 1; i != (int)BaseLayer::LayerType::INVALID; i++) {
            if (typeStr == QString::fromStdString(BaseLayer::typeDescription((BaseLayer::LayerType)i))) {
                type = i;
            }
        }

        if (type > 0 && o.contains(QStringLiteral("title")) && o.contains(QStringLiteral("path")) && o.contains(QStringLiteral("grid")) && o.contains(QStringLiteral("stereoscopic"))) {
            QString title = o.value(QStringLiteral("title")).toString();

            QString path = o.value(QStringLiteral("path")).toString();
            if (type == BaseLayer::IMAGE
                || type == BaseLayer::VIDEO
#ifdef  PDF_SUPPORT
                || type == BaseLayer::PDF
#endif //  PDF_SUPPORT
                || type == BaseLayer::AUDIO) {
                path = checkAndCorrectPath(path, forRelativePaths);
            }
            else if (type == BaseLayer::CONTROL) {
                // For Control layers, path holds "operation:parameter"
                // Read operation and parameter from dedicated fields if present
                QString operation = o.value(QStringLiteral("operation")).toString();
                QString parameter = o.value(QStringLiteral("parameter")).toString();
                if (!operation.isEmpty()) {
                    path = operation + QStringLiteral(":") + parameter;
                }
            }
            else if (type == BaseLayer::REST) {
                // For REST layers, read url from dedicated field if present
                if (o.contains(QStringLiteral("url"))) {
                    path = o.value(QStringLiteral("url")).toString();
                }
            }
            else if (type == BaseLayer::TCP) {
                path = o.value(QStringLiteral("tcpCommandId")).toString(path);
            }

            int grid = PresentationSettings::defaultGridModeForLayers();
            QString gridStr = o.value(QStringLiteral("grid")).toString();
            if (gridStr == QStringLiteral("none") || gridStr == QStringLiteral("pre-split")) {
                grid = BaseLayer::GridMode::None;
            }
            else if (gridStr == QStringLiteral("plane") || gridStr == QStringLiteral("flat")) {
                grid = BaseLayer::GridMode::Plane;
            }
            else if (gridStr == QStringLiteral("dome")) {
                grid = BaseLayer::GridMode::Dome;
            }
            else if (gridStr == QStringLiteral("sphere") || gridStr == QStringLiteral("eqr") || gridStr == QStringLiteral("sphere-eqr")) {
                grid = BaseLayer::GridMode::Sphere_EQR;
            }
            else if (gridStr == QStringLiteral("eac") || gridStr == QStringLiteral("sphere-eac")) {
                grid = BaseLayer::GridMode::Sphere_EAC;
            }

            int stereo = PresentationSettings::defaultStereoModeForLayers();
            QString stereoStr = o.value(QStringLiteral("stereoscopic")).toString();
            if (stereoStr == QStringLiteral("no") || stereoStr == QStringLiteral("mono")) {
                stereo = BaseLayer::StereoMode::No_2D;
            }
            else if (stereoStr == QStringLiteral("yes") || stereoStr == QStringLiteral("sbs") || stereoStr == QStringLiteral("side-by-side")) {
                stereo = BaseLayer::StereoMode::SBS_3D;
            }
            else if (stereoStr == QStringLiteral("tb") || stereoStr == QStringLiteral("top-bottom")) {
                stereo = BaseLayer::StereoMode::TB_3D;
            }
            else if (stereoStr == QStringLiteral("tbf") || stereoStr == QStringLiteral("top-bottom-flip")) {
                stereo = BaseLayer::StereoMode::TBF_3D;
            }

            const int row = addLayer(title, type, path, stereo, grid);
            const int idx = ownIndexOfRow(row);

            if (idx < 0)
                return -1;

#ifdef TEXT_LAYER
            if (type == BaseLayer::TEXT) {
                std::shared_ptr<TextLayer> textLayer = std::static_pointer_cast<TextLayer>(m_layers[idx].first);
                if (o.contains(QStringLiteral("text"))) {
                    std::string text = o.value(QStringLiteral("text")).toString().toStdString();
                    textLayer->setText(text);
                }
                if (o.contains(QStringLiteral("font"))) {
                    std::string font = o.value(QStringLiteral("font")).toString().toStdString();
                    textLayer->setFont(font);
                }
                if (o.contains(QStringLiteral("size"))) {
                    int size = o.value(QStringLiteral("size")).toInt();
                    textLayer->setFontSize(size);
                }
                if (o.contains(QStringLiteral("width")) && o.contains(QStringLiteral("height"))) {
                    int w = o.value(QStringLiteral("width")).toInt();
                    int h = o.value(QStringLiteral("height")).toInt();
                    textLayer->setTextureSize(w, h);
                }
                if (o.contains(QStringLiteral("alignment"))) {
                    std::string alignment = o.value(QStringLiteral("alignment")).toString().toStdString();
                    textLayer->setAlignmentFromStr(alignment);
                }
                if (o.contains(QStringLiteral("color"))) {
                    QString color = o.value(QStringLiteral("color")).toString();
                    QColor textColor(color);
                    textLayer->setColor(color.toStdString(), textColor.redF(), textColor.greenF(), textColor.blueF());
                }
            }
#endif
#ifdef PDF_SUPPORT
            if (type == BaseLayer::PDF) {
                std::shared_ptr<PdfLayer> pdfLayer = std::static_pointer_cast<PdfLayer>(m_layers[idx].first);
                if (o.contains(QStringLiteral("numPages"))) {
                    int numPages = o.value(QStringLiteral("numPages")).toInt();
                    pdfLayer->setNumPages(numPages);
                }

                if (o.contains(QStringLiteral("page"))) {
                    int page = o.value(QStringLiteral("page")).toInt();
                    pdfLayer->setPage(page);
                }
            }  
#endif

            if (type == BaseLayer::REST) {
                RestLayer* restLayer = static_cast<RestLayer*>(m_layers[idx].first.get());
                if (o.contains(QStringLiteral("method"))) {
                    int method = o.value(QStringLiteral("method")).toInt();
                    restLayer->setMethod(method);
                }
                if (o.contains(QStringLiteral("parameters"))) {
                    std::string params = o.value(QStringLiteral("parameters")).toString().toStdString();
                    restLayer->setParameters(params);
                }
                // Backward compatibility: read old body+contentType as parameters
                if (!o.contains(QStringLiteral("parameters")) && o.contains(QStringLiteral("requestBody"))) {
                    std::string body = o.value(QStringLiteral("requestBody")).toString().toStdString();
                    restLayer->setParameters(body);
                }
                restLayer->setIgnoreStatus(o.value(QStringLiteral("ignoreStatus")).toBool(false));
            }

            if (type == BaseLayer::STREAM && o.contains(QStringLiteral("streamKey"))) {
                static_cast<StreamLayer*>(m_layers[idx].first.get())->setStreamKey(o.value(QStringLiteral("streamKey")).toString().toStdString());
            }

#ifdef DIRECTSHOW_SUPPORT
            if (type == BaseLayer::DIRECTSHOW) {
                // Restore the predefined setup key so each machine can re-resolve its own local capture devices.
                if (o.contains(QStringLiteral("directShowPreset"))) {
                    std::string presetKey = o.value(QStringLiteral("directShowPreset")).toString().toStdString();
                    static_cast<DirectShowLayer*>(m_layers[idx].first.get())->setPresetKey(presetKey);
                }
            }
#endif
            if (o.contains(QStringLiteral("captureNode")))
                m_layers[idx].first->setTargetNode(o.value(QStringLiteral("captureNode")).toString().toStdString());
#ifdef CAPTURE_SUPPORT
            if (type == BaseLayer::CAPTURE && o.contains(QStringLiteral("capturePreset"))) {
                // Each machine re-resolves its own local capture source from this setup title.
                static_cast<CaptureLayer*>(m_layers[idx].first.get())->setPresetKey(o.value(QStringLiteral("capturePreset")).toString().toStdString());
            }
#endif

            if (getLayersCanBeLocked() && o.contains(QStringLiteral("locked"))) {
                bool locked = o.value(QStringLiteral("locked")).toBool();
                m_layers[idx].first->setIsLocked(locked);
            }

            if (o.contains(QStringLiteral("visibility"))) {
                int visibility = o.value(QStringLiteral("visibility")).toInt();
                m_layers[idx].first->setAlpha(static_cast<float>(visibility) * 0.01f);
            }

            if (o.contains(QStringLiteral("volume"))) {
                int volume = o.value(QStringLiteral("volume")).toInt();
                m_layers[idx].first->setVolume(volume);
            }

            if (o.contains(QStringLiteral("audioId"))) {
                int audioId = o.value(QStringLiteral("audioId")).toInt();
                m_layers[idx].first->setAudioId(audioId);
            }

            if (o.contains(QStringLiteral("qrCodeDetection"))) {
                bool qrEnabled = o.value(QStringLiteral("qrCodeDetection")).toBool();
                m_layers[idx].first->setQRCodeDetectionEnabled(qrEnabled);
            }

            if (o.contains(QStringLiteral("textureDivisionMode"))) {
                int divMode = o.value(QStringLiteral("textureDivisionMode")).toInt();
                m_layers[idx].first->setTextureDivisionMode(divMode);
            }

            if (o.contains(QStringLiteral("textureDivisionGrid"))) {
                int divGrid = o.value(QStringLiteral("textureDivisionGrid")).toInt();
                m_layers[idx].first->setTextureDivisionGrid(divGrid);
            }

            // Projects from before merge layouts placed each cell on its own.
            if (o.contains(QStringLiteral("textureDivisionMergeColumns"))) {
                m_layers[idx].first->setTextureDivisionMerge(
                    o.value(QStringLiteral("textureDivisionMergeColumns")).toInt(),
                    o.value(QStringLiteral("textureDivisionMergeRows")).toInt());
            } else if (o.contains(QStringLiteral("divisionSubLayers"))) {
                m_layers[idx].first->setTextureDivisionMerge(0, 0);
            }
            // Merged cells take their grid parameters from this layer.
            const bool divisionMerged = m_layers[idx].first->textureDivisionMergeColumns() > 0;

            // Restore sublayer properties for division mode
            if (o.contains(QStringLiteral("divisionSubLayers")) && m_layers[idx].first->textureDivisionMode() == 2) {
                if (!m_layers[idx].first->getSubLayers().empty()) {
                    QJsonArray subArray = o.value(QStringLiteral("divisionSubLayers")).toArray();
                    auto& subs = m_layers[idx].first->getSubLayers();
                    for (int si = 0; si < subArray.size() && si < static_cast<int>(subs.size()); ++si) {
                        QJsonObject subObj = subArray[si].toObject();
                        auto& sub = subs[si];
                        if (!sub) continue;
                        if (subObj.contains(QStringLiteral("visibility")))
                            sub->setAlpha(static_cast<float>(subObj.value(QStringLiteral("visibility")).toInt()) * 0.01f);
                        if (subObj.contains(QStringLiteral("grid")) && !divisionMerged)
                            sub->setGridMode(static_cast<uint8_t>(subObj.value(QStringLiteral("grid")).toInt()));
                        if (subObj.contains(QStringLiteral("stereo")) && !divisionMerged)
                            sub->setStereoMode(static_cast<uint8_t>(subObj.value(QStringLiteral("stereo")).toInt()));
                        if (subObj.contains(QStringLiteral("plane")) && !divisionMerged) {
                            QJsonObject po = subObj.value(QStringLiteral("plane")).toObject();
                            if (po.contains(QStringLiteral("circular"))) sub->setPlaneCircular(po.value(QStringLiteral("circular")).toBool());
                            if (po.contains(QStringLiteral("centreDegrees"))) sub->setPlaneCentreDegrees(po.value(QStringLiteral("centreDegrees")).toDouble());
                            if (po.contains(QStringLiteral("widthDegrees"))) sub->setPlaneWidthDegrees(po.value(QStringLiteral("widthDegrees")).toDouble());
                            if (po.contains(QStringLiteral("aspectRatio")))
                                sub->setPlaneAspectRatio(static_cast<uint8_t>(po.value(QStringLiteral("aspectRatio")).toInt()));
                            if (po.contains(QStringLiteral("width")))
                                sub->setPlaneWidth(po.value(QStringLiteral("width")).toDouble());
                            if (po.contains(QStringLiteral("height")))
                                sub->setPlaneHeight(po.value(QStringLiteral("height")).toDouble());
                            if (po.contains(QStringLiteral("elevation")))
                                sub->setPlaneElevation(po.value(QStringLiteral("elevation")).toDouble());
                            if (po.contains(QStringLiteral("azimuth")))
                                sub->setPlaneAzimuth(po.value(QStringLiteral("azimuth")).toDouble());
                            if (po.contains(QStringLiteral("roll")))
                                sub->setPlaneRoll(po.value(QStringLiteral("roll")).toDouble());
                            if (po.contains(QStringLiteral("distance")))
                                sub->setPlaneDistance(po.value(QStringLiteral("distance")).toDouble());
                            if (po.contains(QStringLiteral("horizontal")))
                                sub->setPlaneHorizontal(po.value(QStringLiteral("horizontal")).toDouble());
                            if (po.contains(QStringLiteral("vertical")))
                                sub->setPlaneVertical(po.value(QStringLiteral("vertical")).toDouble());
                        }
                        if (subObj.contains(QStringLiteral("rotate"))) {
                            QJsonObject ro = subObj.value(QStringLiteral("rotate")).toObject();
                            glm::vec3 rot(0.f);
                            if (ro.contains(QStringLiteral("pitch")))
                                rot.x = static_cast<float>(ro.value(QStringLiteral("pitch")).toDouble());
                            if (ro.contains(QStringLiteral("yaw")))
                                rot.y = static_cast<float>(ro.value(QStringLiteral("yaw")).toDouble());
                            if (ro.contains(QStringLiteral("roll")))
                                rot.z = static_cast<float>(ro.value(QStringLiteral("roll")).toDouble());
                            sub->setRotate(rot);
                        }
                    }
                }
            }

            if ((type == BaseLayer::VIDEO || type == BaseLayer::AUDIO || type == BaseLayer::STREAM || type == BaseLayer::YOUTUBE)) {
                if (o.contains(QStringLiteral("mpvOptions"))) {
                    std::string mpvOptions = o.value(QStringLiteral("mpvOptions")).toString().toStdString();
                    MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layers[idx].first.get());
                    mpvLayer->setMpvOptionsName(mpvOptions);
                }
            }

            if ((type == BaseLayer::VIDEO || type == BaseLayer::AUDIO || type == BaseLayer::YOUTUBE)) {
                BaseLayer* baseLayer = m_layers[idx].first.get();
                if (o.contains(QStringLiteral("end_of_file"))) {
                    int eofMode = 2; // Loop by default
                    QString eofModeText = o.value(QStringLiteral("end_of_file")).toString();
                    if (eofModeText == QStringLiteral("pause")) {
                        eofMode = 0;
                    }
                    else if (eofModeText == QStringLiteral("loop")) {
                        eofMode = 2;
                    }
                    baseLayer->setEOFMode(eofMode);
                }
                if (o.contains(QStringLiteral("section_enabled"))) {
                    bool loopEnabled = o.value(QStringLiteral("section_enabled")).toBool();
                    double loopA = o.contains(QStringLiteral("time_start")) ? o.value(QStringLiteral("time_start")).toDouble() : 0.0;
                    double loopB = o.contains(QStringLiteral("time_end")) ? o.value(QStringLiteral("time_end")).toDouble() : 0.0;
                    baseLayer->setLoopTime(loopA, loopB, loopEnabled);
                }
            }

            if (o.contains(QStringLiteral("keepVisibilityForNumSlides"))) {
                int keepVisibilityForNumSlides = o.value(QStringLiteral("keepVisibilityForNumSlides")).toInt();
                m_layers[idx].first->setKeepVisibilityForNumSlides(keepVisibilityForNumSlides);
            }

            if (o.contains(QStringLiteral("flipY"))) {
                bool flipY = o.value(QStringLiteral("flipY")).toBool();
                m_layers[idx].first->setFlipY(flipY);
            }

            // Only saved when true, so absent means off (overrides the addLayer default).
            m_layers[idx].first->setUseNoSignalImage(o.value(QStringLiteral("noSignalImage")).toBool());
            // Absent means on: layers follow the audio mapping and the spatial mapping unless opted out.
            m_layers[idx].first->setAudioMappingEnabled(o.value(QStringLiteral("audioMapping")).toBool(true));
            m_layers[idx].first->setSpatialAudioEnabled(o.value(QStringLiteral("spatialAudio")).toBool(true));

            if (o.contains(QStringLiteral("ndiOutput"))) {
                m_layers[idx].first->setNdiOutputEnabled(o.value(QStringLiteral("ndiOutput")).toBool());
                if (o.contains(QStringLiteral("ndiSenderName"))) {
                    m_layers[idx].first->setNdiSenderName(o.value(QStringLiteral("ndiSenderName")).toString().toStdString());
                }
            }

            if (o.contains(QStringLiteral("existOnMasterOnly"))) {
                m_layers[idx].first->setExistOnMasterOnly(o.value(QStringLiteral("existOnMasterOnly")).toBool());
            }

            // Absent in older files: the layer keeps the slide default (set in addLayer).
            if (o.contains(QStringLiteral("hierarchy"))) {
                const QString h = o.value(QStringLiteral("hierarchy")).toString();
                if (h == QStringLiteral("front"))
                    m_layers[idx].first->setHierarchy(BaseLayer::LayerHierarchy::FRONT);
                else if (h == QStringLiteral("back"))
                    m_layers[idx].first->setHierarchy(BaseLayer::LayerHierarchy::BACK);
            }

            if (o.contains(QStringLiteral("nodeStreamFormat")))
                m_layers[idx].first->setNodeStreamFormat(o.value(QStringLiteral("nodeStreamFormat")).toInt());
            if (o.contains(QStringLiteral("nodeStreamSync")))
                m_layers[idx].first->setNodeStreamSyncMode(o.value(QStringLiteral("nodeStreamSync")).toInt());
            if (o.contains(QStringLiteral("nodeStreamGroup")))
                m_layers[idx].first->setNodeStreamGroup(o.value(QStringLiteral("nodeStreamGroup")).toString().trimmed().toStdString());
            if (o.contains(QStringLiteral("nodeStreamPort")))
                m_layers[idx].first->setNodeStreamPort(o.value(QStringLiteral("nodeStreamPort")).toInt());
            if (o.contains(QStringLiteral("nodeStreamMaxFps")))
                m_layers[idx].first->setNodeStreamMaxFps(o.value(QStringLiteral("nodeStreamMaxFps")).toInt());
            if (o.contains(QStringLiteral("nodeStreamUseNdi")))
                m_layers[idx].first->setNodeStreamUseNdi(o.value(QStringLiteral("nodeStreamUseNdi")).toBool() ? 1 : 0);
            if (o.contains(QStringLiteral("nodeStreamOutput")))
                m_layers[idx].first->setNodeStreamOutputEnabled(o.value(QStringLiteral("nodeStreamOutput")).toBool());

#ifdef WEBRTC_LAYER
            // The default is on; only an explicit opt-out is stored in the file.
            if (o.contains(QStringLiteral("masterRelay")) && m_layers[idx].first->type() == BaseLayer::WEBRTC) {
                static_cast<WebRTCLayer*>(m_layers[idx].first.get())->setMasterRelayEnabled(o.value(QStringLiteral("masterRelay")).toBool());
            }
            if (o.contains(QStringLiteral("syncDelayMs")) && m_layers[idx].first->type() == BaseLayer::WEBRTC) {
                static_cast<WebRTCLayer*>(m_layers[idx].first.get())->setSyncDelayMs(o.value(QStringLiteral("syncDelayMs")).toInt(kWebRtcDefaultSyncDelayMs));
            }
            if (o.contains(QStringLiteral("whepAuthUsername")) && m_layers[idx].first->type() == BaseLayer::WEBRTC) {
                WebRTCLayer* webRtcLayer = static_cast<WebRTCLayer*>(m_layers[idx].first.get());
                webRtcLayer->setAuthUsername(o.value(QStringLiteral("whepAuthUsername")).toString().toStdString());
                webRtcLayer->setAuthPassword(o.value(QStringLiteral("whepAuthPassword")).toString().toStdString());
            }
#endif

            if (o.contains(QStringLiteral("eye_mode"))) {
                QString em = o.value(QStringLiteral("eye_mode")).toString();
                if (em == QStringLiteral("left"))
                    m_layers[idx].first->setEyeMode(static_cast<uint8_t>(BaseLayer::EyeMode::Left));
                else if (em == QStringLiteral("right"))
                    m_layers[idx].first->setEyeMode(static_cast<uint8_t>(BaseLayer::EyeMode::Right));
                else
                    m_layers[idx].first->setEyeMode(static_cast<uint8_t>(BaseLayer::EyeMode::Both));
            }

            if (grid == BaseLayer::GridMode::Plane && o.contains(QStringLiteral("plane"))) {
                QJsonValue planeValues = o.value(QStringLiteral("plane"));
                QJsonArray planeArray = planeValues.toArray();
                for (auto pa : planeArray) {
                    QJsonObject po = pa.toObject();
                    if (po.contains(QStringLiteral("circular"))) m_layers[idx].first->setPlaneCircular(po.value(QStringLiteral("circular")).toBool());
                    if (po.contains(QStringLiteral("centreDegrees"))) m_layers[idx].first->setPlaneCentreDegrees(po.value(QStringLiteral("centreDegrees")).toDouble());
                    if (po.contains(QStringLiteral("widthDegrees"))) m_layers[idx].first->setPlaneWidthDegrees(po.value(QStringLiteral("widthDegrees")).toDouble());
                    if (po.contains(QStringLiteral("aspectRatio"))) {
                        int planeAR = po.value(QStringLiteral("aspectRatio")).toInt();
                        m_layers[idx].first->setPlaneAspectRatio(static_cast<uint8_t>(planeAR));
                    }
                    if (po.contains(QStringLiteral("width"))) {
                        double planeW = po.value(QStringLiteral("width")).toDouble();
                        m_layers[idx].first->setPlaneWidth(planeW);
                    }
                    if (po.contains(QStringLiteral("height"))) {
                        double planeH = po.value(QStringLiteral("height")).toDouble();
                        m_layers[idx].first->setPlaneHeight(planeH);
                    }
                    if (po.contains(QStringLiteral("elevation"))) {
                        double planeE = po.value(QStringLiteral("elevation")).toDouble();
                        m_layers[idx].first->setPlaneElevation(planeE);
                    }
                    if (po.contains(QStringLiteral("azimuth"))) {
                        double planeA = po.value(QStringLiteral("azimuth")).toDouble();
                        m_layers[idx].first->setPlaneAzimuth(planeA);
                    }
                    if (po.contains(QStringLiteral("roll"))) {
                        double planeR = po.value(QStringLiteral("roll")).toDouble();
                        m_layers[idx].first->setPlaneRoll(planeR);
                    }
                    if (po.contains(QStringLiteral("distance"))) {
                        double planeD = po.value(QStringLiteral("distance")).toDouble();
                        m_layers[idx].first->setPlaneDistance(planeD);
                    }
                    if (po.contains(QStringLiteral("horizontal"))) {
                        double planeHM = po.value(QStringLiteral("horizontal")).toDouble();
                        m_layers[idx].first->setPlaneHorizontal(planeHM);
                    }
                    if (po.contains(QStringLiteral("vertical"))) {
                        double planeVM = po.value(QStringLiteral("vertical")).toDouble();
                        m_layers[idx].first->setPlaneVertical(planeVM);
                    }
                }
            }
            else if (grid == BaseLayer::GridMode::Dome && o.contains(QStringLiteral("dome"))) {
                QJsonValue domeValues = o.value(QStringLiteral("dome"));
                QJsonArray domeArray = domeValues.toArray();
                for (auto pa : domeArray) {
                    QJsonObject po = pa.toObject();
                    glm::vec3 domeRotation = glm::vec3(0.f);
                    if (po.contains(QStringLiteral("yaw"))) {
                        double domeYaw = po.value(QStringLiteral("yaw")).toDouble();
                        domeRotation.y = static_cast<float>(domeYaw);
                        m_layers[idx].first->setRotate(domeRotation);
                    }
                }
            }
            else if ((grid == BaseLayer::GridMode::Sphere_EQR || grid == BaseLayer::GridMode::Sphere_EAC) 
                && o.contains(QStringLiteral("sphere"))) {
                QJsonValue sphereValues = o.value(QStringLiteral("sphere"));
                QJsonArray sphereArray = sphereValues.toArray();
                for (auto pa : sphereArray) {
                    QJsonObject po = pa.toObject();
                    glm::vec3 sphereRotation = glm::vec3(0.f);
                    bool hasRotation = false;
                    if (po.contains(QStringLiteral("pitch"))) {
                        double spherePitch = po.value(QStringLiteral("pitch")).toDouble();
                        sphereRotation.x = static_cast<float>(spherePitch);
                        hasRotation = true;
                    }
                    if (po.contains(QStringLiteral("yaw"))) {
                        double sphereYaw = po.value(QStringLiteral("yaw")).toDouble();
                        sphereRotation.y = static_cast<float>(sphereYaw);
                        hasRotation = true;
                    }
                    if (po.contains(QStringLiteral("roll"))) {
                        double sphereRoll = po.value(QStringLiteral("roll")).toDouble();
                        sphereRotation.z = static_cast<float>(sphereRoll);
                        hasRotation = true;
                    }
                    if (hasRotation) {
                        m_layers[idx].first->setRotate(sphereRotation);
                    }
                }
            }

            if (o.contains(QStringLiteral("roi"))) {
                QJsonValue roiValues = o.value(QStringLiteral("roi"));
                QJsonArray roiArray = roiValues.toArray();
                for (auto ra : roiArray) {
                    QJsonObject pa = ra.toObject();
                    if (pa.contains(QStringLiteral("enabled"))) {
                        bool roiEnabled = pa.value(QStringLiteral("enabled")).toBool();
                        m_layers[idx].first->setRoiEnabled(roiEnabled);
                    }
                    glm::vec4 roi = glm::vec4(0.f, 0.f, 1.f, 1.f);
                    if (pa.contains(QStringLiteral("xpos"))) {
                        roi.x = static_cast<float>(pa.value(QStringLiteral("xpos")).toDouble());
                    }
                    if (pa.contains(QStringLiteral("ypos"))) {
                        roi.y = static_cast<float>(pa.value(QStringLiteral("ypos")).toDouble());
                    }
                    if (pa.contains(QStringLiteral("width"))) {
                        roi.z = static_cast<float>(pa.value(QStringLiteral("width")).toDouble());
                    }
                    if (pa.contains(QStringLiteral("height"))) {
                        roi.w = static_cast<float>(pa.value(QStringLiteral("height")).toDouble());
                    }
                    m_layers[idx].first->setRoi(roi);
                }
            }

            if (o.contains(QStringLiteral("postEffects"))) {
                PostEffects fx;
                applyPostEffectsMap(fx, o.value(QStringLiteral("postEffects")).toObject().toVariantMap());
                m_layers[idx].first->setPostEffects(fx);
            }

            // Per-layer timeline keyframes and effects
            if (o.contains(QStringLiteral("timelineKeyframes")) || o.contains(QStringLiteral("animationEffects"))) {
                if (LayerTimeline *track = timelineAt(row))
                    decodeTrack(o, *track);
            }

            return row;
        }
    }
    return -1;
}

void LayersModel::decodeFromJSON(QJsonObject &obj, const QStringList &forRelativePaths) {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    const QScopedValueRollback<bool> decoding(m_decodingLayers, true);
    if (obj.contains(QStringLiteral("name"))) {
        QString name = obj.value(QStringLiteral("name")).toString();
        setLayersName(name);
    }

    if (obj.contains(QStringLiteral("visibility"))) {
        int vis = obj.value(QStringLiteral("visibility")).toInt();
        setLayersVisibility(vis);
    }

    if (obj.contains(QStringLiteral("lockableLayers"))) {
        bool lockable = obj.value(QStringLiteral("lockableLayers")).toBool();
        setLayersCanBeLocked(lockable);
    }

    if (obj.contains(QStringLiteral("hasTimeline"))) {
        m_hasTimeline = obj.value(QStringLiteral("hasTimeline")).toBool();
    }
    if (obj.contains(QStringLiteral("timelineDuration"))) {
        m_timelineDuration = obj.value(QStringLiteral("timelineDuration")).toInt();
    }
    if (obj.contains(QStringLiteral("timelineOutroStart"))) {
        m_timelineOutroStart = obj.value(QStringLiteral("timelineOutroStart")).toInt();
    }

    if (obj.contains(QStringLiteral("layers"))) {
        clearLayers();
        int numLockedLayersLeft = m_layers.size();
        QJsonValue value = obj.value(QStringLiteral("layers"));
        QJsonArray array = value.toArray();
        for (auto v : array) {
            decodeLayerFromJSON(v.toObject(), forRelativePaths);
        }

        // Remove locked layers that have an identical (same type and title) locked layer
        // This handles the case where a locked layer persists from a previous presentation
        // but the newly loaded presentation contains the same layer.
        if (getLayersCanBeLocked() && numLockedLayersLeft > 0) {
            for (int i = m_layers.size() - 1; i >= numLockedLayersLeft; --i) {
                if (!m_layers[i].first->isLocked())
                    continue;
                int lockedType = static_cast<int>(m_layers[i].first->type());
                std::string lockedTitle = m_layers[i].first->title();
                for (int j = 0; j < numLockedLayersLeft; ++j) {
                    if (j == i || !m_layers[j].first->isLocked())
                        continue;
                    if (static_cast<int>(m_layers[j].first->type()) == lockedType
                        && m_layers[j].first->title() == lockedTitle) {
                        m_layers[j].first->setEnabled(false);
                        m_layerTimelines.erase(m_layers[j].first.get());
                        const int row = rowOfLayer(m_layers[j].first.get());
                        beginRemoveRows(QModelIndex(), row, row);
                        m_order.removeAt(row);
                        m_layers.removeAt(j);
                        endRemoveRows();
                        numLockedLayersLeft--;
                        break;
                    }
                }
            }
        }
    }

    // Resolved once SlidesModel has handed over the references (see setReferencedLayers).
    m_pendingRenderOrder = obj.value(QStringLiteral("renderOrder")).toArray();
    m_pendingReferenceTracks = obj.value(QStringLiteral("referenceAnimations")).toArray();
    applyPendingRenderOrder();
    applyPendingReferenceTracks();

    rebuildEffectSchedule();
    setNeedSync();
}

QJsonObject LayersModel::encodeLayerToJSON(const std::shared_ptr<BaseLayer> &layer, const QStringList &forRelativePaths) {
    QJsonObject layerData;

    layerData.insert(QStringLiteral("type"), QJsonValue(QString::fromStdString(layer->typeName())));

    layerData.insert(QStringLiteral("title"), QJsonValue(QString::fromStdString(layer->title())));

    if (getLayersCanBeLocked() && layer->isLocked()) {
        layerData.insert(QStringLiteral("locked"), QJsonValue(layer->isLocked()));
    }

    if (layer->type() == BaseLayer::IMAGE
        || layer->type() == BaseLayer::VIDEO
#ifdef  PDF_SUPPORT
        || layer->type() == BaseLayer::PDF
#endif //  PDF_SUPPORT
        || layer->type() == BaseLayer::AUDIO) {
        QString checkedFilePath = makePathRelativeTo(QString::fromStdString(layer->filepath()), forRelativePaths);
        layerData.insert(QStringLiteral("path"), QJsonValue(checkedFilePath));
    }
    else {
        layerData.insert(QStringLiteral("path"), QJsonValue(QString::fromStdString(layer->filepath())));
    }
    if (!layer->targetNode().empty())
        layerData.insert(QStringLiteral("captureNode"), QJsonValue(QString::fromStdString(layer->targetNode())));

#ifdef DIRECTSHOW_SUPPORT
    if (layer->type() == BaseLayer::DIRECTSHOW) {
        // Store the predefined setup key so each machine can re-resolve its own local capture devices on load.
        const DirectShowLayer* directShowLayer = static_cast<const DirectShowLayer*>(layer.get());
        if (!directShowLayer->presetKey().empty()) {
            layerData.insert(QStringLiteral("directShowPreset"), QJsonValue(QString::fromStdString(directShowLayer->presetKey())));
        }
    }
#endif
#ifdef CAPTURE_SUPPORT
    if (layer->type() == BaseLayer::CAPTURE) {
        const CaptureLayer* captureLayer = static_cast<const CaptureLayer*>(layer.get());
        if (!captureLayer->presetKey().empty()) {
            layerData.insert(QStringLiteral("capturePreset"), QJsonValue(QString::fromStdString(captureLayer->presetKey())));
        }
        layerData.insert(QStringLiteral("volume"), QJsonValue(layer->volume()));
    }
#endif

#ifdef TEXT_LAYER
    if (layer->type() == BaseLayer::TEXT) {
        std::shared_ptr<TextLayer> textLayer = std::static_pointer_cast<TextLayer>(layer);
        layerData.insert(QStringLiteral("text"), QJsonValue(QString::fromStdString(textLayer->text())));
        layerData.insert(QStringLiteral("font"), QJsonValue(QString::fromStdString(textLayer->fontName())));
        layerData.insert(QStringLiteral("size"), QJsonValue(textLayer->fontSize()));
        layerData.insert(QStringLiteral("width"), QJsonValue(textLayer->width()));
        layerData.insert(QStringLiteral("height"), QJsonValue(textLayer->height()));
        layerData.insert(QStringLiteral("alignment"), QJsonValue(QString::fromStdString(textLayer->alignmentStr())));
        layerData.insert(QStringLiteral("color"), QJsonValue(QString::fromStdString(textLayer->colorHex())));
    }
#endif

#ifdef PDF_SUPPORT
    if (layer->type() == BaseLayer::PDF) {
        std::shared_ptr<PdfLayer> pdfLayer = std::static_pointer_cast<PdfLayer>(layer);
        layerData.insert(QStringLiteral("page"), QJsonValue(pdfLayer->page()));
        layerData.insert(QStringLiteral("numPages"), QJsonValue(pdfLayer->numPages()));
    }
#endif
#ifdef NDI_SUPPORT
    if (layer->type() == BaseLayer::NDI) {
        layerData.insert(QStringLiteral("volume"), QJsonValue(layer->volume()));
    }
#endif
#ifdef OMT_SUPPORT
    if (layer->type() == BaseLayer::OMT) {
        layerData.insert(QStringLiteral("volume"), QJsonValue(layer->volume()));
    }
#endif
#ifdef WEBRTC_LAYER
    if (layer->type() == BaseLayer::WEBRTC) {
        layerData.insert(QStringLiteral("volume"), QJsonValue(layer->volume()));
    }
#endif
    if (layer->supportsTextureSubdivision()) {
        if (layer->isQRCodeDetectionEnabled()) {
            layerData.insert(QStringLiteral("qrCodeDetection"), QJsonValue(true));
        }
        if (layer->textureDivisionMode() != 0) {
            layerData.insert(QStringLiteral("textureDivisionMode"), QJsonValue(layer->textureDivisionMode()));
        }
        if (layer->textureDivisionGrid() != 0) {
            layerData.insert(QStringLiteral("textureDivisionGrid"), QJsonValue(layer->textureDivisionGrid()));
        }
        if (layer->textureDivisionMode() == 2) {
            layerData.insert(QStringLiteral("textureDivisionMergeColumns"), QJsonValue(layer->textureDivisionMergeColumns()));
            layerData.insert(QStringLiteral("textureDivisionMergeRows"), QJsonValue(layer->textureDivisionMergeRows()));
        }
        // Not hasSubLayers(): that is false while a placeholder or no-signal image shows.
        if (layer->textureDivisionMode() == 2 && !layer->getSubLayers().empty()) {
            QJsonArray subLayersArray;
            auto& subs = layer->getSubLayers();
            for (int si = 0; si < static_cast<int>(subs.size()); ++si) {
                auto& sub = subs[si];
                if (!sub) continue;
                QJsonObject subData;
                subData.insert(QStringLiteral("title"), QJsonValue(QString::fromStdString(sub->title())));
                subData.insert(QStringLiteral("visibility"), QJsonValue(static_cast<int>(sub->alpha() * 100.f)));
                subData.insert(QStringLiteral("grid"), QJsonValue(static_cast<int>(sub->gridMode())));
                subData.insert(QStringLiteral("stereo"), QJsonValue(static_cast<int>(sub->stereoMode())));
                if (sub->gridMode() == BaseLayer::GridMode::Plane) {
                    QJsonObject planeObj;
                    planeObj.insert(QStringLiteral("circular"), QJsonValue(sub->planeCircular()));
                    planeObj.insert(QStringLiteral("centreDegrees"), QJsonValue(sub->planeCentreDegrees()));
                    planeObj.insert(QStringLiteral("widthDegrees"), QJsonValue(sub->planeWidthDegrees()));
                    planeObj.insert(QStringLiteral("aspectRatio"), QJsonValue(sub->planeAspectRatio()));
                    planeObj.insert(QStringLiteral("width"), QJsonValue(sub->planeWidth()));
                    planeObj.insert(QStringLiteral("height"), QJsonValue(sub->planeHeight()));
                    planeObj.insert(QStringLiteral("elevation"), QJsonValue(sub->planeElevation()));
                    planeObj.insert(QStringLiteral("azimuth"), QJsonValue(sub->planeAzimuth()));
                    planeObj.insert(QStringLiteral("roll"), QJsonValue(sub->planeRoll()));
                    planeObj.insert(QStringLiteral("distance"), QJsonValue(sub->planeDistance()));
                    planeObj.insert(QStringLiteral("horizontal"), QJsonValue(sub->planeHorizontal()));
                    planeObj.insert(QStringLiteral("vertical"), QJsonValue(sub->planeVertical()));
                    subData.insert(QStringLiteral("plane"), planeObj);
                } else {
                    QJsonObject rotObj;
                    rotObj.insert(QStringLiteral("pitch"), QJsonValue(static_cast<double>(sub->rotate().x)));
                    rotObj.insert(QStringLiteral("yaw"), QJsonValue(static_cast<double>(sub->rotate().y)));
                    rotObj.insert(QStringLiteral("roll"), QJsonValue(static_cast<double>(sub->rotate().z)));
                    subData.insert(QStringLiteral("rotate"), rotObj);
                }
                subLayersArray.push_back(subData);
            }
            layerData.insert(QStringLiteral("divisionSubLayers"), subLayersArray);
        }
    }
    if (layer->type() == BaseLayer::CONTROL) {
        ControlLayer* controlLayer = static_cast<ControlLayer*>(layer.get());
        layerData.insert(QStringLiteral("operation"), QJsonValue(QString::fromStdString(controlLayer->operation())));
        layerData.insert(QStringLiteral("parameter"), QJsonValue(QString::fromStdString(controlLayer->parameter())));
    }
    if (layer->type() == BaseLayer::REST) {
        RestLayer* restLayer = static_cast<RestLayer*>(layer.get());
        layerData.insert(QStringLiteral("url"), QJsonValue(QString::fromStdString(restLayer->url())));
        layerData.insert(QStringLiteral("method"), QJsonValue(restLayer->method()));
        layerData.insert(QStringLiteral("parameters"), QJsonValue(QString::fromStdString(restLayer->parameters())));
        if (restLayer->ignoreStatus())
            layerData.insert(QStringLiteral("ignoreStatus"), QJsonValue(true));
    }
    if (layer->type() == BaseLayer::STREAM) {
        const std::string streamKey = static_cast<StreamLayer*>(layer.get())->streamKey();
        if (!streamKey.empty())
            layerData.insert(QStringLiteral("streamKey"), QJsonValue(QString::fromStdString(streamKey)));
    }
    if (layer->type() == BaseLayer::TCP) {
        auto *tcp = static_cast<TcpLayer *>(layer.get());
        layerData.insert(QStringLiteral("tcpCommandId"), QString::fromStdString(tcp->commandId()));
    }
    if (layer->type() == BaseLayer::VIDEO || layer->type() == BaseLayer::AUDIO || layer->type() == BaseLayer::STREAM || layer->type() == BaseLayer::YOUTUBE) {
        MpvLayer* mpvLayer = static_cast<MpvLayer*>(layer.get());
        if (!mpvLayer->mpvOptionsName().empty()) {
            layerData.insert(QStringLiteral("mpvOptions"), QJsonValue(QString::fromStdString(mpvLayer->mpvOptionsName())));
        }
    }
    if (layer->type() == BaseLayer::VIDEO || layer->type() == BaseLayer::AUDIO || layer->type() == BaseLayer::YOUTUBE) {
        if (layer->hasAudio()) {
            layerData.insert(QStringLiteral("volume"), QJsonValue(layer->volume()));
            layerData.insert(QStringLiteral("audioId"), QJsonValue(layer->audioId()));
        }
        if (layer->eofMode() >= 0) {
            int eofMode = layer->eofMode();
            QString eofModeText;
            switch (eofMode) {
            case 0:
                eofModeText = QStringLiteral("pause");
                break;
            case 2:
                eofModeText = QStringLiteral("loop");
                break;
            default:
                eofModeText = QStringLiteral("loop");
                break;
            }
            layerData.insert(QStringLiteral("end_of_file"), QJsonValue(eofModeText));
        }
        if (layer->loopTimeEnabled()) {
            layerData.insert(QStringLiteral("section_enabled"), QJsonValue(layer->loopTimeEnabled()));
            layerData.insert(QStringLiteral("time_start"), QJsonValue(layer->loopTimeA()));
            layerData.insert(QStringLiteral("time_end"), QJsonValue(layer->loopTimeB()));
        }
    }
    if (layer->type() == BaseLayer::STREAM) {
        if (layer->hasAudio()) {
            layerData.insert(QStringLiteral("volume"), QJsonValue(layer->volume()));
            layerData.insert(QStringLiteral("audioId"), QJsonValue(layer->audioId()));
        }
    }

    QString grid;
    int gridIdx = layer->gridMode();
    if (gridIdx == BaseLayer::GridMode::Plane) {
        grid = QStringLiteral("plane");
    } else if (gridIdx == BaseLayer::GridMode::Dome) {
        grid = QStringLiteral("dome");
    } else if (gridIdx == BaseLayer::GridMode::Sphere_EQR) {
        grid = QStringLiteral("sphere-eqr");
    } else if (gridIdx == BaseLayer::GridMode::Sphere_EAC) {
        grid = QStringLiteral("sphere-eac");
    } else {
        grid = QStringLiteral("pre-split");
    }
    layerData.insert(QStringLiteral("grid"), QJsonValue(grid));

    QString sv;
    int stereoVideoIdx = layer->stereoMode();
    if (stereoVideoIdx == BaseLayer::StereoMode::SBS_3D) {
        sv = QStringLiteral("side-by-side");
    } else if (stereoVideoIdx == BaseLayer::StereoMode::TB_3D) {
        sv = QStringLiteral("top-bottom");
    } else if (stereoVideoIdx == BaseLayer::StereoMode::TBF_3D) {
        sv = QStringLiteral("top-bottom-flip");
    } else {
        sv = QStringLiteral("no");
    }
    layerData.insert(QStringLiteral("stereoscopic"), QJsonValue(sv));

    layerData.insert(QStringLiteral("visibility"), QJsonValue(static_cast<int>(layer->alpha() * 100.f)));
    layerData.insert(QStringLiteral("keepVisibilityForNumSlides"), QJsonValue(layer->keepVisibilityForNumSlides()));

    switch (layer->type()) {
#ifdef NDI_LAYER
    case BaseLayer::NDI:
#endif
#ifdef OMT_LAYER
    case BaseLayer::OMT:
#endif
#ifdef STREAM_LAYER
    case BaseLayer::STREAM:
#endif
#ifdef SPOUT_LAYER
    case BaseLayer::SPOUT:
#endif
#ifdef WEBRTC_LAYER
    case BaseLayer::WEBRTC:
#endif
        // User-toggled in the LayerView; store both states since some types default to flipped.
        layerData.insert(QStringLiteral("flipY"), QJsonValue(layer->flipY()));
        break;
#ifdef DIRECTSHOW_SUPPORT
    case BaseLayer::DIRECTSHOW:
        if (layer->flipY())
            layerData.insert(QStringLiteral("flipY"), QJsonValue(true));
        break;
#endif
    default:
        break;
    }

    if (layer->ndiOutputEnabled()) {
        layerData.insert(QStringLiteral("ndiOutput"), QJsonValue(true));
        layerData.insert(QStringLiteral("ndiSenderName"), QJsonValue(QString::fromStdString(layer->ndiSenderName())));
    }

    if (layer->existOnMasterOnly()) {
        layerData.insert(QStringLiteral("existOnMasterOnly"), QJsonValue(true));
    }

    if (layer->hierarchy() != m_layerHierachy) {
        layerData.insert(QStringLiteral("hierarchy"),
                         QJsonValue(layer->hierarchy() == BaseLayer::LayerHierarchy::FRONT ? QStringLiteral("front") : QStringLiteral("back")));
    }

    if (layer->useNoSignalImage())
        layerData.insert(QStringLiteral("noSignalImage"), QJsonValue(true));

    // Only saved when the layer opted out of the audio mapping / spatial mapping (on by default).
    if (!layer->audioMappingEnabled())
        layerData.insert(QStringLiteral("audioMapping"), QJsonValue(false));
    if (!layer->spatialAudioEnabled())
        layerData.insert(QStringLiteral("spatialAudio"), QJsonValue(false));

    if (layer->nodeStreamOutputEnabled())
        layerData.insert(QStringLiteral("nodeStreamOutput"), QJsonValue(true));
    if (layer->nodeStreamUseNdi() >= 0)
        layerData.insert(QStringLiteral("nodeStreamUseNdi"), QJsonValue(layer->nodeStreamUseNdi() == 1));
    if (layer->nodeStreamFormat() != 0)
        layerData.insert(QStringLiteral("nodeStreamFormat"), QJsonValue(layer->nodeStreamFormat()));
    if (layer->nodeStreamSyncMode() != 0)
        layerData.insert(QStringLiteral("nodeStreamSync"), QJsonValue(layer->nodeStreamSyncMode()));
    if (!layer->nodeStreamGroup().empty())
        layerData.insert(QStringLiteral("nodeStreamGroup"), QJsonValue(QString::fromStdString(layer->nodeStreamGroup())));
    if (layer->nodeStreamPort() != 0)
        layerData.insert(QStringLiteral("nodeStreamPort"), QJsonValue(layer->nodeStreamPort()));
    if (layer->nodeStreamMaxFps() != 0)
        layerData.insert(QStringLiteral("nodeStreamMaxFps"), QJsonValue(layer->nodeStreamMaxFps()));

#ifdef WEBRTC_LAYER
    // The default is on; only persist the opt-out.
    if (layer->type() == BaseLayer::WEBRTC && !static_cast<WebRTCLayer*>(layer.get())->masterRelayEnabled()) {
        layerData.insert(QStringLiteral("masterRelay"), QJsonValue(false));
    }
    if (layer->type() == BaseLayer::WEBRTC
        && static_cast<WebRTCLayer*>(layer.get())->syncDelayMs() != kWebRtcDefaultSyncDelayMs) {
        layerData.insert(QStringLiteral("syncDelayMs"), QJsonValue(static_cast<WebRTCLayer*>(layer.get())->syncDelayMs()));
    }
    if (layer->type() == BaseLayer::WEBRTC) {
        const WebRTCLayer* webRtcLayer = static_cast<WebRTCLayer*>(layer.get());
        if (!webRtcLayer->authUsername().empty()) {
            layerData.insert(QStringLiteral("whepAuthUsername"), QJsonValue(QString::fromStdString(webRtcLayer->authUsername())));
            layerData.insert(QStringLiteral("whepAuthPassword"), QJsonValue(QString::fromStdString(webRtcLayer->authPassword())));
        }
    }
#endif

    if (layer->eyeMode() != static_cast<uint8_t>(BaseLayer::EyeMode::Both)) {
        QString em;
        if (layer->eyeMode() == static_cast<uint8_t>(BaseLayer::EyeMode::Left))
            em = QStringLiteral("left");
        else
            em = QStringLiteral("right");
        layerData.insert(QStringLiteral("eye_mode"), QJsonValue(em));
    }

    // Plane properties
    if (gridIdx == BaseLayer::GridMode::Plane) {
        QJsonArray planeArray;
        QJsonObject planeData;
        planeData.insert(QStringLiteral("circular"), QJsonValue(layer->planeCircular()));
        planeData.insert(QStringLiteral("centreDegrees"), QJsonValue(layer->planeCentreDegrees()));
        planeData.insert(QStringLiteral("widthDegrees"), QJsonValue(layer->planeWidthDegrees()));
        planeData.insert(QStringLiteral("aspectRatio"), QJsonValue(layer->planeAspectRatio()));
        planeData.insert(QStringLiteral("width"), QJsonValue(layer->planeWidth()));
        planeData.insert(QStringLiteral("height"), QJsonValue(layer->planeHeight()));
        planeData.insert(QStringLiteral("elevation"), QJsonValue(layer->planeElevation()));
        planeData.insert(QStringLiteral("azimuth"), QJsonValue(layer->planeAzimuth()));
        planeData.insert(QStringLiteral("roll"), QJsonValue(layer->planeRoll()));
        planeData.insert(QStringLiteral("distance"), QJsonValue(layer->planeDistance()));
        planeData.insert(QStringLiteral("horizontal"), QJsonValue(layer->planeHorizontal()));
        planeData.insert(QStringLiteral("vertical"), QJsonValue(layer->planeVertical()));
        planeArray.push_back(QJsonValue(planeData));
        layerData.insert(QString(QStringLiteral("plane")), QJsonValue(planeArray));
    }
    else if (gridIdx == BaseLayer::GridMode::Dome) {
        QJsonArray domeArray;
        QJsonObject domeData;
        domeData.insert(QStringLiteral("yaw"), QJsonValue(layer->rotate().y));
        domeArray.push_back(QJsonValue(domeData));
        layerData.insert(QString(QStringLiteral("dome")), QJsonValue(domeArray));
    }
    else if (gridIdx == BaseLayer::GridMode::Sphere_EQR || gridIdx == BaseLayer::GridMode::Sphere_EAC) {
        QJsonArray sphereArray;
        QJsonObject sphereData;
        sphereData.insert(QStringLiteral("pitch"), QJsonValue(layer->rotate().x));
        sphereData.insert(QStringLiteral("yaw"), QJsonValue(layer->rotate().y));
        sphereData.insert(QStringLiteral("roll"), QJsonValue(layer->rotate().z));
        sphereArray.push_back(QJsonValue(sphereData));
        layerData.insert(QString(QStringLiteral("sphere")), QJsonValue(sphereArray));
    }

    // ROI details
    QJsonArray roiArray;
    QJsonObject roiData;
    roiData.insert(QStringLiteral("enabled"), QJsonValue(layer->roiEnabled()));
    roiData.insert(QStringLiteral("xpos"), QJsonValue(layer->roi().x));
    roiData.insert(QStringLiteral("ypos"), QJsonValue(layer->roi().y));
    roiData.insert(QStringLiteral("width"), QJsonValue(layer->roi().z));
    roiData.insert(QStringLiteral("height"), QJsonValue(layer->roi().w));
    roiArray.push_back(QJsonValue(roiData));
    layerData.insert(QString(QStringLiteral("roi")), QJsonValue(roiArray));

    // Post effects, kept when only disabled so their settings survive
    const QVariantMap postEffects = postEffectsToMap(layer->postEffects());
    if (postEffects != postEffectsToMap(PostEffects()))
        layerData.insert(QStringLiteral("postEffects"), QJsonObject::fromVariantMap(postEffects));

    // Per-layer timeline keyframes and effects
    if (const LayerTimeline *track = timelineAt(rowOfLayer(layer.get()))) {
        const QJsonObject trackObj = encodeTrack(*track);
        for (auto it = trackObj.begin(); it != trackObj.end(); ++it)
            layerData.insert(it.key(), it.value());
    }

    return layerData;
}

void LayersModel::encodeToJSON(QJsonObject &obj, const QStringList &forRelativePaths) {
    obj.insert(QStringLiteral("name"), QJsonValue(getLayersName()));
    obj.insert(QStringLiteral("visibility"), QJsonValue(getLayersVisibility()));

    if (getLayersCanBeLocked()) {
        obj.insert(QStringLiteral("lockableLayers"), QJsonValue(getLayersCanBeLocked()));
    }

    // Timeline metadata
    if (m_hasTimeline) {
        obj.insert(QStringLiteral("hasTimeline"), QJsonValue(true));
        obj.insert(QStringLiteral("timelineDuration"), QJsonValue(m_timelineDuration));
        if (m_timelineOutroStart > 0)
            obj.insert(QStringLiteral("timelineOutroStart"), QJsonValue(m_timelineOutroStart));
    }

    QJsonArray layersArray;
    for (int li = 0; li < m_layers.size(); ++li) {
        auto layer = m_layers[li].first;
        if (!layer)
            continue;
        layersArray.push_back(QJsonValue(encodeLayerToJSON(layer, forRelativePaths)));
    }

    obj.insert(QString(QStringLiteral("layers")), QJsonValue(layersArray));

    // Render order and animations of layers referenced from earlier slides. Own layers are
    // written as "L<index in layers>", references as "R<slides back>:<index in that slide>".
    if (!m_refs.isEmpty()) {
        QJsonArray orderArray;
        for (const BaseLayer *l : m_order) {
            const int own = ownIndexOf(l);
            if (own >= 0) {
                orderArray.push_back(QStringLiteral("L%1").arg(own));
                continue;
            }
            const int ref = referenceIndexOf(l);
            if (ref >= 0)
                orderArray.push_back(QStringLiteral("R%1:%2").arg(m_refs[ref].slideOffset).arg(m_refs[ref].ownerLayerIdx));
        }
        obj.insert(QStringLiteral("renderOrder"), orderArray);

        QJsonArray refTracks;
        for (const LayerReference &r : m_refs) {
            auto it = m_layerTimelines.find(r.layer.get());
            if (it == m_layerTimelines.end())
                continue;
            QJsonObject trackObj = encodeTrack(it->second);
            if (trackObj.isEmpty())
                continue;
            trackObj.insert(QStringLiteral("slideOffset"), r.slideOffset);
            trackObj.insert(QStringLiteral("layerIndex"), r.ownerLayerIdx);
            refTracks.push_back(trackObj);
        }
        if (!refTracks.isEmpty())
            obj.insert(QStringLiteral("referenceAnimations"), refTracks);
    }

    setLayersNeedsSave(false);
}

QJsonObject LayersModel::encodeTrack(const LayerTimeline &track) const {
    QJsonObject obj;
    if (m_hasTimeline && !track.keyframes.isEmpty()) {
        QJsonArray kfArray;
        for (const LayerKeyframe &kf : track.keyframes) {
            QJsonObject kfObj;
            kfObj.insert(QStringLiteral("timeMs"), QJsonValue(kf.timeMs));
            kfObj.insert(QStringLiteral("alpha"),  QJsonValue(static_cast<double>(kf.alpha)));
            if (kf.easing != 0)
                kfObj.insert(QStringLiteral("easing"), QJsonValue(kf.easing));
            if (kf.hasRotate) {
                kfObj.insert(QStringLiteral("hasRotate"), QJsonValue(true));
                kfObj.insert(QStringLiteral("rotateX"),   QJsonValue(static_cast<double>(kf.rotateX)));
                kfObj.insert(QStringLiteral("rotateY"),   QJsonValue(static_cast<double>(kf.rotateY)));
                kfObj.insert(QStringLiteral("rotateZ"),   QJsonValue(static_cast<double>(kf.rotateZ)));
            }
            if (kf.hasTranslate) {
                kfObj.insert(QStringLiteral("hasTranslate"), QJsonValue(true));
                kfObj.insert(QStringLiteral("translateX"),   QJsonValue(static_cast<double>(kf.translateX)));
                kfObj.insert(QStringLiteral("translateY"),   QJsonValue(static_cast<double>(kf.translateY)));
                kfObj.insert(QStringLiteral("translateZ"),   QJsonValue(static_cast<double>(kf.translateZ)));
            }
            if (kf.hasScale) {
                kfObj.insert(QStringLiteral("hasScale"), QJsonValue(true));
                kfObj.insert(QStringLiteral("scale"),    QJsonValue(static_cast<double>(kf.scale)));
            }
            if (kf.hasPlane) {
                kfObj.insert(QStringLiteral("hasPlane"),        QJsonValue(true));
                kfObj.insert(QStringLiteral("planeAzimuth"),    QJsonValue(static_cast<double>(kf.planeAzimuth)));
                kfObj.insert(QStringLiteral("planeElevation"),  QJsonValue(static_cast<double>(kf.planeElevation)));
                kfObj.insert(QStringLiteral("planeRoll"),       QJsonValue(static_cast<double>(kf.planeRoll)));
                kfObj.insert(QStringLiteral("planeDistance"),   QJsonValue(static_cast<double>(kf.planeDistance)));
                kfObj.insert(QStringLiteral("planeHorizontal"), QJsonValue(static_cast<double>(kf.planeHorizontal)));
                kfObj.insert(QStringLiteral("planeVertical"),   QJsonValue(static_cast<double>(kf.planeVertical)));
            }
            kfArray.push_back(kfObj);
        }
        obj.insert(QStringLiteral("timelineKeyframes"), kfArray);
    }
    if (!track.effects.isEmpty()) {
        QJsonArray efArray;
        for (const auto &e : track.effects) {
            QJsonObject eo;
            eo.insert(QStringLiteral("order"), e.order);
            eo.insert(QStringLiteral("phase"), e.phase);
            eo.insert(QStringLiteral("preset"), e.preset);
            eo.insert(QStringLiteral("trigger"), e.trigger);
            eo.insert(QStringLiteral("delayMs"), e.delayMs);
            eo.insert(QStringLiteral("durationMs"), e.durationMs);
            eo.insert(QStringLiteral("easing"), e.easing);
            eo.insert(QStringLiteral("direction"), e.direction);
            eo.insert(QStringLiteral("feather"), static_cast<double>(e.feather));
            eo.insert(QStringLiteral("amount"), static_cast<double>(e.amount));
            if (cplay::anim::effectPresetIsMove(e.preset)) {
                QJsonObject t;
                t.insert(QStringLiteral("azimuth"), static_cast<double>(e.target.azimuth));
                t.insert(QStringLiteral("elevation"), static_cast<double>(e.target.elevation));
                t.insert(QStringLiteral("roll"), static_cast<double>(e.target.roll));
                t.insert(QStringLiteral("distance"), static_cast<double>(e.target.distance));
                t.insert(QStringLiteral("horizontal"), static_cast<double>(e.target.horizontal));
                t.insert(QStringLiteral("vertical"), static_cast<double>(e.target.vertical));
                eo.insert(QStringLiteral("planeTarget"), t);
                if (e.hasStart) {
                    QJsonObject st;
                    st.insert(QStringLiteral("azimuth"), static_cast<double>(e.start.azimuth));
                    st.insert(QStringLiteral("elevation"), static_cast<double>(e.start.elevation));
                    st.insert(QStringLiteral("roll"), static_cast<double>(e.start.roll));
                    st.insert(QStringLiteral("distance"), static_cast<double>(e.start.distance));
                    st.insert(QStringLiteral("horizontal"), static_cast<double>(e.start.horizontal));
                    st.insert(QStringLiteral("vertical"), static_cast<double>(e.start.vertical));
                    eo.insert(QStringLiteral("planeStart"), st);
                }
            }
            efArray.push_back(eo);
        }
        obj.insert(QStringLiteral("animationEffects"), efArray);
    }
    return obj;
}

void LayersModel::decodeTrack(const QJsonObject &o, LayerTimeline &track) const {
    track.keyframes.clear();
    track.effects.clear();
    const QJsonArray kfArray = o.value(QStringLiteral("timelineKeyframes")).toArray();
    for (const auto &kv : kfArray) {
        QJsonObject ko = kv.toObject();
        LayerKeyframe kf;
        kf.timeMs          = ko.value(QStringLiteral("timeMs")).toInt();
        kf.alpha           = static_cast<float>(ko.value(QStringLiteral("alpha")).toDouble());
        kf.easing          = ko.value(QStringLiteral("easing")).toInt(0);
        kf.hasRotate       = ko.value(QStringLiteral("hasRotate")).toBool(false);
        kf.rotateX         = static_cast<float>(ko.value(QStringLiteral("rotateX")).toDouble(0.0));
        kf.rotateY         = static_cast<float>(ko.value(QStringLiteral("rotateY")).toDouble(0.0));
        kf.rotateZ         = static_cast<float>(ko.value(QStringLiteral("rotateZ")).toDouble(0.0));
        kf.hasTranslate    = ko.value(QStringLiteral("hasTranslate")).toBool(false);
        kf.translateX      = static_cast<float>(ko.value(QStringLiteral("translateX")).toDouble(0.0));
        kf.translateY      = static_cast<float>(ko.value(QStringLiteral("translateY")).toDouble(0.0));
        kf.translateZ      = static_cast<float>(ko.value(QStringLiteral("translateZ")).toDouble(0.0));
        kf.hasScale        = ko.value(QStringLiteral("hasScale")).toBool(false);
        kf.scale           = static_cast<float>(ko.value(QStringLiteral("scale")).toDouble(1.0));
        kf.hasPlane        = ko.value(QStringLiteral("hasPlane")).toBool(false);
        kf.planeAzimuth    = static_cast<float>(ko.value(QStringLiteral("planeAzimuth")).toDouble(0.0));
        kf.planeElevation  = static_cast<float>(ko.value(QStringLiteral("planeElevation")).toDouble(0.0));
        kf.planeRoll       = static_cast<float>(ko.value(QStringLiteral("planeRoll")).toDouble(0.0));
        kf.planeDistance   = static_cast<float>(ko.value(QStringLiteral("planeDistance")).toDouble(0.0));
        kf.planeHorizontal = static_cast<float>(ko.value(QStringLiteral("planeHorizontal")).toDouble(0.0));
        kf.planeVertical   = static_cast<float>(ko.value(QStringLiteral("planeVertical")).toDouble(0.0));
        track.keyframes.append(kf);
    }
    const QJsonArray efArray = o.value(QStringLiteral("animationEffects")).toArray();
    for (const auto &ev : efArray) {
        QJsonObject eo = ev.toObject();
        cplay::anim::LayerEffect e;
        e.order      = eo.value(QStringLiteral("order")).toInt(0);
        e.phase      = std::clamp(eo.value(QStringLiteral("phase")).toInt(0), 0, 1);
        e.preset     = std::clamp(eo.value(QStringLiteral("preset")).toInt(1), 0, cplay::anim::effectPresetCount() - 1);
        e.trigger    = std::clamp(eo.value(QStringLiteral("trigger")).toInt(1), 0, 2);
        e.delayMs    = std::max(0, eo.value(QStringLiteral("delayMs")).toInt(0));
        e.durationMs = std::max(1, eo.value(QStringLiteral("durationMs")).toInt(500));
        e.easing     = eo.value(QStringLiteral("easing")).toInt(0);
        e.direction  = std::clamp(eo.value(QStringLiteral("direction")).toInt(0), 0, cplay::anim::effectDirectionCount() - 1);
        e.feather    = static_cast<float>(eo.value(QStringLiteral("feather")).toDouble(0.1));
        e.amount     = static_cast<float>(eo.value(QStringLiteral("amount")).toDouble(1.0));
        const QJsonObject t = eo.value(QStringLiteral("planeTarget")).toObject();
        e.target.azimuth    = static_cast<float>(t.value(QStringLiteral("azimuth")).toDouble(0.0));
        e.target.elevation  = static_cast<float>(t.value(QStringLiteral("elevation")).toDouble(0.0));
        e.target.roll       = static_cast<float>(t.value(QStringLiteral("roll")).toDouble(0.0));
        e.target.distance   = static_cast<float>(t.value(QStringLiteral("distance")).toDouble(0.0));
        e.target.horizontal = static_cast<float>(t.value(QStringLiteral("horizontal")).toDouble(0.0));
        e.target.vertical   = static_cast<float>(t.value(QStringLiteral("vertical")).toDouble(0.0));
        const QJsonObject st = eo.value(QStringLiteral("planeStart")).toObject();
        e.hasStart = !st.isEmpty();
        e.start.azimuth    = static_cast<float>(st.value(QStringLiteral("azimuth")).toDouble(0.0));
        e.start.elevation  = static_cast<float>(st.value(QStringLiteral("elevation")).toDouble(0.0));
        e.start.roll       = static_cast<float>(st.value(QStringLiteral("roll")).toDouble(0.0));
        e.start.distance   = static_cast<float>(st.value(QStringLiteral("distance")).toDouble(0.0));
        e.start.horizontal = static_cast<float>(st.value(QStringLiteral("horizontal")).toDouble(0.0));
        e.start.vertical   = static_cast<float>(st.value(QStringLiteral("vertical")).toDouble(0.0));
        track.effects.append(e);
    }
}

bool LayersModel::runRenderOnLayersThatShouldUpdate(bool updateRendering, bool preload) {
    bool statusHasUpdated = false;
    // The GUI thread adds/removes layers here (drag-and-drop), which used to realloc
    // m_layers underneath this loop: the render thread kept a reference into the freed
    // QList and crashed. Same for m_draggedLayersAwaitingGridGuess.
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    for (int i = 0; i < m_layers.size(); i++) {
        auto& layer = m_layers[i].first;
        if (layer && layer->isEnabled()) {
            if (layer->shouldUpdate()
                || (preload && !layer->ready()) 
                || (layer->shouldPreLoad() && !layer->ready())
                || m_draggedLayersAwaitingGridGuess.contains(layer.get())) {
                if (!layer->hasInitialized()) {
                    layer->initialize();
                }
                layer->update(layer->shouldUpdate() && updateRendering);
            }
            // A play signal that arrived before this layer finished loading (a slow
            // stream, yt-dlp resolution, etc.) is fulfilled here: update() above has
            // driven the load forward, and this is the per-frame place that already
            // polls ready(), so the deferred start fires as soon as the load lands.
            layer->fulfillPendingStart();
            // The spatial audio mapping follows where the layer is shown (moved, animated).
            layer->updateAudioSpatialSource();
            if (m_draggedLayersAwaitingGridGuess.contains(layer.get())
                && layer->ready()
                && layer->width() > 0
                && layer->height() > 0) {
                guessGridModeForDraggedLayer(i, layer.get());
            }
            if (layer->ready() && layer->shouldUpdateFrame()) {
                // Mostly here to handle update on layers in other windows
                layer->updateFrame();
            }
            // Publish the layer as an own NDI source when requested. We are on
            // the render thread with a current OpenGL context here, which
            // updateNdiOutput() requires. It is also a no-op, and releases any
            // previously allocated resources, when the mode is off.
            layer->updateNdiOutput();
            // Same for streaming the texture to the nodes.
            layer->updateNodeStreamOutput();
            if (m_layers.size() > i && m_layers[i].first->type() != BaseLayer::REST) {
                int currentStatus = m_layers[i].second;
                if (layer && layer->type() == BaseLayer::TCP) {
                    m_layers[i].second = static_cast<TcpLayer *>(layer.get())->triggerStatus();
                }
                else if (layer && layer->ready() && layer->alpha() > 0.f) {
                    m_layers[i].second = 2;
                }
                else if (layer && layer->ready()) {
                    m_layers[i].second = 1;
                }
                else {
                    m_layers[i].second = 0;
                }
                if (currentStatus != m_layers[i].second) {
                    updateLayerRow(layer.get());
                    statusHasUpdated = true;
                }
            }
        }
    }
    return statusHasUpdated;
}

void LayersModel::guessGridModeForDraggedLayer(int layerIdx, BaseLayer *layer) {
    m_draggedLayersAwaitingGridGuess.remove(layer);

    if (!PresentationSettings::guessGridModeOnDragAndDrop())
        return;

    uint8_t guessedGridMode = layer->gridMode();
    if (layer->width() == layer->height()) {
        guessedGridMode = BaseLayer::Dome;
    }
    else if (layer->width() == 2 * layer->height()) {
        guessedGridMode = BaseLayer::Sphere_EQR;
    }

    Q_UNUSED(layerIdx)
    if (guessedGridMode != layer->gridMode()) {
        layer->setGridMode(guessedGridMode);
        updateLayerRow(layer);
        setLayersNeedsSave(true);
        setNeedSync();
    }
}

LayersTypeModel::LayersTypeModel(QObject *parent)
    : QAbstractListModel(parent) {
    for (int i = 1; i != (int)BaseLayer::LayerType::INVALID; i++) {
#ifdef NODE_STREAM_SUPPORT
        // Node-only receiver, never created by the user.
        if (i == (int)BaseLayer::LayerType::NODESTREAM)
            continue;
#endif
        m_layerTypes.append(QString::fromStdString(BaseLayer::typeDescription((BaseLayer::LayerType)i)));
    }
}

int LayersTypeModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_layerTypes.size();
}

QVariant LayersTypeModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_layerTypes.empty())
        return QVariant();

    if (!checkIndex(index)) {
        return QVariant();
    }
    if (role == textRole) {
        return m_layerTypes.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> LayersTypeModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[textRole] = "typeName";
    return roles;
}

void LayersModel::setNeedSync() {
    m_needSync = true;
    m_syncIteration = PresentationSettings::networkSyncIterations();
}

void LayersModel::markAllLayersNeedSync() {
    std::lock_guard<std::recursive_mutex> layersLock(m_layersMutex);
    for (auto& pair : m_layers) {
        if (pair.first)
            pair.first->markNodeStreamLayerForResync();
    }
    setNeedSync();
}

// ---- Timeline implementation ------------------------------------------------

bool LayersModel::getHasTimeline() const {
    return m_hasTimeline;
}

void LayersModel::setHasTimeline(bool value) {
    if (m_hasTimeline == value)
        return;
    if (!value) {
        stopAllTimelines();
        // Back to a regular slide: only the effects the user added keep playing.
        removeDefaultFadeEffects();
        clearAllAnimStates();
    }
    m_hasTimeline = value;
    if (m_hasTimeline) {
        ensureTimelineSizeMatchesLayers();
        int fadeMs = PresentationSettings::fadeDurationToNextSlide();
        // Default duration to 2x the fade duration if still at the compile-time default
        if (m_timelineDuration == 5000 && fadeMs > 0)
            m_timelineDuration = 2 * fadeMs;
        // Default outro start to fadeDurationToNextSlide if not already set
        if (m_timelineOutroStart <= 0 && fadeMs > 0 && fadeMs < m_timelineDuration)
            m_timelineOutroStart = m_timelineDuration - fadeMs;
        // Default fade-in / fade-out for layers without any animation
        int outroMs = (m_timelineOutroStart > 0) ? m_timelineOutroStart : m_timelineDuration;
        bool anyGenerated = false;
        for (int l = 0; l < m_order.size(); ++l) {
            // Referenced layers are already visible when the slide starts; they only get
            // a track when the user animates them.
            if (ownIndexOfRow(l) < 0)
                continue;
            LayerTimeline *track = timelineAt(l);
            if (track && track->keyframes.isEmpty() && track->effects.isEmpty()) {
                anyGenerated = true;
                if (layerSupportsAnimation(l)) {
                    addDefaultFadeEffects(*track, true, hasOutro());
                    continue;
                }
                LayerKeyframe kfStart;
                kfStart.timeMs = 0;
                kfStart.alpha  = 0.f;
                LayerKeyframe kfFull;
                kfFull.timeMs = outroMs;
                kfFull.alpha  = 1.f;
                track->keyframes.append(kfStart);
                track->keyframes.append(kfFull);
                if (m_timelineOutroStart > 0) {
                    LayerKeyframe kfEnd;
                    kfEnd.timeMs = m_timelineDuration;
                    kfEnd.alpha  = 0.f;
                    track->keyframes.append(kfEnd);
                }
            }
        }
        if (anyGenerated)
            Q_EMIT layersModelChanged();
    }
    rebuildEffectSchedule();
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

int LayersModel::getTimelineDuration() const {
    return m_timelineDuration;
}

void LayersModel::setTimelineDuration(int ms) {
    if (m_timelineDuration == ms)
        return;
    m_timelineDuration = std::max(ms, 100);
    // Clamp outro start if it now exceeds duration
    if (m_timelineOutroStart >= m_timelineDuration)
        m_timelineOutroStart = -1;
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

int LayersModel::getTimelineOutroStart() const {
    return m_timelineOutroStart;
}

void LayersModel::setTimelineOutroStart(int ms) {
    int newVal = (ms <= 0 || ms >= m_timelineDuration) ? -1 : ms;
    if (m_timelineOutroStart == newVal)
        return;
    m_timelineOutroStart = newVal;
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

bool LayersModel::hasOutro() const {
    return m_timelineOutroStart > 0 && m_timelineOutroStart < m_timelineDuration;
}

int LayersModel::introDuration() const {
    if (hasOutro())
        return m_timelineOutroStart;
    return m_timelineDuration;
}

int LayersModel::outroDuration() const {
    if (hasOutro())
        return m_timelineDuration - m_timelineOutroStart;
    return 0;
}

void LayersModel::ensureTimelineSizeMatchesLayers() {
    // Drop timelines of layers that no longer exist (their address could be reused).
    // Tracks of referenced layers survive while the layer lives, even if the reference
    // is gone for now (the owner's visibility span may be extended again).
    for (auto it = m_referenceTrackLayers.begin(); it != m_referenceTrackLayers.end();)
        it = it->second.expired() ? m_referenceTrackLayers.erase(it) : std::next(it);
    for (auto it = m_layerTimelines.begin(); it != m_layerTimelines.end();) {
        const bool found = ownIndexOf(it->first) >= 0
            || m_referenceTrackLayers.find(it->first) != m_referenceTrackLayers.end();
        it = found ? std::next(it) : m_layerTimelines.erase(it);
    }
}

void LayersModel::addDefaultFadeEffects(LayerTimeline &track, bool intro, bool outro) const {
    using namespace cplay::anim;
    for (int phase = 0; phase < 2; ++phase) {
        if (!(phase == 0 ? intro : outro))
            continue;
        LayerEffect e;
        e.phase = phase;
        e.preset = static_cast<int>(EffectPreset::Fade);
        e.trigger = static_cast<int>(EffectTrigger::WithPrevious);
        e.easing = static_cast<int>(phase == 0 ? Easing::OutCubic : Easing::InCubic);
        e.durationMs = std::max(1, phase == 0 ? introDuration() : outroDuration());
        int maxOrder = -1;
        for (const auto &kv : m_layerTimelines) {
            for (const auto &other : kv.second.effects) {
                if (other.phase == phase)
                    maxOrder = std::max(maxOrder, other.order);
            }
        }
        for (const auto &other : track.effects) {
            if (other.phase == phase)
                maxOrder = std::max(maxOrder, other.order);
        }
        e.order = maxOrder + 1;
        track.effects.append(e);
    }
}

bool LayersModel::isDefaultIntroFade(const cplay::anim::LayerEffect &e) const {
    using namespace cplay::anim;
    return e.phase == static_cast<int>(EffectPhase::Intro) && e.preset == static_cast<int>(EffectPreset::Fade)
        && e.trigger == static_cast<int>(EffectTrigger::WithPrevious) && e.delayMs == 0
        && e.easing == static_cast<int>(Easing::OutCubic) && e.durationMs == std::max(1, introDuration());
}

void LayersModel::removeDefaultFadeEffects() {
    for (auto &kv : m_layerTimelines) {
        kv.second.effects.removeIf([this](const cplay::anim::LayerEffect &e) {
            return isDefaultIntroFade(e) || isDefaultOutroFade(e);
        });
    }
}

bool LayersModel::isDefaultOutroFade(const cplay::anim::LayerEffect &e) const {
    using namespace cplay::anim;
    return e.phase == static_cast<int>(EffectPhase::Outro) && e.preset == static_cast<int>(EffectPreset::Fade)
        && e.trigger == static_cast<int>(EffectTrigger::WithPrevious) && e.delayMs == 0
        && e.easing == static_cast<int>(Easing::InCubic) && e.durationMs == std::max(1, outroDuration());
}

void LayersModel::addDefaultFadeForNewLayer(int layerIdx) {
    if (!m_hasTimeline || m_decodingLayers || !layerSupportsAnimation(layerIdx))
        return;
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track || !track->keyframes.isEmpty() || !track->effects.isEmpty())
        return;
    addDefaultFadeEffects(*track, true, hasOutro());
    rebuildEffectSchedule();
    Q_EMIT timelineChanged();
}

LayerTimeline *LayersModel::timelineAt(int layerIdx) {
    const BaseLayer *layer = rowLayer(layerIdx);
    if (!layer)
        return nullptr;
    return &m_layerTimelines[layer];
}

const LayerTimeline *LayersModel::timelineAt(int layerIdx) const {
    const BaseLayer *layer = rowLayer(layerIdx);
    if (!layer)
        return nullptr;
    auto it = m_layerTimelines.find(layer);
    return it == m_layerTimelines.end() ? nullptr : &it->second;
}

namespace {

QVariantMap keyframeToMap(const LayerKeyframe &kf) {
    QVariantMap m;
    m[QStringLiteral("timeMs")]          = kf.timeMs;
    m[QStringLiteral("alpha")]           = static_cast<double>(kf.alpha);
    m[QStringLiteral("easing")]          = kf.easing;
    m[QStringLiteral("hasRotate")]       = kf.hasRotate;
    m[QStringLiteral("rotateX")]         = static_cast<double>(kf.rotateX);
    m[QStringLiteral("rotateY")]         = static_cast<double>(kf.rotateY);
    m[QStringLiteral("rotateZ")]         = static_cast<double>(kf.rotateZ);
    m[QStringLiteral("hasTranslate")]    = kf.hasTranslate;
    m[QStringLiteral("translateX")]      = static_cast<double>(kf.translateX);
    m[QStringLiteral("translateY")]      = static_cast<double>(kf.translateY);
    m[QStringLiteral("translateZ")]      = static_cast<double>(kf.translateZ);
    m[QStringLiteral("hasScale")]        = kf.hasScale;
    m[QStringLiteral("scale")]           = static_cast<double>(kf.scale);
    m[QStringLiteral("hasPlane")]        = kf.hasPlane;
    m[QStringLiteral("planeAzimuth")]    = static_cast<double>(kf.planeAzimuth);
    m[QStringLiteral("planeElevation")]  = static_cast<double>(kf.planeElevation);
    m[QStringLiteral("planeRoll")]       = static_cast<double>(kf.planeRoll);
    m[QStringLiteral("planeDistance")]   = static_cast<double>(kf.planeDistance);
    m[QStringLiteral("planeHorizontal")] = static_cast<double>(kf.planeHorizontal);
    m[QStringLiteral("planeVertical")]   = static_cast<double>(kf.planeVertical);
    return m;
}

// Only keys present in the map are changed.
void applyMapToKeyframe(const QVariantMap &m, LayerKeyframe &kf) {
    auto f = [&m](const char *key, float &out) {
        const QString k = QString::fromLatin1(key);
        if (m.contains(k))
            out = static_cast<float>(m.value(k).toDouble());
    };
    auto b = [&m](const char *key, bool &out) {
        const QString k = QString::fromLatin1(key);
        if (m.contains(k))
            out = m.value(k).toBool();
    };
    if (m.contains(QStringLiteral("timeMs")))
        kf.timeMs = m.value(QStringLiteral("timeMs")).toInt();
    if (m.contains(QStringLiteral("easing")))
        kf.easing = m.value(QStringLiteral("easing")).toInt();
    f("alpha", kf.alpha);
    b("hasRotate", kf.hasRotate);
    f("rotateX", kf.rotateX);
    f("rotateY", kf.rotateY);
    f("rotateZ", kf.rotateZ);
    b("hasTranslate", kf.hasTranslate);
    f("translateX", kf.translateX);
    f("translateY", kf.translateY);
    f("translateZ", kf.translateZ);
    b("hasScale", kf.hasScale);
    f("scale", kf.scale);
    b("hasPlane", kf.hasPlane);
    f("planeAzimuth", kf.planeAzimuth);
    f("planeElevation", kf.planeElevation);
    f("planeRoll", kf.planeRoll);
    f("planeDistance", kf.planeDistance);
    f("planeHorizontal", kf.planeHorizontal);
    f("planeVertical", kf.planeVertical);
    kf.alpha = std::clamp(kf.alpha, 0.f, 1.f);
}

void sortKeyframes(QVector<LayerKeyframe> &kfs) {
    std::stable_sort(kfs.begin(), kfs.end(), [](const LayerKeyframe &a, const LayerKeyframe &b) {
        return a.timeMs < b.timeMs;
    });
}

// Interpolates one keyframe group; easing of the segment's first keyframe shapes the segment.
template <size_t N, typename Has, typename Get>
bool evaluateGroup(const QVector<LayerKeyframe> &kfs, int timeMs, Has has, Get get, std::array<float, N> &out) {
    const LayerKeyframe *prev = nullptr;
    const LayerKeyframe *next = nullptr;
    for (const LayerKeyframe &kf : kfs) {
        if (!has(kf))
            continue;
        if (kf.timeMs <= timeMs)
            prev = &kf;
        else {
            next = &kf;
            break;
        }
    }
    if (!prev && !next)
        return false;
    if (!prev) {
        out = get(*next);
        return true;
    }
    if (!next || next->timeMs <= prev->timeMs) {
        out = get(*prev);
        return true;
    }
    const float t = static_cast<float>(timeMs - prev->timeMs) / static_cast<float>(next->timeMs - prev->timeMs);
    const float e = cplay::anim::ease(prev->easing, t);
    const std::array<float, N> a = get(*prev);
    const std::array<float, N> c = get(*next);
    for (size_t i = 0; i < N; ++i)
        out[i] = a[i] + e * (c[i] - a[i]);
    return true;
}

QVariantMap effectToMap(const cplay::anim::LayerEffect &e) {
    QVariantMap m;
    m[QStringLiteral("order")]      = e.order;
    m[QStringLiteral("phase")]      = e.phase;
    m[QStringLiteral("preset")]     = e.preset;
    m[QStringLiteral("presetName")] = QString::fromLatin1(cplay::anim::effectPresetName(e.preset));
    m[QStringLiteral("trigger")]    = e.trigger;
    m[QStringLiteral("delayMs")]    = e.delayMs;
    m[QStringLiteral("durationMs")] = e.durationMs;
    m[QStringLiteral("easing")]     = e.easing;
    m[QStringLiteral("direction")]  = e.direction;
    m[QStringLiteral("feather")]    = static_cast<double>(e.feather);
    m[QStringLiteral("amount")]     = static_cast<double>(e.amount);
    m[QStringLiteral("targetAzimuth")]    = static_cast<double>(e.target.azimuth);
    m[QStringLiteral("targetElevation")]  = static_cast<double>(e.target.elevation);
    m[QStringLiteral("targetRoll")]       = static_cast<double>(e.target.roll);
    m[QStringLiteral("targetDistance")]   = static_cast<double>(e.target.distance);
    m[QStringLiteral("targetHorizontal")] = static_cast<double>(e.target.horizontal);
    m[QStringLiteral("targetVertical")]   = static_cast<double>(e.target.vertical);
    m[QStringLiteral("hasStart")]         = e.hasStart;
    m[QStringLiteral("startAzimuth")]     = static_cast<double>(e.start.azimuth);
    m[QStringLiteral("startElevation")]   = static_cast<double>(e.start.elevation);
    m[QStringLiteral("startRoll")]        = static_cast<double>(e.start.roll);
    m[QStringLiteral("startDistance")]    = static_cast<double>(e.start.distance);
    m[QStringLiteral("startHorizontal")]  = static_cast<double>(e.start.horizontal);
    m[QStringLiteral("startVertical")]    = static_cast<double>(e.start.vertical);
    return m;
}

QVariantMap poseToMap(const cplay::anim::PlanePose &p) {
    QVariantMap m;
    m[QStringLiteral("azimuth")]    = static_cast<double>(p.azimuth);
    m[QStringLiteral("elevation")]  = static_cast<double>(p.elevation);
    m[QStringLiteral("roll")]       = static_cast<double>(p.roll);
    m[QStringLiteral("distance")]   = static_cast<double>(p.distance);
    m[QStringLiteral("horizontal")] = static_cast<double>(p.horizontal);
    m[QStringLiteral("vertical")]   = static_cast<double>(p.vertical);
    return m;
}

// Entrance/exit effects that shape visibility; Move effects leave it alone.
bool hasVisibilityEffects(const LayerTimeline &track, cplay::anim::EffectPhase phase) {
    for (const auto &e : track.effects) {
        if (e.phase == static_cast<int>(phase) && !cplay::anim::effectPresetIsMove(e.preset))
            return true;
    }
    return false;
}

}

QVariantList LayersModel::getKeyframes(int layerIdx) const {
    QVariantList result;
    const LayerTimeline *track = timelineAt(layerIdx);
    if (!track)
        return result;
    const BaseLayer *layer = rowLayer(layerIdx);
    for (const LayerKeyframe &kf : track->keyframes) {
        QVariantMap value = keyframeToMap(kf);
        value[QStringLiteral("planeCircular")] = layer && layer->planeCircular();
        result.append(value);
    }
    return result;
}

void LayersModel::setKeyframes(int layerIdx, const QVariantList &keyframes) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track)
        return;
    track->keyframes.clear();
    for (const QVariant &v : keyframes) {
        LayerKeyframe kf;
        applyMapToKeyframe(v.toMap(), kf);
        track->keyframes.append(kf);
    }
    sortKeyframes(track->keyframes);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::addKeyframe(int layerIdx, int timeMs, float alpha) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track)
        return;
    LayerKeyframe kf;
    kf.timeMs = std::max(0, std::min(timeMs, m_timelineDuration));
    kf.alpha  = std::max(0.f, std::min(alpha, 1.f));
    auto &kfs = track->keyframes;
    int insertPos = 0;
    while (insertPos < kfs.size() && kfs[insertPos].timeMs <= kf.timeMs)
        ++insertPos;
    kfs.insert(insertPos, kf);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::addKeyframe(int layerIdx, int timeMs, float alpha,
                              bool hasRotate,
                              float rotateX, float rotateY, float rotateZ,
                              bool hasTranslate,
                              float translateX, float translateY, float translateZ) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track)
        return;
    LayerKeyframe kf;
    kf.timeMs       = std::max(0, std::min(timeMs, m_timelineDuration));
    kf.alpha        = std::max(0.f, std::min(alpha, 1.f));
    kf.hasRotate    = hasRotate;
    kf.rotateX      = rotateX; kf.rotateY = rotateY; kf.rotateZ = rotateZ;
    kf.hasTranslate = hasTranslate;
    kf.translateX   = translateX; kf.translateY = translateY; kf.translateZ = translateZ;
    auto &kfs = track->keyframes;
    int insertPos = 0;
    while (insertPos < kfs.size() && kfs[insertPos].timeMs <= kf.timeMs)
        ++insertPos;
    kfs.insert(insertPos, kf);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::removeKeyframe(int layerIdx, int keyframeIdx) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track)
        return;
    auto &kfs = track->keyframes;
    if (keyframeIdx < 0 || keyframeIdx >= kfs.size())
        return;
    kfs.removeAt(keyframeIdx);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::updateKeyframe(int layerIdx, int keyframeIdx, int timeMs, float alpha) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track)
        return;
    auto &kfs = track->keyframes;
    if (keyframeIdx < 0 || keyframeIdx >= kfs.size())
        return;
    kfs[keyframeIdx].timeMs = std::max(0, std::min(timeMs, m_timelineDuration));
    kfs[keyframeIdx].alpha  = std::max(0.f, std::min(alpha, 1.f));
    sortKeyframes(kfs);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::updateKeyframe(int layerIdx, int keyframeIdx, int timeMs, float alpha,
                                 bool hasRotate,
                                 float rotateX, float rotateY, float rotateZ,
                                 bool hasTranslate,
                                 float translateX, float translateY, float translateZ) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track)
        return;
    auto &kfs = track->keyframes;
    if (keyframeIdx < 0 || keyframeIdx >= kfs.size())
        return;
    kfs[keyframeIdx].timeMs       = std::max(0, std::min(timeMs, m_timelineDuration));
    kfs[keyframeIdx].alpha        = std::max(0.f, std::min(alpha, 1.f));
    kfs[keyframeIdx].hasRotate    = hasRotate;
    kfs[keyframeIdx].rotateX      = rotateX; kfs[keyframeIdx].rotateY = rotateY; kfs[keyframeIdx].rotateZ = rotateZ;
    kfs[keyframeIdx].hasTranslate = hasTranslate;
    kfs[keyframeIdx].translateX   = translateX; kfs[keyframeIdx].translateY = translateY; kfs[keyframeIdx].translateZ = translateZ;
    sortKeyframes(kfs);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::setKeyframeEasing(int layerIdx, int keyframeIdx, int easing) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track || keyframeIdx < 0 || keyframeIdx >= track->keyframes.size())
        return;
    track->keyframes[keyframeIdx].easing = std::clamp(easing, 0, cplay::anim::easingCount() - 1);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::updateKeyframeExtras(int layerIdx, int keyframeIdx, const QVariantMap &values) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track || keyframeIdx < 0 || keyframeIdx >= track->keyframes.size())
        return;
    LayerKeyframe &kf = track->keyframes[keyframeIdx];
    applyMapToKeyframe(values, kf);
    kf.timeMs = std::clamp(kf.timeMs, 0, m_timelineDuration);
    // A freshly enabled group starts from the layer's current values.
    BaseLayer *layer = rowLayer(layerIdx);
    if (values.value(QStringLiteral("hasPlane")).toBool() && !values.contains(QStringLiteral("planeAzimuth"))) {
        kf.planeAzimuth = static_cast<float>(layer->planeAzimuth());
        kf.planeElevation = static_cast<float>(layer->planeElevation());
        kf.planeRoll = static_cast<float>(layer->planeRoll());
        kf.planeDistance = static_cast<float>(layer->planeDistance());
        kf.planeHorizontal = static_cast<float>((layer->planeCircular() ? layer->planeCentreDegrees() : layer->planeHorizontal()));
        kf.planeVertical = static_cast<float>(layer->planeVertical());
    }
    sortKeyframes(track->keyframes);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

float LayersModel::evaluateAlphaAt(int layerIdx, int timeMs) const {
    const LayerTimeline *track = timelineAt(layerIdx);
    if (!track || track->keyframes.isEmpty())
        return 0.f;
    std::array<float, 1> a{0.f};
    evaluateGroup(track->keyframes, timeMs, [](const LayerKeyframe &) { return true; },
        [](const LayerKeyframe &k) { return std::array<float, 1>{k.alpha}; }, a);
    return std::clamp(a[0], 0.f, 1.f);
}

cplay::anim::LayerAnimState LayersModel::evaluateAnimState(int layerIdx, int timeMs) const {
    using namespace cplay::anim;
    LayerAnimState state;
    BaseLayer *layer = rowLayer(layerIdx);
    if (!layer)
        return state;

    // Render state carries offsets from the saved values.
    PlanePose pose;
    if (evaluateMovePose(layer, timeMs, pose))
        setPoseOffsets(layer, pose, state);

    const LayerTimeline *track = timelineAt(layerIdx);
    if (!track)
        return state;

    // Keyframe values are absolute.
    const auto &kfs = activeKeyframes(*track);
    std::array<float, 3> v3{};
    if (evaluateGroup(kfs, timeMs, [](const LayerKeyframe &k) { return k.hasRotate; },
            [](const LayerKeyframe &k) { return std::array<float, 3>{k.rotateX, k.rotateY, k.rotateZ}; }, v3))
        state.rotateOffset = glm::vec3(v3[0], v3[1], v3[2]) - layer->rotate();
    if (evaluateGroup(kfs, timeMs, [](const LayerKeyframe &k) { return k.hasTranslate; },
            [](const LayerKeyframe &k) { return std::array<float, 3>{k.translateX, k.translateY, k.translateZ}; }, v3))
        state.translateOffset = glm::vec3(v3[0], v3[1], v3[2]) - layer->translate();
    std::array<float, 1> v1{};
    if (evaluateGroup(kfs, timeMs, [](const LayerKeyframe &k) { return k.hasScale; },
            [](const LayerKeyframe &k) { return std::array<float, 1>{k.scale}; }, v1))
        state.scale = std::max(0.f, v1[0]);
    std::array<float, 6> v6{};
    if (evaluateGroup(kfs, timeMs, [](const LayerKeyframe &k) { return k.hasPlane; },
            [](const LayerKeyframe &k) {
                return std::array<float, 6>{k.planeAzimuth, k.planeElevation, k.planeRoll,
                                            k.planeHorizontal, k.planeVertical, k.planeDistance};
            }, v6)) {
        state.planeAngleOffset = glm::vec3(v6[0] - static_cast<float>(layer->planeAzimuth()),
                                           v6[1] - static_cast<float>(layer->planeElevation()),
                                           v6[2] - static_cast<float>(layer->planeRoll()));
        state.planePosOffset = glm::vec3(v6[3] - static_cast<float>((layer->planeCircular() ? layer->planeCentreDegrees() : layer->planeHorizontal())),
                                         v6[4] - static_cast<float>(layer->planeVertical()),
                                         v6[5] - static_cast<float>(layer->planeDistance()));
    }

    // Effects: intro effects run within [0, outroStart), exit effects from outroStart.
    const int outroStart = hasOutro() ? m_timelineOutroStart : m_timelineDuration;
    if (timeMs < outroStart || !hasOutro())
        applyScheduledEffects(layer, *track, m_introSchedule, timeMs, true, state);
    else
        applyScheduledEffects(layer, *track, m_outroSchedule, timeMs - outroStart, false, state);

    return state;
}

void LayersModel::applyScheduledEffects(const BaseLayer *layer, const LayerTimeline &track,
                                        const QVector<ScheduledEffect> &schedule, int localMs, bool intro,
                                        cplay::anim::LayerAnimState &state) const {
    using namespace cplay::anim;
    for (const ScheduledEffect &s : schedule) {
        if (s.layer != layer || s.effectIdx < 0 || s.effectIdx >= track.effects.size())
            continue;
        const LayerEffect &e = track.effects[s.effectIdx];
        if (effectPresetIsMove(e.preset))
            continue;
        const int dur = std::max(1, s.endMs - s.startMs);
        const float p = static_cast<float>(localMs - s.startMs) / static_cast<float>(dur);
        applyEffect(e, p, state);
        // Hidden until its entrance starts, and after its exit ends.
        if ((intro && localMs < s.startMs) || (!intro && localMs >= s.endMs))
            state.alphaMul = 0.f;
    }
}

void LayersModel::setPoseOffsets(const BaseLayer *layer, const cplay::anim::PlanePose &pose,
                                 cplay::anim::LayerAnimState &state) const {
    state.planeAngleOffset = glm::vec3(pose.azimuth - static_cast<float>(layer->planeAzimuth()),
                                       pose.elevation - static_cast<float>(layer->planeElevation()),
                                       pose.roll - static_cast<float>(layer->planeRoll()));
    state.planePosOffset = glm::vec3(pose.horizontal - static_cast<float>((layer->planeCircular() ? layer->planeCentreDegrees() : layer->planeHorizontal())),
                                     pose.vertical - static_cast<float>(layer->planeVertical()),
                                     pose.distance - static_cast<float>(layer->planeDistance()));
}

cplay::anim::PlanePose LayersModel::entryPose(const BaseLayer *layer) const {
    auto it = m_entryPoses.find(layer);
    if (it != m_entryPoses.end())
        return it->second;
    cplay::anim::PlanePose p;
    if (layer) {
        p.azimuth = static_cast<float>(layer->planeAzimuth());
        p.elevation = static_cast<float>(layer->planeElevation());
        p.roll = static_cast<float>(layer->planeRoll());
        p.distance = static_cast<float>(layer->planeDistance());
        p.horizontal = static_cast<float>((layer->planeCircular() ? layer->planeCentreDegrees() : layer->planeHorizontal()));
        p.vertical = static_cast<float>(layer->planeVertical());
    }
    return p;
}

bool LayersModel::evaluateMovePose(const BaseLayer *layer, int timeMs, cplay::anim::PlanePose &pose,
                                   const cplay::anim::LayerEffect *stopBefore) const {
    using namespace cplay::anim;
    pose = entryPose(layer);
    bool moved = m_entryPoses.find(layer) != m_entryPoses.end();
    auto it = m_layerTimelines.find(layer);
    if (it == m_layerTimelines.end())
        return moved;
    const LayerTimeline &track = it->second;
    const bool inOutro = hasOutro() && timeMs >= m_timelineOutroStart;
    const bool completed = runMoveEffects(layer, track, m_introSchedule,
                                          inOutro ? std::numeric_limits<int>::max() : timeMs,
                                          pose, moved, stopBefore);
    if (inOutro && completed)
        runMoveEffects(layer, track, m_outroSchedule, timeMs - m_timelineOutroStart, pose, moved, stopBefore);
    return moved;
}

bool LayersModel::runMoveEffects(const BaseLayer *layer, const LayerTimeline &track,
                                 const QVector<ScheduledEffect> &schedule, int localMs,
                                 cplay::anim::PlanePose &pose, bool &moved,
                                 const cplay::anim::LayerEffect *stopBefore) const {
    using namespace cplay::anim;
    // Each move starts where the previous one ended (or from the entry pose), unless it
    // has its own start; a first move with its own start holds it until it plays.
    PlanePose from = pose;
    bool first = true;
    for (const ScheduledEffect &s : schedule) {
        if (s.layer != layer || s.effectIdx < 0 || s.effectIdx >= track.effects.size())
            continue;
        const LayerEffect &e = track.effects[s.effectIdx];
        if (&e == stopBefore)
            return false;
        if (!effectPresetIsMove(e.preset))
            continue;
        moved = true;
        if (e.hasStart) {
            from = e.start;
            if (first)
                pose = e.start;
        }
        first = false;
        if (localMs < s.startMs)
            continue;
        const float p = std::clamp(static_cast<float>(localMs - s.startMs)
                                   / static_cast<float>(std::max(1, s.endMs - s.startMs)), 0.f, 1.f);
        pose = lerpPose(from, e.target, ease(e.easing, p));
        from = e.target;
    }
    return true;
}

void LayersModel::setEntryPoses(std::unordered_map<const BaseLayer *, cplay::anim::PlanePose> poses) {
    m_entryPoses = std::move(poses);
}

bool LayersModel::finalMovePose(const BaseLayer *layer, cplay::anim::PlanePose &pose) const {
    // Exit moves only run on layers leaving the show, so they are not carried on.
    return evaluateMovePose(layer, hasOutro() ? m_timelineOutroStart : std::numeric_limits<int>::max(), pose);
}

void LayersModel::applyCarriedPoses() {
    for (int l = 0; l < m_order.size(); ++l) {
        if (ownIndexOfRow(l) >= 0)
            continue;
        BaseLayer *layer = rowLayer(l);
        if (!layer || isTimelineSuppressed(layer) || hasTrackForLayer(layer))
            continue;
        layer->setAnimState(evaluateAnimState(l, 0));
        updateLayer(l);
    }
}

QVariantMap LayersModel::poseBeforeEffect(int layerIdx, int effectIdx) const {
    const LayerTimeline *track = timelineAt(layerIdx);
    const BaseLayer *layer = rowLayer(layerIdx);
    if (!track || !layer || effectIdx < 0 || effectIdx >= track->effects.size())
        return poseToMap(entryPose(layer));
    if (track->effects[effectIdx].hasStart)
        return poseToMap(track->effects[effectIdx].start);
    cplay::anim::PlanePose pose;
    evaluateMovePose(layer, std::numeric_limits<int>::max(), pose, &track->effects[effectIdx]);
    return poseToMap(pose);
}

void LayersModel::captureMovePose(int layerIdx, int effectIdx, bool end) {
    using namespace cplay::anim;
    LayerTimeline *track = timelineAt(layerIdx);
    BaseLayer *layer = rowLayer(layerIdx);
    if (!track || !layer || effectIdx < 0 || effectIdx >= track->effects.size()
        || !effectPresetIsMove(track->effects[effectIdx].preset))
        return;
    LayerEffect &e = track->effects[effectIdx];

    PlanePose before;
    const bool movedBefore = evaluateMovePose(layer, std::numeric_limits<int>::max(), before, &e);
    if (!e.hasStart) {
        e.start = before;
        e.hasStart = true;
    }

    const LayerAnimState &s = layer->animState();
    const bool anim = layer->animActive();
    PlanePose shown;
    shown.azimuth = static_cast<float>(layer->planeAzimuth()) + (anim ? s.planeAngleOffset.x : 0.f);
    shown.elevation = static_cast<float>(layer->planeElevation()) + (anim ? s.planeAngleOffset.y : 0.f);
    shown.roll = static_cast<float>(layer->planeRoll()) + (anim ? s.planeAngleOffset.z : 0.f);
    shown.horizontal = static_cast<float>((layer->planeCircular() ? layer->planeCentreDegrees() : layer->planeHorizontal())) + (anim ? s.planePosOffset.x : 0.f);
    shown.vertical = static_cast<float>(layer->planeVertical()) + (anim ? s.planePosOffset.y : 0.f);
    shown.distance = static_cast<float>(layer->planeDistance()) + (anim ? s.planePosOffset.z : 0.f);
    (end ? e.target : e.start) = shown;

    // Slides before the layer's first move show its saved pose, which a drag may have changed.
    if (!movedBefore) {
        layer->setPlaneAzimuth(e.start.azimuth);
        layer->setPlaneElevation(e.start.elevation);
        layer->setPlaneRoll(e.start.roll);
        if (layer->planeCircular()) layer->setPlaneCentreDegrees(e.start.horizontal);
        else layer->setPlaneHorizontal(e.start.horizontal);
        layer->setPlaneVertical(e.start.vertical);
        layer->setPlaneDistance(e.start.distance);
    }
    layer->setAnimState(evaluateAnimState(layerIdx, m_timelinePositionMs));
    updateLayer(layerIdx);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

bool LayersModel::effectPresetIsMove(int preset) const {
    return cplay::anim::effectPresetIsMove(preset);
}

bool LayersModel::layerHasKeyframes(int layerIdx) const {
    const LayerTimeline *track = timelineAt(layerIdx);
    return track && !track->keyframes.isEmpty();
}

bool LayersModel::layerHasEffects(int layerIdx) const {
    const LayerTimeline *track = timelineAt(layerIdx);
    return track && !track->effects.isEmpty();
}

bool LayersModel::isLayerTimelineDriven(const BaseLayer *layer) const {
    if (!layer)
        return false;
    auto it = m_layerTimelines.find(layer);
    if (it == m_layerTimelines.end())
        return false;
    if (!activeKeyframes(it->second).isEmpty())
        return true;
    return hasVisibilityEffects(it->second, cplay::anim::EffectPhase::Intro);
}

void LayersModel::clearAllAnimStates() {
    for (const auto &l : m_layers) {
        if (l.first && l.first->animActive() && !isTimelineSuppressed(l.first.get()))
            l.first->clearAnimState();
    }
    // A referenced layer keeps the pose it got from earlier slides unless this slide animates it.
    for (const auto &r : m_refs) {
        if (r.layer && r.layer->animActive() && !isTimelineSuppressed(r.layer.get())
            && isLayerTimelineDriven(r.layer.get()))
            r.layer->clearAnimState();
    }
}

void LayersModel::applyTimelineAt(int timeMs, bool skipReferences) {
    const bool inOutro = hasOutro() && timeMs >= m_timelineOutroStart;
    for (int l = 0; l < m_order.size(); ++l) {
        BaseLayer *layer = rowLayer(l);
        if (!layer || isTimelineSuppressed(layer))
            continue;
        const LayerTimeline *track = timelineAt(l);
        const bool isRef = ownIndexOfRow(l) < 0;
        if (isRef && skipReferences)
            continue;
        // Referenced layers without a track in this slide keep their visibility and the
        // pose earlier slides moved them to.
        if (isRef && !hasTrackForLayer(layer)) {
            layer->setAnimState(evaluateAnimState(l, timeMs));
            updateLayer(l);
            continue;
        }
        const bool hasKfs = track && !activeKeyframes(*track).isEmpty();
        const bool hasIntroEffects = track && hasVisibilityEffects(*track, cplay::anim::EffectPhase::Intro);

        if (hasKfs) {
            layer->setAlpha(evaluateAlphaAt(l, timeMs));
        }
        else if (hasIntroEffects) {
            // Visibility is shaped by the effects; keep the layer running once the intro started.
            layer->setAlpha(timeMs > 0 ? 1.f : 0.f);
        }
        else if (isRef) {
            // Only exit effects or moves: already visible when this slide starts.
            layer->setAlpha(1.f);
        }
        else {
            // No keyframes: fade in linearly, hold 1.0 afterwards
            const int introEnd = plainFadeInMs();
            if (introEnd > 0 && timeMs < introEnd && !inOutro) {
                float t = static_cast<float>(timeMs) / static_cast<float>(introEnd);
                layer->setAlpha(t);
            } else {
                layer->setAlpha(1.f);
            }
        }
        layer->setAnimState(evaluateAnimState(l, timeMs));
        updateLayer(l);
    }
}

void LayersModel::applyTimelineOutroAt(int outroTimeMs, const QVector<float> &targetAlphaPerLayer) {
    if (!hasOutro())
        return;
    int absTimeMs = std::max(m_timelineOutroStart, std::min(m_timelineOutroStart + outroTimeMs, m_timelineDuration));
    for (int l = 0; l < m_order.size(); ++l) {
        BaseLayer *layer = rowLayer(l);
        // Layers that continue in the live slide do not run this slide's exit.
        if (!layer || isTimelineSuppressed(layer))
            continue;
        const LayerTimeline *track = timelineAt(l);
        float targetAlpha = (l < targetAlphaPerLayer.size()) ? targetAlphaPerLayer[l] : layer->alpha();
        const bool hasOutroEffects = track && hasVisibilityEffects(*track, cplay::anim::EffectPhase::Outro);
        if (track && !activeKeyframes(*track).isEmpty()) {
            layer->setAlpha(evaluateAlphaAt(l, absTimeMs) * targetAlpha);
        }
        else if (hasOutroEffects) {
            layer->setAlpha(targetAlpha);
        }
        else {
            // No keyframes: layer is fully visible at the outro boundary, fades to 0
            const int dur = plainFadeOutMs();
            if (dur > 0) {
                float ft = static_cast<float>(std::min(outroTimeMs, dur)) / static_cast<float>(dur);
                layer->setAlpha(1.f - ft);
            }
        }
        layer->setAnimState(evaluateAnimState(l, absTimeMs));
        updateLayer(l);
    }
}

// ---- Entrance / exit effects ----------------------------------------------

void LayersModel::rebuildEffectSchedule() {
    using namespace cplay::anim;
    m_introSchedule.clear();
    m_outroSchedule.clear();
    m_introStepStarts.clear();
    m_introEffectsSpan = 0;
    m_outroEffectsSpan = 0;

    struct Item {
        const BaseLayer *layer;
        int layerIdx;
        int effectIdx;
        const LayerEffect *effect;
    };

    for (int phase = 0; phase < 2; ++phase) {
        QVector<Item> items;
        for (int l = 0; l < m_order.size(); ++l) {
            const LayerTimeline *track = timelineAt(l);
            if (!track)
                continue;
            for (int i = 0; i < track->effects.size(); ++i) {
                if (track->effects[i].phase == phase)
                    items.append({rowLayer(l), l, i, &track->effects[i]});
            }
        }
        std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) {
            if (a.effect->order != b.effect->order)
                return a.effect->order < b.effect->order;
            return a.layerIdx < b.layerIdx;
        });

        auto &schedule = phase == 0 ? m_introSchedule : m_outroSchedule;
        int prevStart = 0;
        int prevEnd = 0;
        int maxEnd = 0;
        int step = 0;
        for (int k = 0; k < items.size(); ++k) {
            const LayerEffect &e = *items[k].effect;
            int trigger = e.trigger;
            // Exits run automatically when the slide is left.
            if (phase == 1 && trigger == static_cast<int>(EffectTrigger::OnClick))
                trigger = static_cast<int>(EffectTrigger::AfterPrevious);

            int start = 0;
            if (trigger == static_cast<int>(EffectTrigger::OnClick)) {
                // A leading click step leaves room for the regular slide fade first.
                const int stepStart = k == 0 ? std::max(1, PresentationSettings::fadeDurationToNextSlide()) : maxEnd;
                ++step;
                m_introStepStarts.append(stepStart);
                start = stepStart + e.delayMs;
            }
            else if (trigger == static_cast<int>(EffectTrigger::WithPrevious)) {
                start = prevStart + e.delayMs;
            }
            else {
                start = prevEnd + e.delayMs;
            }
            const int end = start + std::max(1, e.durationMs);
            schedule.append({items[k].layer, items[k].effectIdx, start, end, step});
            prevStart = start;
            prevEnd = end;
            maxEnd = std::max(maxEnd, end);
        }
        if (phase == 0)
            m_introEffectsSpan = std::max(maxEnd, m_introStepStarts.isEmpty() ? 0 : m_introStepStarts.last());
        else
            m_outroEffectsSpan = maxEnd;
    }
    if (!m_hasTimeline)
        fitEffectsOnlyTiming();
    Q_EMIT animationStepChanged();
}

void LayersModel::fitTimelineToEffects() {
    // Slides without a timeline are fitted on every schedule rebuild.
    if (!m_hasTimeline)
        return;
    bool changed = false;
    const int introDur = introDuration();
    if (m_introEffectsSpan > introDur) {
        if (hasOutro()) {
            m_timelineDuration += m_introEffectsSpan - introDur;
            m_timelineOutroStart = m_introEffectsSpan;
        }
        else {
            m_timelineDuration = std::max(m_introEffectsSpan, 100);
        }
        changed = true;
    }
    if (m_outroEffectsSpan > outroDuration()) {
        if (!hasOutro())
            m_timelineOutroStart = std::max(m_timelineDuration, 1);
        m_timelineDuration = m_timelineOutroStart + m_outroEffectsSpan;
        changed = true;
    }
    if (changed)
        Q_EMIT timelineChanged();
}

void LayersModel::fitEffectsOnlyTiming() {
    const int fadeMs = std::max(1, PresentationSettings::fadeDurationToNextSlide());
    const int intro = std::max(fadeMs, m_introEffectsSpan);
    const int outro = std::max(fadeMs, m_outroEffectsSpan);
    m_timelineOutroStart = intro;
    m_timelineDuration = intro + outro;
}

bool LayersModel::hasAnyEffects() const {
    for (int l = 0; l < m_order.size(); ++l) {
        if (layerHasEffects(l))
            return true;
    }
    return false;
}

bool LayersModel::animationsActive() const {
    return m_hasTimeline || hasAnyEffects();
}

const QVector<LayerKeyframe> &LayersModel::activeKeyframes(const LayerTimeline &track) const {
    static const QVector<LayerKeyframe> none;
    return m_hasTimeline ? track.keyframes : none;
}

int LayersModel::plainFadeInMs() const {
    // Regular slides fade in over the slide fade; timeline slides over the first intro step.
    if (!m_hasTimeline)
        return std::min(std::max(1, PresentationSettings::fadeDurationToNextSlide()), introDuration());
    return !m_introStepStarts.isEmpty() ? m_introStepStarts.first() : introDuration();
}

int LayersModel::plainFadeOutMs() const {
    if (!m_hasTimeline)
        return std::min(std::max(1, PresentationSettings::fadeDurationToNextSlide()), outroDuration());
    return outroDuration();
}

QVariantList LayersModel::getEffects(int layerIdx) const {
    QVariantList result;
    const LayerTimeline *track = timelineAt(layerIdx);
    if (!track)
        return result;
    const BaseLayer *layer = rowLayer(layerIdx);
    for (int i = 0; i < track->effects.size(); ++i) {
        QVariantMap m = effectToMap(track->effects[i]);
        m[QStringLiteral("planeCircular")] = layer && layer->planeCircular();
        const auto &schedule = track->effects[i].phase == 0 ? m_introSchedule : m_outroSchedule;
        for (const ScheduledEffect &s : schedule) {
            if (s.layer == layer && s.effectIdx == i) {
                m[QStringLiteral("startMs")] = s.startMs;
                m[QStringLiteral("endMs")] = s.endMs;
                m[QStringLiteral("step")] = s.step;
                break;
            }
        }
        m[QStringLiteral("effectIdx")] = i;
        m[QStringLiteral("layerIdx")] = layerIdx;
        result.append(m);
    }
    return result;
}

QVariantList LayersModel::getSlideEffects(int phase) const {
    QVariantList result;
    const auto &schedule = phase == 0 ? m_introSchedule : m_outroSchedule;
    for (const ScheduledEffect &s : schedule) {
        const int layerIdx = rowOfLayer(s.layer);
        const LayerTimeline *track = timelineAt(layerIdx);
        if (!track || s.effectIdx < 0 || s.effectIdx >= track->effects.size())
            continue;
        QVariantMap m = effectToMap(track->effects[s.effectIdx]);
        m[QStringLiteral("layerIdx")] = layerIdx;
        m[QStringLiteral("effectIdx")] = s.effectIdx;
        m[QStringLiteral("layerTitle")] = layerTitle(layerIdx);
        m[QStringLiteral("startMs")] = s.startMs;
        m[QStringLiteral("endMs")] = s.endMs;
        m[QStringLiteral("step")] = s.step;
        result.append(m);
    }
    return result;
}

int LayersModel::addEffect(int layerIdx, int phase, int preset) {
    using namespace cplay::anim;
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track || !layerSupportsAnimation(layerIdx))
        return -1;
    LayerEffect e;
    e.phase = std::clamp(phase, 0, 1);
    e.preset = std::clamp(preset, 0, effectPresetCount() - 1);
    int maxOrder = -1;
    for (const auto &kv : m_layerTimelines) {
        for (const auto &other : kv.second.effects) {
            if (other.phase == e.phase)
                maxOrder = std::max(maxOrder, other.order);
        }
    }
    e.order = maxOrder + 1;
    e.trigger = static_cast<int>(EffectTrigger::WithPrevious);
    e.easing = static_cast<int>(e.phase == 0 ? Easing::OutCubic : Easing::InCubic);
    e.durationMs = std::max(1, PresentationSettings::fadeDurationToNextSlide());
    if (e.preset == static_cast<int>(EffectPreset::Appear))
        e.durationMs = 1;
    const bool isMove = effectPresetIsMove(e.preset);
    if (isMove)
        e.easing = static_cast<int>(Easing::InOutCubic);
    track->effects.append(e);
    rebuildEffectSchedule();
    // A new move starts as a no-op where the layer currently ends up.
    if (isMove)
        evaluateMovePose(rowLayer(layerIdx), std::numeric_limits<int>::max(), track->effects.last().target,
                         &track->effects.last());
    fitTimelineToEffects();
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
    return track->effects.size() - 1;
}

void LayersModel::updateEffect(int layerIdx, int effectIdx, const QVariantMap &values) {
    using namespace cplay::anim;
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track || effectIdx < 0 || effectIdx >= track->effects.size())
        return;
    LayerEffect &e = track->effects[effectIdx];
    const bool wasMove = effectPresetIsMove(e.preset);
    auto readInt = [&values](const char *key, int &out, int lo, int hi) {
        const QString k = QString::fromLatin1(key);
        if (values.contains(k))
            out = std::clamp(values.value(k).toInt(), lo, hi);
    };
    auto readFloat = [&values](const char *key, float &out, float lo, float hi) {
        const QString k = QString::fromLatin1(key);
        if (values.contains(k))
            out = std::clamp(static_cast<float>(values.value(k).toDouble()), lo, hi);
    };
    readInt("phase", e.phase, 0, 1);
    readInt("preset", e.preset, 0, effectPresetCount() - 1);
    readInt("trigger", e.trigger, 0, 2);
    readInt("delayMs", e.delayMs, 0, 3600000);
    readInt("durationMs", e.durationMs, 1, 3600000);
    readInt("easing", e.easing, 0, easingCount() - 1);
    readInt("direction", e.direction, 0, effectDirectionCount() - 1);
    readFloat("feather", e.feather, 0.f, 1.f);
    readFloat("amount", e.amount, 0.f, 10.f);
    readFloat("targetAzimuth", e.target.azimuth, -36000.f, 36000.f);
    readFloat("targetElevation", e.target.elevation, -36000.f, 36000.f);
    readFloat("targetRoll", e.target.roll, -36000.f, 36000.f);
    readFloat("targetDistance", e.target.distance, -1.0e6f, 1.0e6f);
    readFloat("targetHorizontal", e.target.horizontal, -1.0e6f, 1.0e6f);
    readFloat("targetVertical", e.target.vertical, -1.0e6f, 1.0e6f);
    if (values.contains(QStringLiteral("hasStart")))
        e.hasStart = values.value(QStringLiteral("hasStart")).toBool();
    readFloat("startAzimuth", e.start.azimuth, -36000.f, 36000.f);
    readFloat("startElevation", e.start.elevation, -36000.f, 36000.f);
    readFloat("startRoll", e.start.roll, -36000.f, 36000.f);
    readFloat("startDistance", e.start.distance, -1.0e6f, 1.0e6f);
    readFloat("startHorizontal", e.start.horizontal, -1.0e6f, 1.0e6f);
    readFloat("startVertical", e.start.vertical, -1.0e6f, 1.0e6f);
    rebuildEffectSchedule();
    if (!wasMove && effectPresetIsMove(e.preset))
        evaluateMovePose(rowLayer(layerIdx), std::numeric_limits<int>::max(), e.target, &e);
    fitTimelineToEffects();
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::removeEffect(int layerIdx, int effectIdx) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track || effectIdx < 0 || effectIdx >= track->effects.size())
        return;
    track->effects.removeAt(effectIdx);
    rebuildEffectSchedule();
    if (BaseLayer *layer = rowLayer(layerIdx))
        layer->clearAnimState();
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::moveEffect(int layerIdx, int effectIdx, int newPos) {
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track || effectIdx < 0 || effectIdx >= track->effects.size())
        return;
    const BaseLayer *layer = rowLayer(layerIdx);
    const auto &schedule = track->effects[effectIdx].phase == 0 ? m_introSchedule : m_outroSchedule;
    QVector<QPair<const BaseLayer *, int>> ordered;
    int current = -1;
    for (const ScheduledEffect &s : schedule) {
        if (s.layer == layer && s.effectIdx == effectIdx)
            current = ordered.size();
        ordered.append({s.layer, s.effectIdx});
    }
    if (current < 0)
        return;
    newPos = std::clamp(newPos, 0, static_cast<int>(ordered.size()) - 1);
    if (newPos == current)
        return;
    ordered.move(current, newPos);
    for (int i = 0; i < ordered.size(); ++i) {
        auto it = m_layerTimelines.find(ordered[i].first);
        if (it != m_layerTimelines.end() && ordered[i].second < it->second.effects.size())
            it->second.effects[ordered[i].second].order = i;
    }
    rebuildEffectSchedule();
    fitTimelineToEffects();
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

bool LayersModel::layerSupportsAnimation(int layerIdx) const {
    const BaseLayer *layer = rowLayer(layerIdx);
    return layer && layer->hasTexture();
}

void LayersModel::markLayerNeedsSave(int layerIdx) {
    const int ref = referenceIndexOf(rowLayer(layerIdx));
    if (ref < 0)
        setLayersNeedsSave(true);
    else if (m_refs[ref].owner)
        m_refs[ref].owner->setLayersNeedsSave(true);
}

QVariantMap LayersModel::getPostEffects(int layerIdx) const {
    const BaseLayer *layer = rowLayer(layerIdx);
    return postEffectsToMap(layer ? layer->postEffects() : PostEffects());
}

void LayersModel::updatePostEffects(int layerIdx, const QVariantMap &values) {
    BaseLayer *layer = rowLayer(layerIdx);
    if (!layer)
        return;
    PostEffects fx = layer->postEffects();
    applyPostEffectsMap(fx, values);
    layer->setPostEffects(fx);
    markLayerNeedsSave(layerIdx);
    Q_EMIT postEffectsChanged(layerIdx);
}

void LayersModel::resetPostEffect(int layerIdx, const QString &effect) {
    const QVariantMap defaults = postEffectsToMap(PostEffects());
    for (const PostEffectInfo &info : postEffectInfos()) {
        if (effect != QLatin1String(info.key))
            continue;
        QVariantMap values;
        for (const char *param : info.params)
            values[QString::fromLatin1(param)] = defaults.value(QString::fromLatin1(param));
        updatePostEffects(layerIdx, values);
        return;
    }
}

void LayersModel::clearPostEffects(int layerIdx) {
    BaseLayer *layer = rowLayer(layerIdx);
    if (!layer || !layer->postEffects().active())
        return;
    updatePostEffects(layerIdx, postEffectsToMap(PostEffects()));
}

bool LayersModel::layerHasPostEffects(int layerIdx) const {
    const BaseLayer *layer = rowLayer(layerIdx);
    return layer && layer->postEffects().active();
}

void LayersModel::previewEffect(int layerIdx, int effectIdx) {
    const LayerTimeline *track = timelineAt(layerIdx);
    if (!track || effectIdx < 0 || effectIdx >= track->effects.size())
        return;
    const BaseLayer *layer = rowLayer(layerIdx);
    if (track->effects[effectIdx].phase == 0) {
        for (const ScheduledEffect &s : m_introSchedule) {
            if (s.layer == layer && s.effectIdx == effectIdx) {
                startTimelineFrom(s.startMs);
                return;
            }
        }
    }
    else {
        stopTimeline();
        startTimelineOutro();
    }
}

bool LayersModel::convertEffectToKeyframes(int layerIdx, int effectIdx) {
    using namespace cplay::anim;
    LayerTimeline *track = timelineAt(layerIdx);
    if (!track || effectIdx < 0 || effectIdx >= track->effects.size())
        return false;
    const LayerEffect e = track->effects[effectIdx];
    if (e.preset != static_cast<int>(EffectPreset::Fade) && e.preset != static_cast<int>(EffectPreset::Zoom))
        return false;
    const BaseLayer *layer = rowLayer(layerIdx);
    const auto &schedule = e.phase == 0 ? m_introSchedule : m_outroSchedule;
    int startMs = -1;
    int endMs = -1;
    for (const ScheduledEffect &s : schedule) {
        if (s.layer == layer && s.effectIdx == effectIdx) {
            startMs = s.startMs;
            endMs = s.endMs;
        }
    }
    if (startMs < 0)
        return false;
    const int offset = e.phase == 0 ? 0 : (hasOutro() ? m_timelineOutroStart : m_timelineDuration);
    const bool intro = e.phase == 0;
    LayerKeyframe a;
    a.timeMs = offset + startMs;
    a.alpha = intro ? 0.f : 1.f;
    a.easing = e.easing;
    LayerKeyframe b;
    b.timeMs = offset + endMs;
    b.alpha = intro ? 1.f : 0.f;
    if (e.preset == static_cast<int>(EffectPreset::Zoom)) {
        a.hasScale = b.hasScale = true;
        const float hidden = std::max(0.f, 1.f - std::min(e.amount, 1.f));
        a.scale = intro ? hidden : 1.f;
        b.scale = intro ? 1.f : hidden;
    }
    track->effects.removeAt(effectIdx);
    track->keyframes.append(a);
    track->keyframes.append(b);
    sortKeyframes(track->keyframes);
    rebuildEffectSchedule();
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
    return true;
}

QStringList LayersModel::effectPresetNames() const {
    QStringList names;
    for (int i = 0; i < cplay::anim::effectPresetCount(); ++i)
        names.append(QString::fromLatin1(cplay::anim::effectPresetName(i)));
    return names;
}

int LayersModel::effectPresetCategory(int preset) const {
    return cplay::anim::effectPresetCategory(preset);
}

bool LayersModel::effectPresetUsesDirection(int preset) const {
    return cplay::anim::effectPresetUsesDirection(preset);
}

bool LayersModel::effectPresetUsesFeather(int preset) const {
    return cplay::anim::effectPresetUsesFeather(preset);
}

QStringList LayersModel::effectDirectionNames() const {
    QStringList names;
    for (int i = 0; i < cplay::anim::effectDirectionCount(); ++i)
        names.append(QString::fromLatin1(cplay::anim::effectDirectionName(i)));
    return names;
}

QStringList LayersModel::easingNames() const {
    QStringList names;
    for (int i = 0; i < cplay::anim::easingCount(); ++i)
        names.append(QString::fromLatin1(cplay::anim::easingName(i)));
    return names;
}

double LayersModel::easingValue(int easing, double t) const {
    return static_cast<double>(cplay::anim::ease(easing, static_cast<float>(t)));
}

// ---- On-click build steps ----------------------------------------------------

QVariantList LayersModel::introStepStarts() const {
    QVariantList result;
    for (int s : m_introStepStarts)
        result.append(s);
    return result;
}

int LayersModel::introStopAfter(int startMs) const {
    for (int s : m_introStepStarts) {
        if (s > startMs)
            return s;
    }
    return hasOutro() ? m_timelineOutroStart : m_timelineDuration;
}

bool LayersModel::hasPendingAnimationStep() const {
    if (m_outroRunning || !animationsActive())
        return false;
    if (m_timelineRunning)
        return !m_timelineReversed && m_timelineStopAtMs >= 0 && m_introStepStarts.contains(m_timelineStopAtMs);
    return m_introStepStarts.contains(m_timelinePositionMs);
}

bool LayersModel::playNextAnimationStep() {
    if (!hasPendingAnimationStep())
        return false;
    // Clicking during an animation finishes it and starts the next step right away.
    const int from = m_timelineRunning ? m_timelineStopAtMs : m_timelinePositionMs;
    startTimelineFrom(from);
    return true;
}

bool LayersModel::playPreviousAnimationStep() {
    if (m_outroRunning || m_introStepStarts.isEmpty() || !animationsActive())
        return false;
    const int pos = m_timelineRunning ? m_timelinePauseOffset : m_timelinePositionMs;
    int target = -1;
    for (int s : m_introStepStarts) {
        if (s < pos)
            target = s;
    }
    if (target < 0)
        return false;
    stopTimeline();
    applyTimelineAt(target);
    m_timelinePositionMs = target;
    Q_EMIT timelinePositionChanged(target);
    Q_EMIT animationStepChanged();
    return true;
}

// ---- Layer animation (one layer on its own) ---------------------------------

bool LayersModel::playLayerAnimation(int layerIdx, bool show) {
    LayerPlay play;
    if (!buildLayerPlay(layerIdx, show, play))
        return false;

    const BaseLayer *layer = play.layer;
    m_layerPlays.removeIf([layer](const LayerPlay &p) { return p.layer == layer; });
    m_layerFades.removeIf([layer](const LayerFade &f) { return f.layer == layer && f.subLayerIdx < 0; });
    play.elapsed.start();
    m_layerPlays.append(play);
    advanceLayerPlays();
    ensureTimerRunning();
    return true;
}

bool LayersModel::buildLayerPlay(int layerIdx, bool show, LayerPlay &play) const {
    using namespace cplay::anim;
    BaseLayer *layer = rowLayer(layerIdx);
    const LayerTimeline *track = timelineAt(layerIdx);
    const EffectPhase phase = show ? EffectPhase::Intro : EffectPhase::Outro;
    if (!layer || !track || !layerSupportsAnimation(layerIdx))
        return false;

    play.layer = layer;
    play.show = show;
    // The layer's effects of the phase in their slide order, timed among themselves:
    // there is no click to wait for, so "On click" follows the previous effect.
    QVector<int> order;
    for (int i = 0; i < track->effects.size(); ++i) {
        if (track->effects[i].phase == static_cast<int>(phase))
            order.append(i);
    }
    std::stable_sort(order.begin(), order.end(), [track](int a, int b) {
        return track->effects[a].order < track->effects[b].order;
    });
    if (order.isEmpty())
        return false;
    int prevStart = 0;
    int prevEnd = 0;
    for (int i : order) {
        const LayerEffect &e = track->effects[i];
        const int start = (e.trigger == static_cast<int>(EffectTrigger::WithPrevious) ? prevStart : prevEnd) + e.delayMs;
        const int end = start + std::max(1, e.durationMs);
        play.schedule.append({layer, i, start, end, 0});
        play.spanMs = std::max(play.spanMs, end);
        prevStart = start;
        prevEnd = end;
    }
    // Entrance moves start where the layer enters the slide, exit moves where its entrance left it.
    play.moved = show ? evaluateMovePose(layer, -1, play.startPose)
                      : finalMovePose(layer, play.startPose);
    return true;
}

bool LayersModel::scrubLayerAnimation(int layerIdx, int value) {
    BaseLayer *layer = rowLayer(layerIdx);
    if (!layer)
        return false;
    value = std::clamp(value, 0, 100);

    LayerPlay *play = nullptr;
    for (LayerPlay &p : m_layerPlays) {
        if (p.layer == layer) {
            play = &p;
            break;
        }
    }
    if (!play) {
        LayerPlay fresh;
        if (!buildLayerPlay(layerIdx, layer->alpha() < 1.f, fresh))
            return false;
        m_layerPlays.append(fresh);
        play = &m_layerPlays.last();
    }
    // Grabbing a running animation holds it where the slider is.
    play->scrubbing = true;
    play->scrubValue = value;
    play->scrubMs = static_cast<int>(static_cast<qint64>(play->spanMs) * (play->show ? value : 100 - value) / 100);

    if (value == 0) {
        layer->setAlpha(0.f);
        layer->clearAnimState();
    }
    else {
        layer->setAlpha(1.f);
        layer->setAnimState(evaluateLayerPlay(*play, play->scrubMs));
    }
    // Fully shown or hidden ends the scrub, like a finished animation.
    if (value == 0 || value == 100) {
        m_layerPlays.removeIf([layer](const LayerPlay &p) { return p.layer == layer; });
        stopTimerIfIdle();
    }
    updateLayer(layerIdx);
    // Keeps the other visibility slider of the layer (layer list or layer view) in step.
    Q_EMIT layerAnimationProgressChanged(layerIdx, value);
    return true;
}

int LayersModel::layerPlayMs(const LayerPlay &play) const {
    return play.scrubbing ? play.scrubMs : static_cast<int>(play.elapsed.elapsed());
}

bool LayersModel::hasTimedLayerPlays() const {
    for (const LayerPlay &p : m_layerPlays) {
        if (!p.scrubbing)
            return true;
    }
    return false;
}

bool LayersModel::layerAnimationRunning(int layerIdx) const {
    const BaseLayer *layer = rowLayer(layerIdx);
    for (const LayerPlay &p : m_layerPlays) {
        if (layer && p.layer == layer)
            return true;
    }
    return false;
}

void LayersModel::stopLayerAnimations() {
    if (m_layerPlays.isEmpty())
        return;
    QVector<int> rows;
    for (const LayerPlay &p : m_layerPlays)
        rows.append(rowOfLayer(p.layer));
    m_layerPlays.clear();
    stopTimerIfIdle();
    for (int row : rows) {
        if (row >= 0)
            Q_EMIT layerAnimationFinished(row);
    }
}

cplay::anim::LayerAnimState LayersModel::evaluateLayerPlay(const LayerPlay &play, int ms) const {
    using namespace cplay::anim;
    LayerAnimState state;
    auto it = m_layerTimelines.find(play.layer);
    if (it == m_layerTimelines.end())
        return state;
    PlanePose pose = play.startPose;
    bool moved = play.moved;
    runMoveEffects(play.layer, it->second, play.schedule, ms, pose, moved, nullptr);
    if (moved)
        setPoseOffsets(play.layer, pose, state);
    applyScheduledEffects(play.layer, it->second, play.schedule, ms, play.show, state);
    return state;
}

int LayersModel::layerPlayProgress(const BaseLayer *layer) const {
    for (const LayerPlay &p : m_layerPlays) {
        if (p.layer != layer)
            continue;
        if (p.scrubbing)
            return p.scrubValue;
        const int ms = layerPlayMs(p);
        const int done = std::clamp(static_cast<int>(100LL * ms / std::max(1, p.spanMs)), 0, 100);
        return p.show ? done : 100 - done;
    }
    return -1;
}

void LayersModel::advanceLayerPlays() {
    QVector<int> finished;
    for (int k = m_layerPlays.size() - 1; k >= 0; --k) {
        const LayerPlay &play = m_layerPlays[k];
        if (play.scrubbing)
            continue;
        const int row = rowOfLayer(play.layer);
        BaseLayer *layer = rowLayer(row);
        if (!layer) {
            m_layerPlays.removeAt(k);
            continue;
        }
        const int ms = static_cast<int>(play.elapsed.elapsed());
        const bool done = ms >= play.spanMs;
        if (done && !play.show) {
            // Hidden: the exit pose no longer matters.
            layer->setAlpha(0.f);
            layer->clearAnimState();
        }
        else {
            // The effects shape visibility; the layer itself is fully shown meanwhile.
            layer->setAlpha(1.f);
            layer->setAnimState(evaluateLayerPlay(play, done ? play.spanMs : ms));
        }
        updateLayer(row);
        if (done) {
            m_layerPlays.removeAt(k);
            finished.append(row);
        }
        else {
            Q_EMIT layerAnimationProgressChanged(row, layerPlayProgress(layer));
        }
    }
    if (!finished.isEmpty()) {
        setNeedSync();
        stopTimerIfIdle();
    }
    for (int row : finished)
        Q_EMIT layerAnimationFinished(row);
}

bool LayersModel::fadeLayer(const BaseLayer *layer, bool show, int subLayerIdx) {
    const int row = rowOfLayer(layer);
    if (row < 0)
        return false;
    if (subLayerIdx < 0 && playLayerAnimation(row, show))
        return true;

    LayerFade fade;
    fade.layer = layer;
    fade.subLayerIdx = subLayerIdx;
    int targetRow = -1;
    BaseLayer *target = layerFadeTarget(fade, targetRow);
    if (!target)
        return false;

    // A layer taken over from its entrance or exit fades from where that left it.
    if (subLayerIdx < 0 && layerAnimationRunning(row)) {
        m_layerPlays.removeIf([layer](const LayerPlay &p) { return p.layer == layer; });
        target->clearAnimState();
    }
    m_layerFades.removeIf([layer, subLayerIdx](const LayerFade &f) {
        return f.layer == layer && f.subLayerIdx == subLayerIdx;
    });
    fade.from = target->alpha();
    fade.to = show ? 1.f : 0.f;
    // Like the visibility slider, a partly faded layer only fades the rest of the way.
    fade.durationMs = static_cast<int>(PlaybackSettings::fadeDuration() * std::abs(fade.to - fade.from));
    fade.elapsed.start();
    m_layerFades.append(fade);
    advanceLayerFades();
    ensureTimerRunning();
    return true;
}

BaseLayer *LayersModel::layerFadeTarget(const LayerFade &fade, int &row) const {
    row = rowOfLayer(fade.layer);
    BaseLayer *layer = rowLayer(row);
    if (!layer || fade.subLayerIdx < 0)
        return layer;
    if (!layer->hasSubLayers())
        return nullptr;
    auto &subLayers = layer->getSubLayers();
    if (fade.subLayerIdx >= static_cast<int>(subLayers.size()))
        return nullptr;
    return subLayers[fade.subLayerIdx].get();
}

void LayersModel::advanceLayerFades() {
    QVector<int> finished;
    for (int k = m_layerFades.size() - 1; k >= 0; --k) {
        const LayerFade &fade = m_layerFades[k];
        int row = -1;
        BaseLayer *target = layerFadeTarget(fade, row);
        if (!target) {
            m_layerFades.removeAt(k);
            continue;
        }
        const int ms = static_cast<int>(fade.elapsed.elapsed());
        const bool done = ms >= fade.durationMs;
        const float t = done ? 1.f : static_cast<float>(ms) / static_cast<float>(fade.durationMs);
        target->setAlpha(fade.from + (fade.to - fade.from) * t);
        updateLayer(row);
        const bool ownRow = fade.subLayerIdx < 0;
        if (done) {
            m_layerFades.removeAt(k);
            if (ownRow)
                finished.append(row);
        }
        else if (ownRow) {
            // Keeps the layer's visibility sliders in step, as for an entrance or exit.
            Q_EMIT layerAnimationProgressChanged(row, qRound(target->alpha() * 100.f));
        }
    }
    setNeedSync();
    stopTimerIfIdle();
    for (int row : finished)
        Q_EMIT layerAnimationFinished(row);
}

// ---- Timeline playback (per-slide) ------------------------------------------

void LayersModel::startTimeline() {
    startTimelineFrom(0);
}

void LayersModel::startTimelineFrom(int startMs) {
    if (!animationsActive())
        return;

    stopTimeline();

    m_timelinePauseOffset = std::max(0, std::min(startMs, m_timelineDuration));
    m_timelineRunning     = true;
    m_timelineReversed    = false;
    m_timelinePositionMs  = m_timelinePauseOffset;
    m_timelineStopAtMs    = introStopAfter(m_timelinePauseOffset);
    m_timelineElapsed.start();
    ensureTimerRunning();

    Q_EMIT timelineStarted();
    Q_EMIT timelinePositionChanged(m_timelinePauseOffset);
    Q_EMIT animationStepChanged();
}

void LayersModel::startTimelineReverse() {
    if (!animationsActive())
        return;

    stopTimeline();

    m_timelinePauseOffset = 0;
    m_timelineRunning     = true;
    m_timelineReversed    = true;
    m_timelinePositionMs  = m_timelineDuration;
    m_timelineStopAtMs    = -1;
    m_timelineElapsed.start();
    ensureTimerRunning();

    Q_EMIT timelineStarted();
    Q_EMIT timelinePositionChanged(m_timelineDuration);
}

void LayersModel::startTimelineIntro() {
    if (!animationsActive())
        return;

    stopTimeline();

    m_timelinePauseOffset = 0;
    m_timelineRunning     = true;
    m_timelineReversed    = false;
    m_timelinePositionMs  = 0;
    m_timelineStopAtMs    = introStopAfter(0);
    m_timelineElapsed.start();
    ensureTimerRunning();

    Q_EMIT timelineStarted();
    Q_EMIT timelinePositionChanged(0);
    Q_EMIT animationStepChanged();
}

void LayersModel::startTimelineOutro(int fromOutroMs) {
    if (!animationsActive() || !hasOutro())
        return;

    stopOutro();

    applyTimelineAt(getTimelineOutroStart());

    m_outroTargetAlpha.clear();
    for (int l = 0; l < m_order.size(); ++l) {
        const BaseLayer *layer = rowLayer(l);
        m_outroTargetAlpha.append(layer ? layer->alpha() : 0.f);
    }

    m_outroOffsetMs = std::clamp(fromOutroMs, 0, outroDuration());
    m_outroRunning = true;
    m_timelinePositionMs = getTimelineOutroStart() + m_outroOffsetMs;
    m_outroElapsed.start();
    ensureTimerRunning();

    Q_EMIT outroStarted();
    Q_EMIT timelinePositionChanged(m_timelinePositionMs);
}

void LayersModel::stopTimeline() {
    if (!m_timelineRunning)
        return;
    m_timelineRunning = false;
    stopTimerIfIdle();
    Q_EMIT timelineStopped();
}

void LayersModel::stopOutro() {
    if (!m_outroRunning)
        return;
    m_outroRunning = false;
    m_outroTargetAlpha.clear();
    stopTimerIfIdle();
    Q_EMIT outroStopped();
}

void LayersModel::stopAllTimelines() {
    bool wasRunning = m_timelineRunning || m_outroRunning;
    m_timelineRunning = false;
    m_outroRunning    = false;
    m_outroTargetAlpha.clear();
    stopTimerIfIdle();
    if (wasRunning) {
        Q_EMIT timelineStopped();
        Q_EMIT outroStopped();
    }
}

bool LayersModel::timelineRunning() const {
    return m_timelineRunning;
}

bool LayersModel::timelineReversed() const {
    return m_timelineReversed;
}

int LayersModel::timelinePositionMs() const {
    return m_timelinePositionMs;
}

bool LayersModel::outroRunning() const {
    return m_outroRunning;
}

bool LayersModel::hasTimelineKeyframes() const {
    for (int l = 0; l < m_order.size(); ++l) {
        if (layerHasEffects(l) || (m_hasTimeline && layerHasKeyframes(l)))
            return true;
    }
    return false;
}

void LayersModel::jumpToIntroStart() {
    if (!animationsActive())
        return;

    stopTimeline();

    applyTimelineAt(0);
    // Used when the slide is reset after being left; carried-over layers that are not
    // part of the live slide must not stay visible at their time-0 pose.
    hideUnsuppressedReferences(false);
    m_timelinePositionMs = 0;

    int vis = timelineVisibility();
    if (vis >= 0)
        setLayersVisibility(vis, false);

    Q_EMIT timelinePositionChanged(0);
}

void LayersModel::jumpToOutroStart() {
    if (!animationsActive() || !hasOutro())
        return;

    stopTimeline();

    int outroStartMs = getTimelineOutroStart();
    applyTimelineAt(outroStartMs);
    m_timelinePositionMs = outroStartMs;

    int vis = timelineVisibility();
    if (vis >= 0)
        setLayersVisibility(vis, false);

    Q_EMIT timelinePositionChanged(outroStartMs);
}

int LayersModel::timelineVisibility() const {
    if (!hasTimelineKeyframes())
        return -1;

    const int posMs = m_timelinePositionMs;
    if (m_timelineDuration <= 0)
        return 0;

    // Follows the layers this slide does not animate.
    const int fadeIn = std::max(1, plainFadeInMs());
    if (!hasOutro() || posMs <= m_timelineOutroStart)
        return std::min(100, posMs * 100 / fadeIn);
    const int fadeOut = plainFadeOutMs();
    if (fadeOut <= 0)
        return 0;
    return std::max(0, 100 - (posMs - m_timelineOutroStart) * 100 / fadeOut);
}

void LayersModel::ensureTimerRunning() {
    if (m_timelineTimer && !m_timelineTimer->isActive())
        m_timelineTimer->start();
}

void LayersModel::stopTimerIfIdle() {
    if (!m_timelineRunning && !m_outroRunning && !hasTimedLayerPlays() && m_layerFades.isEmpty() && m_timelineTimer)
        m_timelineTimer->stop();
}

void LayersModel::onTimelineTick() {
    // --- Advance the intro / normal track ---
    if (m_timelineRunning) {
        if (!animationsActive()) {
            stopTimeline();
        } else {
            int elapsed  = static_cast<int>(m_timelineElapsed.elapsed()) + m_timelinePauseOffset;
            int duration = m_timelineDuration;
            int posMs = m_timelineReversed ? std::max(0, duration - elapsed) : elapsed;
            int stopAt = (!m_timelineReversed && m_timelineStopAtMs >= 0) ? m_timelineStopAtMs
                       : ((hasOutro() && !m_timelineReversed) ? introDuration() : duration);
            bool finished = m_timelineReversed ? (posMs <= 0) : (posMs >= stopAt);

            if (finished) {
                posMs = m_timelineReversed ? 0 : stopAt;
            }

            applyTimelineAt(posMs);
            m_timelinePositionMs = posMs;
            int vis = timelineVisibility();
            if (vis >= 0)
                setLayersVisibility(vis, false);
            Q_EMIT timelinePositionChanged(posMs);

            if (finished) {
                stopTimeline();
                Q_EMIT animationStepChanged();
            }
        }
    }

    // --- Advance the outro track ---
    if (m_outroRunning) {
        if (!animationsActive()) {
            stopOutro();
        } else {
            int elapsed    = static_cast<int>(m_outroElapsed.elapsed()) + m_outroOffsetMs;
            int outroDur   = outroDuration();
            int outroTimeMs = std::min(elapsed, outroDur);
            bool finished  = (outroTimeMs >= outroDur);

            if (finished) {
                applyTimelineOutroAt(outroDur, m_outroTargetAlpha);
                // References are hidden below; resetting them to time 0 would flash them at full alpha.
                applyTimelineAt(0, true);
                // The exit has finished, also when the live slide was faded out by re-triggering it.
                hideUnsuppressedReferences(true);
                m_timelinePositionMs = 0;
                setLayersVisibility(0, false);
                stopOutro();
                Q_EMIT timelinePositionChanged(0);
            } else {
                applyTimelineOutroAt(outroTimeMs, m_outroTargetAlpha);
                int currentPosMs = getTimelineOutroStart() + outroTimeMs;
                m_timelinePositionMs = currentPosMs;
                int vis = timelineVisibility();
                if (vis >= 0)
                    setLayersVisibility(vis, false);
                Q_EMIT timelinePositionChanged(currentPosMs);
            }
        }
    }

    // --- Layers shown or hidden on their own (after the slide, so they win) ---
    if (!m_layerPlays.isEmpty())
        advanceLayerPlays();
    if (!m_layerFades.isEmpty())
        advanceLayerFades();
}
