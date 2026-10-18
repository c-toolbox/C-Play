/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "slidesmodel.h"
#include "layersmodel.h"
#include "locationsettings.h"
#include "presentationsettings.h"
#include "layers/baselayer.h"
#include "utils/autosave.h"
#ifdef CAPTURE_SUPPORT
#include <capture/capturelayer.h>
#endif
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QThread>

#include <algorithm>
#include <functional>
#include <utility>

SlideVisibilityModel::SlideVisibilityModel(QList<QSharedPointer<LayersModel>>* slideList, QObject* parent)
    : QAbstractTableModel(parent), m_slideList(slideList){
    resetTable();
}

SlideVisibilityModel::~SlideVisibilityModel() {
}

int SlideVisibilityModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return static_cast<int>(m_visibilityLayers.size());
}

int SlideVisibilityModel::columnCount(const QModelIndex &parent) const {
    if (parent.isValid() || !m_slideList)
        return 0;

    //Column = Slides
    return static_cast<int>(m_slideList->size());
}

QVariant SlideVisibilityModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || !m_slideList || index.row() < 0 || index.column() < 0
        || index.row() >= static_cast<int>(m_visibilityLayers.size())
        || index.column() >= m_slideList->size()) {
        return QVariant();
    }

    //Rows = Layers
    //Column = Slides

    switch (role) {
    case DisplayRole:
        return QString(QStringLiteral("%1")).arg(calculateLocalIndex(index.column(), index.row()));
    case ColorRole:
        return cellColor(index.column(), index.row());
    case TextRole:
        return cellText(index.column(), index.row());
    case TimelineRole:
        return cellAnimated(index.column(), index.row());
    default:
        break;
    }

    return QVariant();
}

QVariant SlideVisibilityModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole || !m_slideList) {
        return QVariant();
    }

    if (orientation == Qt::Horizontal) {
        if(section >= 0 && section < m_slideList->size())
            return QString(QStringLiteral("%1")).arg(m_slideList->at(section)->getLayersName());
        else
            return QString(QStringLiteral("%1")).arg(section+1);
    }
    else if (orientation == Qt::Vertical) {
        //Count until we find correct layer
        int layerNum = 0;
        for (auto s : *m_slideList) {
            const Layers& slideLayers = s->getLayers();
            for (auto layer : slideLayers) {
                if (section == layerNum) {
                    return QString(QStringLiteral("%1")).arg(QString::fromStdString(layer.first->title()));
                }
                layerNum++;
            }
        }

        return QString(QStringLiteral("%1")).arg(section+1);
    }
    return QVariant();
}

QHash<int, QByteArray> SlideVisibilityModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[DisplayRole] = "display";
    roles[ColorRole] = "color";
    roles[TextRole] = "text";
    roles[TimelineRole] = "timeline";
    return roles;
}

std::vector<std::pair<BaseLayer*, int>> SlideVisibilityModel::getLayersVisibility(int slideIdx) {
    std::vector<std::pair<BaseLayer*, int>> layersOperationsInSlide;
    if (!m_slideList || slideIdx < 0 || slideIdx >= m_slideList->size())
        return layersOperationsInSlide;

    int columns = static_cast<int>(m_slideList->size());
    for (int row = 0; row < static_cast<int>(m_visibilityLayers.size()); row++) {
        int keepIdx = (row * columns) + slideIdx;
        if (keepIdx >= 0 && keepIdx < static_cast<int>(m_keepVisibilityMatrix.size()) && m_visibilityLayers[row])
            layersOperationsInSlide.push_back(std::make_pair(m_visibilityLayers[row], m_keepVisibilityMatrix[keepIdx]));
    }

    return layersOperationsInSlide;
}

void SlideVisibilityModel::cellClicked(int column, int row) {
    if (!m_slideList || column < 0 || row < 0 || column >= m_slideList->size()
        || row >= static_cast<int>(m_visibilityLayers.size()))
        return;

    BaseLayer* foundLayer = findLayer(row);
    if (foundLayer) {
        // Calculate local index
        int localIndex = calculateLocalIndex(column, row, true);
        if (localIndex >= 0) {
            int currentKVS = foundLayer->keepVisibilityForNumSlides();
            foundLayer->setKeepVisibilityForNumSlides(localIndex);

            int maxColumns = m_slideList->size()-1;
            int lowIdx = 0;
            int highIdx = maxColumns;
            if (localIndex > currentKVS) {
                lowIdx = std::max(column - localIndex, 0);
                highIdx = std::min(column + localIndex + 1, maxColumns);
            }
            else {
                lowIdx = std::max(column - localIndex, 0);
                highIdx = std::min(column + currentKVS + 1, maxColumns);
            }
            Q_EMIT dataChanged(index(row, lowIdx), index(row, highIdx));
            if (localIndex != currentKVS)
                Q_EMIT keepVisibilityChanged(foundLayer);
        }
    }
}

void SlideVisibilityModel::resetTable() {
    beginResetModel();

    if (!m_slideList) {
        m_keepVisibilityMatrix.clear();
        m_visibilityLayers.clear();
        m_layerOwnerSlide.clear();
        endResetModel();
        return;
    }

    // Calculate the table/matrix for visibility of layers across slides
    std::vector<int> newKeepVisibilityMatrix;

    // Below vector contains all layers, in order from first to last slide.
    std::vector<BaseLayer*> newVisibilityLayers;

    // Track which slide index owns each layer row
    std::vector<int> newLayerOwnerSlide;

    // Negative number if column number is before the layer owner (the slide it is own by)
    // 0 if the column number equals the layer owner
    // Positive number for large then the layer owner
    
    //Column = Slides
    int columns = m_slideList->size();

    //Rows = Layers
    int slideIdx = 0;
    for (auto s : *m_slideList) {
        const Layers& slideLayers = s->getLayers();
        for (auto layer : slideLayers) {
            newVisibilityLayers.push_back(layer.first.get());
            newLayerOwnerSlide.push_back(slideIdx);
        }
        slideIdx++;
    }
    int rows = static_cast<int>(newVisibilityLayers.size());

    //Fill matrix
    for (int row = 0; row < rows; row++) {
        for (int column = 0; column < columns; column++) {
            int layerNum = 0;
            int slideNum = 0;
            bool storedValue = false;
            for (auto s : *m_slideList) {
                const Layers& slideLayers = s->getLayers();
                for (int i = 0; i < slideLayers.size(); i++) {
                    if (row == layerNum) {
                        if (!storedValue) {
                            newKeepVisibilityMatrix.push_back(column - slideNum);
                            storedValue = true;
                        }                    
                    }
                    layerNum++;
                }
                slideNum++;
            }
            if (!storedValue) {
                newKeepVisibilityMatrix.push_back(column - slideNum);
                storedValue = true;
            }
        }
    }

    m_keepVisibilityMatrix = newKeepVisibilityMatrix;
    m_visibilityLayers = newVisibilityLayers;
    m_layerOwnerSlide = newLayerOwnerSlide;

    endResetModel();
}

BaseLayer* SlideVisibilityModel::findLayer(int row) {
    int layerNum = 0;
    for (auto s : *m_slideList) {
        const Layers& slideLayers = s->getLayers();
        for (auto layer : slideLayers) {
            if (row == layerNum && layer.first) {
                return layer.first.get();
            }
            layerNum++;
        }
    }
    return nullptr;
}

int SlideVisibilityModel::calculateLocalIndex(int column, int row, bool ignoreKeepValue) const {
    if (!m_slideList || column < 0 || row < 0)
        return -1;

    // Negative number if column number is before the layer owner (the slide it is own by)
    // 0 if the column number equals the layer owner
    // Positive number for large then the layer owner

    int layerNum = 0;
    int slideNum = 0;
    for (auto s : *m_slideList) {
        const Layers& slideLayers = s->getLayers();
        for (auto layer : slideLayers) {
            if (row == layerNum) {
                if (!layer.first)
                    return -1;
                if(ignoreKeepValue)
                    return column - slideNum;
                else
                    return column - slideNum - layer.first->keepVisibilityForNumSlides();
            }
            layerNum++;
        }
        slideNum++;
    }

    return column - slideNum;
}

QString SlideVisibilityModel::cellColor(int column, int row) const {
    // Negative number = grey
    // First zero, where visibility starts = green (or cyan if timeline keyframes)
    // Other zeros, still visible = white (or cyan if that slide animates the layer)
    // One, where layer fade = red
    // Positive value above one = black

    if (!m_slideList || column < 0 || row < 0 || column >= m_slideList->size()
        || row >= static_cast<int>(m_visibilityLayers.size()))
        return QStringLiteral("grey");

    int keepIdx = (row * m_slideList->size()) + column;

    if(keepIdx < 0 || keepIdx >= static_cast<int>(m_keepVisibilityMatrix.size()) || !m_visibilityLayers[row])
        return QStringLiteral("grey");

    int val = m_keepVisibilityMatrix[keepIdx];

    if (val == 0) {
        if (cellAnimated(column, row))
            return QStringLiteral("cyan");
        return QStringLiteral("green");
    }
    else if (val < 0) {
        return QStringLiteral("grey");
    }

    val -= m_visibilityLayers[row]->keepVisibilityForNumSlides();
    if (val <= 0) {
        if (cellAnimated(column, row))
            return QStringLiteral("cyan");
        return QStringLiteral("white");
    }
    else if (val == 1) {
        return QStringLiteral("red");
    }
    else {
        return QStringLiteral("black");
    }
}

QString SlideVisibilityModel::cellText(int column, int row) const {
    int keepIdx = m_slideList ? (row * m_slideList->size()) + column : -1;

    if (!m_slideList || column < 0 || row < 0 || column >= m_slideList->size()
        || keepIdx < 0 || keepIdx >= static_cast<int>(m_keepVisibilityMatrix.size())
        || row >= static_cast<int>(m_visibilityLayers.size()) || !m_visibilityLayers[row])
        return QStringLiteral("");

    int val = m_keepVisibilityMatrix[keepIdx];
    if(val == 0) {
        if (cellAnimated(column, row))
            return QStringLiteral("Timeline Intro");
        return QStringLiteral("0% -> 100%");
    }
    else if (val < 0)
        return QStringLiteral("");

    val -= m_visibilityLayers[row]->keepVisibilityForNumSlides();
    if (val <= 0) {
        // Kept from an earlier slide: listed as a reference in this slide.
        if (cellAnimated(column, row))
            return QStringLiteral("Ref. Timeline");
        return QStringLiteral("Ref. 100%");
    }
    else if (val == 1) {
        // The last slide showing the layer runs its exit.
        if (cellAnimated(column - 1, row))
            return QStringLiteral("Timeline Outro");
        return QStringLiteral("100% -> 0%");
    }
    else {
        return QStringLiteral("");
    }
}

bool SlideVisibilityModel::cellAnimated(int column, int row) const {
    if (!m_slideList || row < 0 || row >= static_cast<int>(m_layerOwnerSlide.size())
        || row >= static_cast<int>(m_visibilityLayers.size())
        || column < 0 || column >= m_slideList->size() || !m_slideList->at(column))
        return false;

    // A timeline slide plays its whole timeline over its own layers; otherwise a slide
    // only animates the layers it has effects (or a track) for.
    const LayersModel *slide = m_slideList->at(column).get();
    if (m_layerOwnerSlide[row] == column && slide->getHasTimeline())
        return slide->hasTimelineKeyframes();
    return slide->hasTrackForLayer(m_visibilityLayers[row]);
}

SlidesModel::SlidesModel(QObject *parent)
    : QAbstractListModel(parent),
    m_masterSlide(new LayersModel(this)),
    m_dummySlide(new LayersModel(this))
    , m_visibilityModel(new SlideVisibilityModel(&m_slides, this))
    , m_needSync(false)
    , m_syncIteration(0)
    , m_slideFadeTime(0)
    , m_slidesName(QStringLiteral(""))
    , m_slidesPath(QStringLiteral("")) {
    m_masterSlide->setLayersName(QStringLiteral("Master"));
    m_masterSlide->setHierarchy(BaseLayer::LayerHierarchy::BACK);
    m_masterSlide->setLayersCanBeLocked(true);
    m_dummySlide->setLayersEnabled(false);
    m_preloadLayers = PresentationSettings::preLoadLayers();
    m_clearCopyTimer = new QTimer(this);
    m_clearCopyTimer->setInterval(30000);
    m_clearCopyTimer->setSingleShot(true);
    connect(m_clearCopyTimer, &QTimer::timeout, this, &SlidesModel::clearCopyLayer);
    m_autosaveTimer = new QTimer(this);
    m_autosaveTimer->setInterval(Autosave::intervalMs);
    m_autosaveTimer->setSingleShot(true);
    connect(m_autosaveTimer, &QTimer::timeout, this, &SlidesModel::autosave);
    // Not restarted by further edits, so continuous edits are still autosaved regularly.
    // The timer as context queues the start when the flag is set from the render thread.
    connect(this, &SlidesModel::slidesNeedsSaveChanged, m_autosaveTimer, [this]() {
        if (m_slidesNeedsSave && !m_autosaveTimer->isActive())
            m_autosaveTimer->start();
    });
    // Edits since the last autosave are written on exit, before the slides are cleared for shutdown.
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [this]() {
        if (m_autosaveTimer->isActive()) {
            m_autosaveTimer->stop();
            autosave();
        }
    });
    connect(this, &SlidesModel::slideModelChanged, this, &SlidesModel::rebuildReferences);
    connect(this, &SlidesModel::slideModelChanged, m_visibilityModel, &SlideVisibilityModel::resetTable);
    connect(m_visibilityModel, &SlideVisibilityModel::keepVisibilityChanged, this, [this](BaseLayer *layer) {
        rebuildReferences();
        updateReferenceOutroFades(layer);
        setSlidesNeedsSave(true);
    });
    connect(m_masterSlide, &LayersModel::layersNeedsSaveChanged, this, &SlidesModel::setLayersNeedsSave);
}

SlidesModel::~SlidesModel() {
    clearAllForShutdown();
    m_slides.clear();
    delete m_masterSlide;
    delete m_dummySlide;
    delete m_visibilityModel;
}

int SlidesModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_slides.size();
}

QVariant SlidesModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_slides.empty())
        return QVariant();

    const int row = index.row();
    if (row < 0 || row >= m_slides.size() || !m_slides[row])
        return QVariant();

    auto slideItem = m_slides.at(row);

    switch (role) {
    case NameRole:
        return QVariant(slideItem->getLayersName());
    case PathRole:
        return QVariant(slideItem->getLayersPath());
    case LayersRole:
        return QVariant(slideItem->numberOfLayers());
    case RefsRole:
        return QVariant(slideItem->numberOfReferences());
    case LayerMinStatusRole:
        return QVariant(slideItem->minLayerStatus());
    case LayerMaxStatusRole:
        return QVariant(slideItem->maxLayerStatus());
    case LockedRole:
        return QVariant(slideItem->getLayersCanBeLocked());
    case LockedCountRole:
        return QVariant(slideItem->getLockedLayerCount());
    case VisibilityRole: {
        int tlVis = slideTimelineVisibility(index.row());
        if (tlVis >= 0)
            return QVariant(tlVis);
        return QVariant(slideItem->getLayersVisibility());
    }
    }

    return QVariant();
}

QHash<int, QByteArray> SlidesModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[PathRole] = "filepath";
    roles[LayersRole] = "layers";
    roles[RefsRole] = "refs";
    roles[LayerMinStatusRole] = "layerminstatus";
    roles[LayerMaxStatusRole] = "layermaxstatus";
    roles[LockedRole] = "locked";
    roles[LockedCountRole] = "countlocked";
    roles[VisibilityRole] = "visibility";
    return roles;
}

int SlidesModel::numberOfSlides() {
    return m_slides.size();
}

bool SlidesModel::needsSync() {
    return m_needSync;
}

void SlidesModel::setNeedsSync(bool value) {
    if (value) {
        setNeedSync();
    }
    else {
        m_needSync = false;
    }
    Q_EMIT needsSyncChanged();
}

void SlidesModel::markAllLayersNeedSync() {
    std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
    for (auto& slide : m_slides) {
        if (slide)
            slide->markAllLayersNeedSync();
    }
}

void SlidesModel::setHasSynced() {
    if (m_syncIteration > 0) {
        m_syncIteration--;
    }
    else {
        m_needSync = false;
    }
}

bool SlidesModel::preLoadLayers() {
    return m_preloadLayers;
}

void SlidesModel::setPreLoadLayers(bool value) {
    m_preloadLayers = value;
    setNeedSync();
    Q_EMIT preLoadLayersChanged();
}

int SlidesModel::selectedSlideIdx() {
    return m_selectedSlideIdx;
}

void SlidesModel::setSelectedSlideIdx(int value) {
    m_previousSelectedSlideIdx = m_selectedSlideIdx;
    m_selectedSlideIdx = std::max(value, -1);
    if (m_selectedSlideIdx >= 0 && m_selectedSlideIdx < numberOfSlides()) {
        for (auto layer : m_slides[m_selectedSlideIdx]->getLayers()) {
            if (layer.first)
                layer.first->setShouldPreLoad(true);
        }
    }
    else if (m_selectedSlideIdx == -1) {
        for (auto layer : m_masterSlide->getLayers()) {
            if (layer.first)
                layer.first->setShouldPreLoad(true);
        }
    }
    Q_EMIT selectedSlideChanged();
}

int SlidesModel::previousSlideIdx() {
    return m_previousSelectedSlideIdx;
}

int SlidesModel::nextSlideIdx() {
    if (m_selectedSlideIdx + 1 < m_slides.size())
        return m_selectedSlideIdx + 1;
    else if (m_slides.empty())
        return -1;
    else
        return 0;
}

int SlidesModel::triggeredSlideIdx() {
    return m_triggeredSlideIdx;
}

void SlidesModel::setTriggeredSlideIdx(int value) {
    m_previousTriggeredSlideIdx = m_triggeredSlideIdx;
    m_triggeredSlideIdx = std::max(value, -1);

    updateTimelineSuppression();
    if (m_triggeredSlideIdx >= 0 && m_triggeredSlideIdx < m_slides.size() && m_slides[m_triggeredSlideIdx])
        m_slides[m_triggeredSlideIdx]->applyCarriedPoses();
    // The render stack follows the triggered slide; nodes need the new layer order.
    if (m_previousTriggeredSlideIdx != m_triggeredSlideIdx)
        setNeedSync();

    Q_EMIT triggeredSlideChanged();
}

int SlidesModel::triggeredSlideVisibility() {
    if (m_triggeredSlideIdx >= 0 && m_triggeredSlideIdx < m_slides.size())
        return slide(m_triggeredSlideIdx)->getLayersVisibility();
    else
        return 0;
}

void SlidesModel::setTriggeredSlideVisibility(int value) {
    // Visibiity scheme
    // Calculate based on keepVisibilityForNumSlides which layers should
    // have 100%(i.e. value) or which should have 0% (100 - value)
    // The SlideVisibilityModel has the visbility matrix precalculated
    // to run this scheme on.

    float valF = static_cast<float>(value) * 0.01f;

    // Propogate down
    if (m_triggeredSlideIdx >= 0 && m_triggeredSlideIdx < m_slides.size()) {
        std::vector<std::pair<BaseLayer*, int>> layers = m_visibilityModel->getLayersVisibility(m_triggeredSlideIdx);
        for (int i = 0; i < layers.size(); i++) {
            if (!layers[i].first)
                continue;
            int localIdx = layers[i].second;

            if (localIdx == 0) { // Exact
                layers[i].first->setAlpha(valF); // 0&->100%
                continue;
            }
            else if (localIdx < 0) {  //Upcoming
                layers[i].first->setAlpha(0.f); //0%

                //Let's start updating certain number of upcoming slides
                if (-localIdx <= PresentationSettings::updateUpcomingSlideCount()) {
                    layers[i].first->setShouldPreLoad(true);
                }
                continue;
            }

            localIdx -= layers[i].first->keepVisibilityForNumSlides();
            if (localIdx <= 0) {
                // Kept from an earlier slide; this slide's timeline drives it if it animates it.
                if (!slide(m_triggeredSlideIdx)->isLayerTimelineDriven(layers[i].first))
                    layers[i].first->setAlpha(1.f); // 100%
                continue;
            }
            else if (localIdx == 1) {
                if(layers[i].first->alpha() > 0.f)
                    layers[i].first->setAlpha(1.f - valF); //100%->0%
                continue;
            }
            else {
                layers[i].first->setAlpha(0.f); // 0%
                // Slide hidden again. Let's skip it.
                layers[i].first->setShouldUpdate(false);
                continue;
            }
        }

        slide(m_triggeredSlideIdx)->setLayersVisibility(value, false);
        updateSlide(m_triggeredSlideIdx);

        if (m_previousTriggeredSlideIdx != m_triggeredSlideIdx
            && m_previousTriggeredSlideIdx >= 0 && m_previousTriggeredSlideIdx < m_slides.size()
            && slide(m_previousTriggeredSlideIdx)->getLayersVisibility() > 0){
            slide(m_previousTriggeredSlideIdx)->setLayersVisibility(100 - value, false);
            updateSlide(m_previousTriggeredSlideIdx);
        }
    }

    Q_EMIT triggeredSlideVisibilityChanged();
}

int SlidesModel::slideFadeTime() {
    return m_slideFadeTime;
}

void SlidesModel::setSlideFadeTime(int value) {
    m_slideFadeTime = value;
    Q_EMIT slideFadeTimeChanged();
}

int SlidesModel::previousTriggeredIdx() {
    return m_previousTriggeredSlideIdx;
}

LayersModel *SlidesModel::masterSlide() {
    return m_masterSlide;
}

LayersModel *SlidesModel::slide(int i) {
    if (i == -1)
        return masterSlide();
    else if (i >= 0 && m_slides.size() > i && m_slides[i])
        return m_slides[i].get();
    else
        return m_dummySlide;
}

LayersModel* SlidesModel::slide(std::string name) {
    // Returning first with correct name
    QString nameLowCase = QString::fromStdString(name).toLower();
    if (nameLowCase == QStringLiteral("master")) {
        return masterSlide();
    }
    for (int i = 0; i < m_slides.size(); i++) {
        if (m_slides[i] && m_slides[i]->getLayersName().toLower() == nameLowCase) {
            return slide(i);
        }
    }
    return nullptr;
}

LayersModel *SlidesModel::selectedSlide() {
    return slide(m_selectedSlideIdx);
}

QSharedPointer<LayersModel> SlidesModel::slideShared(int i) {
    std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
    if (i >= 0 && m_slides.size() > i && m_slides[i])
        return m_slides[i];
    else
        return QSharedPointer<LayersModel>();
}

QList<QSharedPointer<LayersModel>> SlidesModel::snapshotSlides() {
    std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
    return m_slides;
}

bool SlidesModel::trySnapshotSlides(QList<QSharedPointer<LayersModel>>& out) {
    std::unique_lock<std::recursive_mutex> lock(m_slidesMutex, std::try_to_lock);
    if (!lock.owns_lock())
        return false;
    out = m_slides;
    return true;
}

std::vector<std::shared_ptr<BaseLayer>> SlidesModel::buildRenderStack(const QList<QSharedPointer<LayersModel>>& slides) const {
    std::vector<std::shared_ptr<BaseLayer>> stack;
    const int t = m_triggeredSlideIdx;

    std::vector<std::shared_ptr<BaseLayer>> top;
    QSet<const BaseLayer*> inTop;
    if (t >= 0 && t < slides.size() && slides[t]) {
        top = slides[t]->renderOrderShared();
        for (const auto& l : top)
            inTop.insert(l.get());
    }

    // Other slides (e.g. one still fading out) stay below the triggered slide.
    for (int s = 0; s < slides.size(); ++s) {
        if (!slides[s] || s == t)
            continue;
        const Layers owned = slides[s]->getLayers();
        for (int l = static_cast<int>(owned.size()) - 1; l >= 0; --l) {
            if (owned[l].first && !inTop.contains(owned[l].first.get()))
                stack.push_back(owned[l].first);
        }
    }
    for (auto it = top.rbegin(); it != top.rend(); ++it) {
        if (*it)
            stack.push_back(*it);
    }
    return stack;
}

bool SlidesModel::tryRenderStack(std::vector<std::shared_ptr<BaseLayer>>& out) {
    QList<QSharedPointer<LayersModel>> slides;
    if (!trySnapshotSlides(slides))
        return false;
    out = buildRenderStack(slides);
    return true;
}

void SlidesModel::rebuildReferences() {
    const QList<QSharedPointer<LayersModel>> slides = snapshotSlides();

    QSet<const BaseLayer*> alive;
    for (const auto& s : slides) {
        if (!s)
            continue;
        for (const auto& l : s->getLayers()) {
            if (l.first)
                alive.insert(l.first.get());
        }
    }

    for (int j = 0; j < slides.size(); ++j) {
        if (!slides[j])
            continue;
        QVector<LayerReference> refs;
        // Nearest earlier slide first.
        for (int i = j - 1; i >= 0; --i) {
            if (!slides[i])
                continue;
            const Layers owned = slides[i]->getLayers();
            for (int k = 0; k < owned.size(); ++k) {
                const auto& l = owned[k].first;
                if (l && l->keepVisibilityForNumSlides() >= j - i) {
                    LayerReference r;
                    r.layer = l;
                    r.owner = slides[i].get();
                    r.slideOffset = j - i;
                    r.ownerLayerIdx = k;
                    refs.append(r);
                }
            }
        }
        slides[j]->pruneReferenceTracks(alive);
        slides[j]->setReferencedLayers(refs);
    }
    if (rowCount() > 0)
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {RefsRole});

    updateTimelineSuppression();
    scheduleMovePoseUpdate();
}

void SlidesModel::updateReferenceOutroFades(BaseLayer *layer) {
    if (!layer)
        return;
    const QList<QSharedPointer<LayersModel>> slides = snapshotSlides();

    int owner = -1;
    for (int i = 0; i < slides.size() && owner < 0; ++i) {
        if (!slides[i])
            continue;
        for (const auto &l : slides[i]->getLayers()) {
            if (l.first.get() == layer) {
                owner = i;
                break;
            }
        }
    }
    if (owner < 0)
        return;

    const int last = owner + layer->keepVisibilityForNumSlides();
    bool changed = false;
    for (int j = owner + 1; j < slides.size(); ++j) {
        if (!slides[j])
            continue;
        if (j == last)
            changed |= slides[j]->addReferenceOutroFade(layer);
        else
            changed |= slides[j]->removeReferenceOutroFade(layer);
    }
    if (changed)
        m_visibilityModel->resetTable();
}

void SlidesModel::scheduleMovePoseUpdate() {
    if (m_movePoseUpdatePending)
        return;
    m_movePoseUpdatePending = true;
    QTimer::singleShot(0, this, &SlidesModel::updateMovePoses);
}

void SlidesModel::updateMovePoses() {
    m_movePoseUpdatePending = false;
    const QList<QSharedPointer<LayersModel>> slides = snapshotSlides();

    std::unordered_map<const BaseLayer*, cplay::anim::PlanePose> carried;
    for (const auto& s : slides) {
        if (!s)
            continue;
        const auto order = s->renderOrderShared();
        std::unordered_map<const BaseLayer*, cplay::anim::PlanePose> entry;
        for (const auto& l : order) {
            auto it = carried.find(l.get());
            if (it != carried.end())
                entry.emplace(l.get(), it->second);
        }
        s->setEntryPoses(std::move(entry));
        for (const auto& l : order) {
            cplay::anim::PlanePose pose;
            if (l && s->finalMovePose(l.get(), pose))
                carried[l.get()] = pose;
        }
    }

    const int t = m_triggeredSlideIdx;
    if (t >= 0 && t < slides.size() && slides[t])
        slides[t]->applyCarriedPoses();
}

void SlidesModel::updateTimelineSuppression() {
    const QList<QSharedPointer<LayersModel>> slides = snapshotSlides();
    const int t = m_triggeredSlideIdx;

    QSet<const BaseLayer*> live;
    if (t >= 0 && t < slides.size() && slides[t]) {
        for (const auto& l : slides[t]->renderOrderShared())
            live.insert(l.get());
    }

    for (int i = 0; i < slides.size(); ++i) {
        if (!slides[i])
            continue;
        if (i == t) {
            slides[i]->setTimelineSuppressedLayers(QSet<const BaseLayer*>(), true);
            continue;
        }
        QSet<const BaseLayer*> suppressed;
        for (const auto& l : slides[i]->renderOrderShared()) {
            if (live.contains(l.get()))
                suppressed.insert(l.get());
        }
        slides[i]->setTimelineSuppressedLayers(suppressed, false);
    }
}

SlideVisibilityModel* SlidesModel::visibilityModel() {
    return m_visibilityModel;
}

int SlidesModel::addSlide() {
    {
        std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
        beginInsertRows(QModelIndex(), m_slides.size(), m_slides.size());
        m_slides.push_back(QSharedPointer<LayersModel>(new LayersModel(this)));
        setSlidesNeedsSave(true);
        setNeedSync();
        endInsertRows();

        connect(m_slides[m_slides.size() - 1].get(), &LayersModel::layersModelChanged, this, &SlidesModel::slideContentChanged);
        connect(m_slides[m_slides.size() - 1].get(), &LayersModel::layersNeedsSaveChanged, this, &SlidesModel::setLayersNeedsSave);
        connect(m_slides[m_slides.size() - 1].get(), &LayersModel::timelineChanged, this, &SlidesModel::scheduleMovePoseUpdate);
        connect(m_slides[m_slides.size() - 1].get(), &LayersModel::layerVisibilitySpanChanged, this, &SlidesModel::updateReferenceOutroFades);

        LayersModel *lm = m_slides[m_slides.size() - 1].get();
        connect(lm, &LayersModel::layersVisibilityChanged, this, [this, lm]() {
            for (int i = 0; i < m_slides.size(); ++i) {
                if (m_slides[i].get() == lm) {
                    updateSlide(i);
                    onSlideVisibilityChanged(i);
                    break;
                }
            }
        });
    }

    Q_EMIT slideModelChanged();

    return m_slides.size() - 1;
}

struct SlidesModel::RemovalStep {
    // A removed slide is kept, without its layers, so undo puts back the same slide that
    // layer removals further down the undo stack point at.
    struct Slide {
        int row = -1;
        QSharedPointer<LayersModel> slide;
        QJsonObject data;
    };
    struct Layer {
        QPointer<LayersModel> slide;
        RemovedLayer removed;
    };
    // In removal order, highest row first.
    QVector<Slide> slides;
    QVector<Layer> layers;
};

namespace {
// Removal steps that can be undone. Older steps are dropped.
constexpr int kMaxRemovalUndoSteps = 50;

// Rows highest first, so removing one row does not shift the rows still to be removed.
QList<int> descendingUniqueRows(QList<int> rows) {
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    return rows;
}
}

void SlidesModel::removeSlide(int i) {
    removeSlides({i});
}

bool SlidesModel::removeSlides(const QList<int> &rows) {
    auto step = std::make_shared<RemovalStep>();
    {
        std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
        for (const int i : descendingUniqueRows(rows)) {
            if(i < 0 || i >= m_slides.size() || (m_slides[i] && m_slides[i]->getLayersCanBeLocked()))
                continue;

            RemovalStep::Slide removed;
            removed.row = i;
            removed.slide = m_slides[i];
            if (removed.slide) {
                // No relative paths, so the snapshot does not depend on where the presentation is saved.
                removed.slide->encodeToJSON(removed.data, QStringList());
                // Brought back hidden; it is no longer the triggered slide.
                removed.data.insert(QStringLiteral("visibility"), 0);
            }

            beginRemoveRows(QModelIndex(), i, i);
            m_slides.removeAt(i);

            if (m_triggeredSlideIdx == i)
                m_triggeredSlideIdx = -1;
            else if (m_triggeredSlideIdx > i)
                m_triggeredSlideIdx -= 1;

            if (m_previousTriggeredSlideIdx == i)
                m_previousTriggeredSlideIdx = -2;
            else if (m_previousTriggeredSlideIdx > i)
                m_previousTriggeredSlideIdx -= 1;

            if (m_previousSelectedSlideIdx == i)
                m_previousSelectedSlideIdx = -2;
            else if (m_previousSelectedSlideIdx > i)
                m_previousSelectedSlideIdx -= 1;

            if (m_selectedSlideIdx == i)
                m_selectedSlideIdx = -1;

            endRemoveRows();
            step->slides.append(removed);
        }

        if (step->slides.isEmpty())
            return false;

        setSlidesNeedsSave(true);
        setNeedSync();
    }

    // Release the media of the removed slides now, as before undo existed; undo decodes them again.
    for (const auto &removed : std::as_const(step->slides)) {
        if (removed.slide) {
            removed.slide->clearLayers();
            removed.slide->setReferencedLayers({});
        }
    }
    pushRemovalStep(step);

    if (m_slides.isEmpty()) {
        setSelectedSlideIdx(-1);
    }

    Q_EMIT slideModelChanged();
    return true;
}

bool SlidesModel::removeLayers(LayersModel *slide, const QList<int> &rows) {
    if (!slide)
        return false;

    auto step = std::make_shared<RemovalStep>();
    for (const int row : descendingUniqueRows(rows)) {
        RemovedLayer removed = slide->takeLayer(row);
        if (removed.isValid())
            step->layers.append({QPointer<LayersModel>(slide), removed});
    }
    return pushRemovalStep(step);
}

bool SlidesModel::undoRemove() {
    while (!m_removalUndo.isEmpty()) {
        const std::shared_ptr<RemovalStep> step = m_removalUndo.takeLast();
        bool restored = false;

        // Rows were removed highest first, so they are put back lowest first.
        for (auto it = step->slides.crbegin(); it != step->slides.crend(); ++it) {
            if (!it->slide)
                continue;
            {
                std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
                const int row = std::clamp(it->row, 0, static_cast<int>(m_slides.size()));
                beginInsertRows(QModelIndex(), row, row);
                m_slides.insert(row, it->slide);
                // Slide indices at or after the row move down one (negative values are "none" markers).
                for (int *idx : {&m_selectedSlideIdx, &m_previousSelectedSlideIdx, &m_triggeredSlideIdx, &m_previousTriggeredSlideIdx}) {
                    if (*idx >= row)
                        *idx += 1;
                }
                endInsertRows();
            }
            QJsonObject data = it->data;
            it->slide->decodeFromJSON(data, QStringList());
            restored = true;
        }

        for (auto it = step->layers.crbegin(); it != step->layers.crend(); ++it) {
            if (it->slide && it->slide->restoreLayer(it->removed)) {
                restored = true;
                if (it->slide == selectedSlide())
                    updateSelectedSlide();
            }
        }

        if (!restored)
            continue; // Everything of this step is gone, try the one before.

        setSlidesNeedsSave(true);
        setNeedSync();
        Q_EMIT slideModelChanged();
        return true;
    }
    return false;
}

bool SlidesModel::pushRemovalStep(const std::shared_ptr<RemovalStep> &step) {
    if (!step || (step->slides.isEmpty() && step->layers.isEmpty()))
        return false;
    m_removalUndo.append(step);
    while (m_removalUndo.size() > kMaxRemovalUndoSteps)
        m_removalUndo.removeFirst();
    return true;
}

void SlidesModel::clearRemovalUndo() {
    m_removalUndo.clear();
}

void SlidesModel::moveSlide(int i, int t) {
    {
        std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
        if (i < 0 || t < 0 || i >= m_slides.size() || t >= m_slides.size() || i == t) {
            Q_EMIT dataChanged(index(i, 0), index(t, 0));
            Q_EMIT slideModelChanged();
            return;
        }

        int destinationRow = (t > i) ? (t + 1) : t;

        if (!beginMoveRows(QModelIndex(), i, i, QModelIndex(), destinationRow)) {
            Q_EMIT dataChanged(index(i, 0), index(t, 0));
            Q_EMIT slideModelChanged();
            return;
        }

        m_slides.move(i, t);
        m_previousSelectedSlideIdx = t;

        // Rows between i and t shift by one towards i.
        auto remap = [i, t](int idx) {
            if (idx == i)
                return t;
            if (i < t && idx > i && idx <= t)
                return idx - 1;
            if (t < i && idx >= t && idx < i)
                return idx + 1;
            return idx;
        };
        m_triggeredSlideIdx = remap(m_triggeredSlideIdx);
        m_previousTriggeredSlideIdx = remap(m_previousTriggeredSlideIdx);

        endMoveRows();

        Q_EMIT dataChanged(index(t, 0), index(t, 0));
        setSlidesNeedsSave(true);
        setNeedSync();
    }

    Q_EMIT slideModelChanged();
}

void SlidesModel::moveSlideUp(int i) {
    {
        std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
        if (i < 1 || i >= m_slides.size())
            return;
        if (!beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1))
            return;
        m_slides.move(i, i - 1);
        m_previousSelectedSlideIdx = i - 1;

        if (m_triggeredSlideIdx == i)
            m_triggeredSlideIdx = i - 1;
        else if (m_triggeredSlideIdx == i-1)
            m_triggeredSlideIdx = i;

        if (m_previousTriggeredSlideIdx == i)
            m_previousTriggeredSlideIdx = i - 1;
        else if (m_previousTriggeredSlideIdx == i - 1)
            m_previousTriggeredSlideIdx = i;

        endMoveRows();
        setSlidesNeedsSave(true);
        setNeedSync();
    }

    Q_EMIT slideModelChanged();
}

void SlidesModel::moveSlideDown(int i) {
    {
        std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
        if (i < 0 || i >= (m_slides.size() - 1))
            return;
        if (!beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i))
            return;
        m_slides.move(i, i + 1);
        m_previousSelectedSlideIdx = i + 1;

        if (m_triggeredSlideIdx == i)
            m_triggeredSlideIdx = i + 1;
        else if (m_triggeredSlideIdx == i + 1)
            m_triggeredSlideIdx = i;

        if (m_previousTriggeredSlideIdx == i)
            m_previousTriggeredSlideIdx = i + 1;
        else if (m_previousTriggeredSlideIdx == i + 1)
            m_previousTriggeredSlideIdx = i;

        endMoveRows();
        setSlidesNeedsSave(true);
        setNeedSync();
    }

    Q_EMIT slideModelChanged();
}

void SlidesModel::updateSlide(int i) {
    if (i >= 0 && i < m_slides.size()) {
        // Called from the render thread too (layer status changes inside
        // runRenderOnLayersThatShouldUpdate), and emitting model signals into QML
        // from there is not allowed. Bounce it onto the thread the model lives on.
        if (QThread::currentThread() != thread()) {
            QMetaObject::invokeMethod(this, [this, i]() { updateSlide(i); }, Qt::QueuedConnection);
            return;
        }
        Q_EMIT dataChanged(index(i, 0), index(i, 0));
    }
}

void SlidesModel::updateSelectedSlide() {
    if (m_selectedSlideIdx >= 0 && m_selectedSlideIdx < m_slides.size())
        updateSlide(m_selectedSlideIdx);
}

void SlidesModel::clearSlides() {
    clearRemovalUndo();
    {
        std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
        //Move all locked slides above unlocked ones
        //Remove unlocked slides
        int unlockedSlides = 0;
        int lockedSlides = 0;
        for (int i = 0; i < m_slides.size(); i++) {
            if (m_slides[i] && m_slides[i]->getLayersCanBeLocked()) {
                if (unlockedSlides > 0) {
                    if (beginMoveRows(QModelIndex(), i, i, QModelIndex(), lockedSlides)) {
                        m_slides.move(i, lockedSlides);
                        endMoveRows();
                    }
                }
                lockedSlides++;
            }
            else {
                unlockedSlides++;
            }
        }
        if (lockedSlides < m_slides.size()) {
            beginRemoveRows(QModelIndex(), lockedSlides, m_slides.size() - 1);
            for (int i = 0; i < lockedSlides; i++) {
                m_slides[i]->clearLayers();
                updateSlide(i);
            }
            m_slides.erase(m_slides.begin() + lockedSlides, m_slides.end());
            endRemoveRows();
        }
        else {
            for (int i = 0; i < lockedSlides; i++) {
                m_slides[i]->clearLayers();
                updateSlide(i);
            }
        }
        m_masterSlide->clearLayers();
        setSlidesName(QStringLiteral(""));
        setSlidesPath(QStringLiteral(""));
        setSlidesNeedsSave(false);
        setNeedSync();

        m_selectedSlideIdx = -1;
        m_previousSelectedSlideIdx = -1;
        m_triggeredSlideIdx = -1;
        m_previousTriggeredSlideIdx = -1;
    }

    Q_EMIT selectedSlideChanged();
    Q_EMIT triggeredSlideChanged();
    Q_EMIT slideModelChanged();
}

void SlidesModel::clearAllForShutdown() {
    std::lock_guard<std::recursive_mutex> lock(m_slidesMutex);
    clearRemovalUndo();

    if (m_masterSlide)
        m_masterSlide->clearLayers();

    for (auto& slide : m_slides) {
        if (slide)
            slide->clearLayers();
    }

    m_layerToCopyFrom.reset();
}

void SlidesModel::slideContentChanged() {
    Q_EMIT slideModelChanged();
}

void SlidesModel::copyLayer() {
    LayersModel* src = selectedSlide();
    m_layerToCopyFrom = src ? src->getLayerToCopy() : std::shared_ptr<BaseLayer>();
    m_clearCopyTimer->start();
}

void SlidesModel::clearCopyLayer() {
    m_layerToCopyFrom.reset();
    Q_EMIT copyCleared();
}

bool SlidesModel::copyIsAvailable() {
    return !m_layerToCopyFrom.expired();
}

void SlidesModel::pasteLayer() {
    std::shared_ptr<BaseLayer> src = m_layerToCopyFrom.lock();
    if (!src)
        return;
    LayersModel* slideToPaste = slide(getSlideToPasteIdx());
    if (slideToPaste) {
        slideToPaste->addCopyOfLayer(src.get());
        updateSlide(getSlideToPasteIdx());
        m_clearCopyTimer->start();
    }
}

void SlidesModel::pasteLayerAsProperties(int layerIdx) {
    std::shared_ptr<BaseLayer> src = m_layerToCopyFrom.lock();
    if (!src || !selectedSlide())
        return;
    selectedSlide()->overwriteLayerProperties(src.get(), layerIdx);
    m_clearCopyTimer->start();
}

void SlidesModel::setSlideToPasteIdx(int value) {
    m_slideToPasteIdx = value;
}

int SlidesModel::getSlideToPasteIdx() {
    return m_slideToPasteIdx;
}

void SlidesModel::setSlidesNeedsSave(bool value) {
    m_slidesNeedsSave = value;
    Q_EMIT slidesNeedsSaveChanged();
}

bool SlidesModel::getSlidesNeedsSave() {
    return m_slidesNeedsSave;
}

void SlidesModel::setLayersNeedsSave(bool value) {
    if (value)
        setSlidesNeedsSave(true);
}

void SlidesModel::setSlidesName(QString name) {
    m_slidesName = name;
    Q_EMIT slidesNameChanged();
}

QString SlidesModel::getSlidesName() const {
    return m_slidesName;
}

void SlidesModel::setSlidesPath(QString path) {
    m_slidesPath = path;
}

QString SlidesModel::getSlidesPath() const {
    return m_slidesPath;
}

QUrl SlidesModel::getSlidesPathAsURL() const {
    return QUrl(QStringLiteral("file:///") + m_slidesPath);
}

std::string SlidesModel::getSlidesAsFormattedString(size_t charsPerItem) const {
    std::string fullItemList = "";
    for (int i = 0; i < m_slides.size(); i++) {
        std::string title = std::to_string(i + 1) + ". ";

        if (m_slides[i])
            title += m_slides[i]->getLayersName().toStdString();

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
        if (i < m_slides.size() - 1)
            fullItemList += "\n";
    }
    return fullItemList;
}

QString SlidesModel::makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider) {
    // Assuming filePath is absolute
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            return foundDir.relativeFilePath(filePath);
        }
    }
    return filePath;
}

void SlidesModel::loadFromJSONFile(const QString &path) {
    // Removals from the presentation loaded before cannot be put back into this one.
    clearRemovalUndo();
    QString fileToOpen = path;
    fileToOpen.replace(QStringLiteral("file:///"), QStringLiteral(""));

    QStringList pathsToConsider;
    pathsToConsider.append(LocationSettings::cPlayMediaLocation());
    pathsToConsider.append(LocationSettings::cPlayFileLocation());
    pathsToConsider.append(LocationSettings::univiewVideoLocation());

    fileToOpen = m_masterSlide->checkAndCorrectPath(fileToOpen, pathsToConsider);

    QFileInfo jsonFileInfo(fileToOpen);
    if (!jsonFileInfo.exists()) {
        qDebug() << QStringLiteral("C-play presentation ") << fileToOpen << QStringLiteral(" did not exist.");
        return;
    }

    QFile f(fileToOpen);
    if (!f.open(QIODevice::ReadOnly)) {
        qDebug() << QStringLiteral("Failed to open C-play presentation: ") << fileToOpen;
        return;
    }
    QByteArray fileContent = f.readAll();
    f.close();  

    QJsonDocument doc = QJsonDocument::fromJson(fileContent);
    if (doc.isNull()) {
        qDebug() << QStringLiteral("Parsing C-play presentation failed: ") << fileToOpen;
        return;
    }

    QJsonObject obj = doc.object();

    QStringList fileSearchPaths;
    fileSearchPaths.append(jsonFileInfo.absoluteDir().absolutePath());
    fileSearchPaths.append(LocationSettings::cPlayMediaLocation());
    fileSearchPaths.append(LocationSettings::cPlayFileLocation());
    fileSearchPaths.append(LocationSettings::univiewVideoLocation());

    if (obj.contains(QStringLiteral("master"))) {
        QJsonValue value = obj.value(QStringLiteral("master"));
        QJsonObject o = value.toObject();
        m_masterSlide->decodeFromJSON(o, fileSearchPaths);
        m_masterSlide->setLayersName(QStringLiteral("Master"));
    }

    bool slideVisible = false;
    if (obj.contains(QStringLiteral("slides"))) {
        QJsonValue value = obj.value(QStringLiteral("slides"));
        QJsonArray array = value.toArray();
        for (auto v : array) {
            QJsonObject o = v.toObject();
            int idx = addSlide();
            m_slides[idx]->decodeFromJSON(o, fileSearchPaths);
            if (!slideVisible && m_slides[idx]->getLayersVisibility() > 0) {
                slideVisible = true;
                m_previousTriggeredSlideIdx = idx - 1;
                m_selectedSlideIdx = idx;
                m_triggeredSlideIdx = idx;
            }
        }
    }

    if (!slideVisible)
        m_selectedSlideIdx = -1;

    // Always preload the first slide after loading a presentation,
    // even if preLoadLayers is not enabled.
    if (!m_slides.empty()) {
        for (auto layer : m_slides[0]->getLayers()) {
            if (layer.first)
                layer.first->setShouldPreLoad(true);
        }
    }

    updateRecentLoadedPresentations(jsonFileInfo.absoluteFilePath());
    setSlidesPath(jsonFileInfo.absoluteFilePath());
    setSlidesName(jsonFileInfo.baseName());
    setSlidesNeedsSave(false);
    setNeedSync();

    Q_EMIT presentationHasLoaded();
}

QJsonDocument SlidesModel::asJSONDocument(const QStringList &pathsToConsider) {
    QJsonDocument doc;
    QJsonObject obj = doc.object();

    QJsonObject masterSlideData;
    m_masterSlide->encodeToJSON(masterSlideData, pathsToConsider);
    obj.insert(QStringLiteral("master"), masterSlideData);

    QJsonArray slidesArray;
    for (auto slide : m_slides) {
        if (!slide)
            continue;
        QJsonObject slideData;
        slide->encodeToJSON(slideData, pathsToConsider);
        slidesArray.push_back(QJsonValue(slideData));
    }
    obj.insert(QString(QStringLiteral("slides")), QJsonValue(slidesArray));

    doc.setObject(obj);
    return doc;
}

void SlidesModel::saveAsJSONFile(const QString &path) {
    QString fileToSave = path;
    fileToSave.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileToSaveInfo(fileToSave);

    QStringList pathsToConsider;
    pathsToConsider.append(fileToSaveInfo.absoluteDir().absolutePath());
    pathsToConsider.append(LocationSettings::cPlayMediaLocation());
    pathsToConsider.append(LocationSettings::cPlayFileLocation());
    pathsToConsider.append(LocationSettings::univiewVideoLocation());

    QJsonDocument doc = asJSONDocument(pathsToConsider);
    QFile jsonFile(fileToSave);
    if(!jsonFile.open(QFile::WriteOnly)) {
        qDebug() << QStringLiteral("Failed to open C-play presentation for writing: ") << fileToSave;
        return;
    }
    jsonFile.write(doc.toJson());
    jsonFile.close();

    QFileInfo fileInfo(jsonFile);
    setSlidesName(fileInfo.baseName());
    setSlidesPath(fileToSave);
    updateRecentLoadedPresentations(fileToSave);

    setSlidesNeedsSave(false);
}

void SlidesModel::autosave() {
    // Saved or cleared since the edit that started the timer.
    if (!m_slidesNeedsSave)
        return;
    if (numberOfSlides() == 0 && m_masterSlide->getLayers().size() == 0)
        return;

    // Absolute paths, as the autosave is not in the folder of the presentation.
    const QString path = Autosave::filePathFor(m_slidesPath, m_slidesName, QStringLiteral("cplaypres"));
    Autosave::write(path, asJSONDocument({}));
}

void SlidesModel::runStartAfterPresentationLoad() {
    // Let's start all layers that have a visibility
    for (int i = -1; i < numberOfSlides(); i++) {
        const Layers& slideLayers = slide(i)->getLayers();
        for (auto layer : slideLayers) {
            if (layer.first && layer.first->alpha() > 0.f) {
                // requestStart(): a layer whose load is still in progress (e.g. a slow
                // stream) records the request and starts as soon as it becomes ready,
                // instead of the play signal being dropped because start() requires
                // ready() right now.
                layer.first->requestStart();
            }

        }
    }
    setNeedSync();
    setSlidesNeedsSave(false);
}

void SlidesModel::runUpdateAudioOutputOnLayers() {
    for (int i = -1; i < numberOfSlides(); i++) {
        const Layers& slideLayers = slide(i)->getLayers();
        for (auto layer : slideLayers) {
            if (layer.first)
                layer.first->updateAudioOutput();
        }
    }
}

void SlidesModel::runUpdateVolumeOnLayers(int volume) {
    m_volumeScaling = static_cast<float>(volume) / 100.f;
    for (int i = -1; i < numberOfSlides(); i++) {
        const Layers& slideLayers = slide(i)->getLayers();
        for (auto layer : slideLayers) {
            if (!layer.first)
                continue;
            layer.first->setVolumeScaling(m_volumeScaling);
            float volLevelF = static_cast<float>(layer.first->volume()) * m_volumeScaling;
            layer.first->setVolume(static_cast<int>(volLevelF), false);
        }
    }
}

void SlidesModel::runUpdateMuteOnLayers(bool mute) {
    for (int i = -1; i < numberOfSlides(); i++) {
        const Layers& slideLayers = slide(i)->getLayers();
        for (auto layer : slideLayers) {
            if (!layer.first)
                continue;
            layer.first->setVolumeMute(mute);
        }
    }
}

void SlidesModel::checkMasterLayersRunBasedOnMediaVisibility(int mediaVisibility) {
    // Start/stop master layers that are visible dependent on media visibility
    const Layers& slideLayers = masterSlide()->getLayers();
    for (int i = 0; i < slideLayers.size(); i++) {
        if (!slideLayers[i].first)
            continue;
        // Front layers are rendered above the media, so media visibility must not affect them.
        if (slideLayers[i].first->hierarchy() == BaseLayer::LayerHierarchy::FRONT)
            continue;
        if (PresentationSettings::mediaVisibilityControlMasterLayers()) {
            slideLayers[i].first->setAlpha(static_cast<float>(100 - mediaVisibility) / 100.f);
            masterSlide()->updateLayer(i);
        }
        else {
            if (slideLayers[i].first->alpha() > 0.f) {
                if (mediaVisibility < 100 && slideLayers[i].first->pause()) {
                    slideLayers[i].first->requestStart();
                }
                else if (mediaVisibility == 100 && !slideLayers[i].first->pause()) {
                    slideLayers[i].first->stop();
                }
            }
        }
    }
}

bool SlidesModel::pauseLayerUpdate() {
    return m_pauseLayerUpdate;
}
void SlidesModel::setPauseLayerUpdate(bool value) {
    m_pauseLayerUpdate = value;
    Q_EMIT pauseLayerUpdateChanged();
}

QStringList SlidesModel::recentPresentations() const {
    return PresentationSettings::recentLoadedPresentations();
}

void SlidesModel::setRecentPresentations(QStringList list) {
    PresentationSettings::setRecentLoadedPresentations(list);
    PresentationSettings::self()->save();
    Q_EMIT recentPresentationsChanged();
}

void SlidesModel::clearRecentPresentations() {
    QStringList empty;
    PresentationSettings::setRecentLoadedPresentations(empty);
    Q_EMIT recentPresentationsChanged();
    PresentationSettings::self()->save();
}

void SlidesModel::runRenderOnLayersThatShouldUpdate(bool updateRendering) {
#ifdef CAPTURE_SUPPORT
    // Captures of layers deleted on the GUI thread are closed here, on the render thread that owns them.
    CaptureLayer::processPendingGLCleanup();
#endif

    if (pauseLayerUpdate()) {
        return;
    }

    const bool preload = preLoadLayers();

    // Master slide: m_masterSlide is a stable member, never removed from the model
    // while the app runs, so it is safe to use directly from the render thread.
    if (m_masterSlide) {
        m_masterSlide->runRenderOnLayersThatShouldUpdate(updateRendering, preload);
    }

    // The slides themselves must not be reached through slide(i)/numberOfSlides():
    // those read m_slides without m_slidesMutex and return a raw pointer. At startup
    // (clearSlides() + addSlide() from a presentation load, which the layers panel
    // opening triggers) the GUI thread erases and reallocs m_slides underneath the
    // render thread, destroying the LayersModel - and with it its m_layers container
    // and mutex - while this loop is iterating it. That produced use-after-free
    // crashes on a garbage BaseLayer vtable inside runRenderOnLayersThatShouldUpdate.
    // Take a shared-pointer snapshot instead: it keeps every LayersModel alive for
    // the duration of the loop even if the GUI thread removes it in the meantime,
    // and the per-model m_layersMutex then does its job again.
    QList<QSharedPointer<LayersModel>> slides;
    if (!trySnapshotSlides(slides)) {
        // GUI thread is mid-modification of the slide list: skip this frame.
        return;
    }
    for (int i = 0; i < slides.size(); i++) {
        if (pauseLayerUpdate()) {
            break;
        }
        if (slides[i] && slides[i]->runRenderOnLayersThatShouldUpdate(updateRendering, preload)) {
            updateSlide(i);
        }
    }
}

void SlidesModel::setNeedSync() {
    m_needSync = true;
    m_syncIteration = PresentationSettings::networkSyncIterations();
}

void SlidesModel::updateRecentLoadedPresentations(QString path) {
    // Autosaves are listed on their own.
    if (Autosave::isAutosave(path))
        return;
    QStringList recentPresentations = PresentationSettings::recentLoadedPresentations();
    recentPresentations.push_front(path);
    recentPresentations.removeDuplicates();
    if (recentPresentations.size() > 8)
        recentPresentations.pop_back();
    PresentationSettings::setRecentLoadedPresentations(recentPresentations);
    Q_EMIT recentPresentationsChanged();
    PresentationSettings::self()->save();
}

void SlidesModel::onSlideVisibilityChanged(int slideIdx) {
    if (slideIdx != m_triggeredSlideIdx)
        return;
    if (!slideHasTimelineKeyframes(slideIdx))
        return;

    int value = slide(slideIdx)->getLayersVisibility();
    float valF = static_cast<float>(value) * 0.01f;

    std::vector<std::pair<BaseLayer*, int>> layers = m_visibilityModel->getLayersVisibility(slideIdx);
    for (int i = 0; i < static_cast<int>(layers.size()); i++) {
        if (!layers[i].first)
            continue;
        int localIdx = layers[i].second;

        if (localIdx == 0) {
            // Layers animated by keyframes or entrance effects get their alpha from
            // the timeline; the rest follow the slide visibility envelope.
            if (slide(slideIdx)->isLayerTimelineDriven(layers[i].first))
                continue;
            layers[i].first->setAlpha(valF);
            continue;
        }
        else if (localIdx < 0) {  // Upcoming
            layers[i].first->setAlpha(0.f);
            if (-localIdx <= PresentationSettings::updateUpcomingSlideCount()) {
                layers[i].first->setShouldPreLoad(true);
            }
            continue;
        }

        localIdx -= layers[i].first->keepVisibilityForNumSlides();
        if (localIdx <= 0) {
            // Kept from an earlier slide; animated by this slide's timeline when it has a track.
            if (!slide(slideIdx)->isLayerTimelineDriven(layers[i].first))
                layers[i].first->setAlpha(1.f); // 100%
            continue;
        }
        else if (localIdx == 1) {
            if (layers[i].first->alpha() > 0.f)
                layers[i].first->setAlpha(1.f - valF); // 100%->0%
            continue;
        }
        else {
            layers[i].first->setAlpha(0.f); // 0%
            layers[i].first->setShouldUpdate(false);
            continue;
        }
    }

    if (m_previousTriggeredSlideIdx != m_triggeredSlideIdx
        && m_previousTriggeredSlideIdx >= 0 && m_previousTriggeredSlideIdx < m_slides.size()
        && slide(m_previousTriggeredSlideIdx)->getLayersVisibility() > 0
        && !slideHasTimelineKeyframes(m_previousTriggeredSlideIdx)) {
        slide(m_previousTriggeredSlideIdx)->setLayersVisibility(100 - value, false);
        updateSlide(m_previousTriggeredSlideIdx);
    }
}

void SlidesModel::startTimeline(int slideIdx) {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return;
    m_slides[slideIdx]->startTimeline();
}

void SlidesModel::startTimelineFrom(int slideIdx, int startMs) {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return;
    m_slides[slideIdx]->startTimelineFrom(startMs);
}

void SlidesModel::startTimelineReverse(int slideIdx) {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return;
    m_slides[slideIdx]->startTimelineReverse();
}

void SlidesModel::startTimelineIntro(int slideIdx) {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return;
    m_slides[slideIdx]->startTimelineIntro();
}

void SlidesModel::startTimelineOutro(int slideIdx, int fromOutroMs) {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return;
    m_slides[slideIdx]->startTimelineOutro(fromOutroMs);
}

void SlidesModel::stopTimeline(int slideIdx) {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return;
    m_slides[slideIdx]->stopTimeline();
}

void SlidesModel::stopOutro(int slideIdx) {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return;
    m_slides[slideIdx]->stopOutro();
}

void SlidesModel::stopAllTimelines() {
    for (int i = 0; i < m_slides.size(); ++i) {
        if (m_slides[i])
            m_slides[i]->stopAllTimelines();
    }
}

bool SlidesModel::timelineRunning(int slideIdx) const {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return false;
    return m_slides.at(slideIdx)->timelineRunning();
}

bool SlidesModel::timelineReversed(int slideIdx) const {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return false;
    return m_slides.at(slideIdx)->timelineReversed();
}

int SlidesModel::timelineSlideIdx() const {
    // For backward compatibility: return first slide with running timeline
    for (int i = 0; i < m_slides.size(); ++i) {
        if (m_slides.at(i)->timelineRunning())
            return i;
    }
    return -1;
}

int SlidesModel::timelinePositionMs(int slideIdx) const {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return 0;
    return m_slides.at(slideIdx)->timelinePositionMs();
}

bool SlidesModel::outroRunning(int slideIdx) const {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return false;
    return m_slides.at(slideIdx)->outroRunning();
}

bool SlidesModel::slideHasTimelineKeyframes(int slideIdx) const {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return false;
    return m_slides.at(slideIdx)->hasTimelineKeyframes();
}

void SlidesModel::jumpToIntroStart(int slideIdx) {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return;
    m_slides[slideIdx]->jumpToIntroStart();
    updateSlide(slideIdx);
}

void SlidesModel::jumpToOutroStart(int slideIdx) {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return;
    m_slides[slideIdx]->jumpToOutroStart();
    updateSlide(slideIdx);
}

bool SlidesModel::nextAnimationStep() {
    if (m_triggeredSlideIdx < 0 || m_triggeredSlideIdx >= m_slides.size() || !m_slides[m_triggeredSlideIdx])
        return false;
    return m_slides[m_triggeredSlideIdx]->playNextAnimationStep();
}

bool SlidesModel::previousAnimationStep() {
    if (m_triggeredSlideIdx < 0 || m_triggeredSlideIdx >= m_slides.size() || !m_slides[m_triggeredSlideIdx])
        return false;
    return m_slides[m_triggeredSlideIdx]->playPreviousAnimationStep();
}

bool SlidesModel::hasPendingAnimationStep() const {
    if (m_triggeredSlideIdx < 0 || m_triggeredSlideIdx >= m_slides.size() || !m_slides[m_triggeredSlideIdx])
        return false;
    return m_slides[m_triggeredSlideIdx]->hasPendingAnimationStep();
}

int SlidesModel::slideTimelineVisibility(int slideIdx) const {
    if (slideIdx < 0 || slideIdx >= m_slides.size())
        return -1;
    return m_slides.at(slideIdx)->timelineVisibility();
}