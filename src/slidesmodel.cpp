/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "slidesmodel.h"
#include "layersmodel.h"
#include "locationsettings.h"
#include "presentationsettings.h"
#include "layers/baselayer.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

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
    if (parent.isValid())
        return 0;

    //Column = Slides
    return static_cast<int>(m_slideList->size());
}

QVariant SlideVisibilityModel::data(const QModelIndex& index, int role) const {
    //Rows = Layers
    //Column = Slides

    switch (role) {
    case DisplayRole:
        return QString(QStringLiteral("%1")).arg(calculateLocalIndex(index.column(), index.row()));
    case ColorRole:
        return cellColor(index.column(), index.row());
    case TextRole:
        return cellText(index.column(), index.row());
    default:
        break;
    }

    return QVariant();
}

QVariant SlideVisibilityModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole) {
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
    return roles;
}

std::vector<std::pair<BaseLayer*, int>> SlideVisibilityModel::getLayersVisibility(int slideIdx) {
    std::vector<std::pair<BaseLayer*, int>> layersOperationsInSlide;

    int columns = static_cast<int>(m_slideList->size());
    for (int row = 0; row < static_cast<int>(m_visibilityLayers.size()); row++) {
        int keepIdx = (row * columns) + slideIdx;
        layersOperationsInSlide.push_back(std::make_pair(m_visibilityLayers[row], m_keepVisibilityMatrix[keepIdx]));
    }

    return layersOperationsInSlide;
}

void SlideVisibilityModel::cellClicked(int column, int row) {
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
        }
    }
}

void SlideVisibilityModel::resetTable() {
    beginResetModel();

    // Calculate the table/matrix for visibility of layers across slides
    std::vector<int> newKeepVisibilityMatrix;

    // Below vector contains all layers, in order from first to last slide.
    std::vector<BaseLayer*> newVisibilityLayers;

    // Negative number if column number is before the layer owner (the slide it is own by)
    // 0 if the column number equals the layer owner
    // Positive number for large then the layer owner
    
    //Column = Slides
    int columns = m_slideList->size();

    //Rows = Layers
    for (auto s : *m_slideList) {
        const Layers& slideLayers = s->getLayers();
        for (auto layer : slideLayers) {
            newVisibilityLayers.push_back(layer.first.get());
        }
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

    endResetModel();
}

BaseLayer* SlideVisibilityModel::findLayer(int row) {
    int layerNum = 0;
    for (auto s : *m_slideList) {
        const Layers& slideLayers = s->getLayers();
        for (auto layer : slideLayers) {
            if (row == layerNum) {
                return layer.first.get();
            }
            layerNum++;
        }
    }
    return nullptr;
}

int SlideVisibilityModel::calculateLocalIndex(int column, int row, bool ignoreKeepValue) const {
    // Negative number if column number is before the layer owner (the slide it is own by)
    // 0 if the column number equals the layer owner
    // Positive number for large then the layer owner

    int layerNum = 0;
    int slideNum = 0;
    for (auto s : *m_slideList) {
        const Layers& slideLayers = s->getLayers();
        for (auto layer : slideLayers) {
            if (row == layerNum) {
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
    // First zero, where visibility starts = green
    // Other zeros, still visible = white
    // One, where layer fade = red
    // Positive value above one = black

    int keepIdx = (row * m_slideList->size()) + column;

    if(keepIdx >= m_keepVisibilityMatrix.size() || row >= m_visibilityLayers.size())
        return QStringLiteral("grey");

    int val = m_keepVisibilityMatrix[keepIdx];

    if (val == 0) {
        return QStringLiteral("green");
    }
    else if (val < 0) {
        return QStringLiteral("grey");
    }

    val -= m_visibilityLayers[row]->keepVisibilityForNumSlides();
    if (val <= 0) {
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
    int keepIdx = (row * m_slideList->size()) + column;

    if (keepIdx >= m_keepVisibilityMatrix.size() || row >= m_visibilityLayers.size())
        return QStringLiteral("");

    int val = m_keepVisibilityMatrix[keepIdx];

    if (val == 0) {
        return QStringLiteral("0% -> 100%");
    }
    else if (val < 0) {
        return QStringLiteral("");
    }

    val -= m_visibilityLayers[row]->keepVisibilityForNumSlides();
    if (val <= 0) {
        return QStringLiteral("100%");
    }
    else if (val == 1) {
        return QStringLiteral("100% -> 0%");
    }
    else {
        return QStringLiteral("");
    }
}

SlidesModel::SlidesModel(QObject *parent)
    : QAbstractListModel(parent),
    m_masterSlide(new LayersModel(this)),
    m_visibilityModel(new SlideVisibilityModel(&m_slides, this)),
    m_needSync(false),
    m_syncIteration(0),
    m_slideFadeTime(0),
    m_slidesName(QStringLiteral("")),
    m_slidesPath(QStringLiteral("")) {
    m_masterSlide->setLayersName(QStringLiteral("Master"));
    m_masterSlide->setHierarchy(BaseLayer::LayerHierarchy::BACK);
    m_masterSlide->setLayersCanBeLocked(true);
    m_clearCopyTimer = new QTimer(this);
    m_clearCopyTimer->setInterval(30000);
    m_clearCopyTimer->setSingleShot(true);
    connect(m_clearCopyTimer, &QTimer::timeout, this, &SlidesModel::clearCopyLayer);
    connect(this, &SlidesModel::slideModelChanged, m_visibilityModel, &SlideVisibilityModel::resetTable);
}

SlidesModel::~SlidesModel() {
    m_slides.clear();
    delete m_masterSlide;
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

    auto slideItem = m_slides.at(index.row());

    switch (role) {
    case NameRole:
        return QVariant(slideItem->getLayersName());
    case PathRole:
        return QVariant(slideItem->getLayersPath());
    case LayersRole:
        return QVariant(slideItem->numberOfLayers());
    case LayerMinStatusRole:
        return QVariant(slideItem->minLayerStatus());
    case LayerMaxStatusRole:
        return QVariant(slideItem->maxLayerStatus());
    case LockedRole:
        return QVariant(slideItem->getLayersCanBeLocked());
    case LockedCountRole:
        return QVariant(slideItem->getLockedLayerCount());
    case VisibilityRole:
        return QVariant(slideItem->getLayersVisibility());
    }

    return QVariant();
}

QHash<int, QByteArray> SlidesModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[NameRole] = "name";
    roles[PathRole] = "filepath";
    roles[LayersRole] = "layers";
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
            layer.first->setShouldPreLoad(true);
        }
    }
    else if (m_selectedSlideIdx == -1) {
        for (auto layer : m_masterSlide->getLayers()) {
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
                layers[i].first->setAlpha(1.f); // 100%
                continue;
            }
            else if (localIdx == 1) {
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
    }

    // Need to update UI for all layers that changed
    // Let's just update selected, triggered and previous slide
    if (m_previousTriggeredSlideIdx != m_triggeredSlideIdx 
        && m_previousTriggeredSlideIdx >= 0 && m_previousTriggeredSlideIdx < m_slides.size()) {
        slide(m_previousTriggeredSlideIdx)->setLayersVisibility(100 - value, false);
        updateSlide(m_previousTriggeredSlideIdx);
    }
    if (m_selectedSlideIdx != m_triggeredSlideIdx && m_selectedSlideIdx != m_previousTriggeredSlideIdx
        && m_selectedSlideIdx >= 0 && m_selectedSlideIdx < m_slides.size()) {
        slide(m_selectedSlideIdx)->setLayersVisibility(100 - value, false);
        updateSlide(m_selectedSlideIdx);
    }

    setSlidesNeedsSave(true);

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
    else if (i >= 0 && m_slides.size() > i)
        return m_slides[i].get();
    else
        return nullptr;
}

LayersModel* SlidesModel::slide(std::string name) {
    // Returning first with correct name
    QString nameLowCase = QString::fromStdString(name).toLower();
    if (nameLowCase == QStringLiteral("master")) {
        return masterSlide();
    }
    for (int i = 0; i < m_slides.size(); i++) {
        if (m_slides[i]->getLayersName().toLower() == nameLowCase) {
            return slide(i);
        }
    }
    return nullptr;
}

LayersModel *SlidesModel::selectedSlide() {
    return slide(m_selectedSlideIdx);
}

SlideVisibilityModel* SlidesModel::visibilityModel() {
    return m_visibilityModel;
}

int SlidesModel::addSlide() {
    beginInsertRows(QModelIndex(), m_slides.size(), m_slides.size());
    m_slides.push_back(QSharedPointer<LayersModel>(new LayersModel(this)));
    setSlidesNeedsSave(true);
    setNeedSync();
    endInsertRows();

    connect(m_slides[m_slides.size() - 1].get(), &LayersModel::layersModelChanged, this, &SlidesModel::slideContentChanged);
    Q_EMIT slideModelChanged();

    return m_slides.size() - 1;
}

void SlidesModel::removeSlide(int i) {
    if(i < 0 || i >= m_slides.size() || m_slides[i]->getLayersCanBeLocked())
        return;

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

    if (m_slides.isEmpty()) {
        setSelectedSlideIdx(-1);
    }

    setSlidesNeedsSave(true);
    setNeedSync();

    Q_EMIT slideModelChanged();
}

void SlidesModel::moveSlideUp(int i) {
    if (i < 1)
        return;
    beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1);
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

    Q_EMIT slideModelChanged();
}

void SlidesModel::moveSlideDown(int i) {
    if (i < 0 || i == (m_slides.size() - 1))
        return;
    beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i);
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

    Q_EMIT slideModelChanged();
}

void SlidesModel::updateSlide(int i) {
    if (i >= 0 && i < m_slides.size()) {
        Q_EMIT dataChanged(index(i, 0), index(i, 0));
        setSlidesNeedsSave(true);
    }
}

void SlidesModel::updateSelectedSlide() {
    if (m_selectedSlideIdx >= 0 && m_selectedSlideIdx < m_slides.size())
        updateSlide(m_selectedSlideIdx);
}

void SlidesModel::clearSlides() {
    //Move all locked slides above unlocked ones
    //Remove unlocked slides
    int unlockedSlides = 0;
    int lockedSlides = 0;
    for (int i = 0; i < m_slides.size(); i++) {
        if (m_slides[i]->getLayersCanBeLocked()) {
            if (unlockedSlides > 0) {
                beginMoveRows(QModelIndex(), i, i, QModelIndex(), lockedSlides);
                m_slides.move(i, lockedSlides);
                endMoveRows();
            }
            lockedSlides++;
        }
        else {
            unlockedSlides++;
        }
    }
    beginRemoveRows(QModelIndex(), lockedSlides, m_slides.size() - 1);
    for (int i = 0; i < lockedSlides; i++) {
        m_slides[i]->clearLayers();
        updateSlide(i);
    }
    m_slides.erase(m_slides.begin() + lockedSlides, m_slides.end());
    m_masterSlide->clearLayers();
    endRemoveRows();

    setSlidesName(QStringLiteral(""));
    setSlidesPath(QStringLiteral(""));
    setSlidesNeedsSave(false);
    setNeedSync();

    Q_EMIT slideModelChanged();
}

void SlidesModel::slideContentChanged() {
    Q_EMIT slideModelChanged();
}

void SlidesModel::copyLayer() {
    m_layerToCopyFrom = selectedSlide()->getLayerToCopy();
    m_clearCopyTimer->start();
}

void SlidesModel::clearCopyLayer() {
    m_layerToCopyFrom = nullptr;
    Q_EMIT copyCleared();
}

bool SlidesModel::copyIsAvailable() {
    return (m_layerToCopyFrom != nullptr);
}

void SlidesModel::pasteLayer() {
    LayersModel* slideToPaste = slide(getSlideToPasteIdx());
    if (slideToPaste) {
        slideToPaste->addCopyOfLayer(m_layerToCopyFrom);
        updateSlide(getSlideToPasteIdx());
        m_clearCopyTimer->start();
    }
}

void SlidesModel::pasteLayerAsProperties(int layerIdx) {
    selectedSlide()->overwriteLayerProperties(m_layerToCopyFrom, layerIdx);
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

        title += m_slides[i]->getLayersName().toStdString();

        size_t countChars = title.size();
        if (countChars < charsPerItem) {
            title.insert(title.end(), charsPerItem - countChars, ' ');
        }
        else if (countChars >= charsPerItem) {
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
    f.open(QIODevice::ReadOnly);
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
        setSelectedSlideIdx(-1);
    else
        setSelectedSlideIdx(m_selectedSlideIdx);

    updateRecentLoadedPresentations(jsonFileInfo.absoluteFilePath());
    setSlidesPath(jsonFileInfo.absoluteFilePath());
    setSlidesName(jsonFileInfo.baseName());
    setSlidesNeedsSave(false);
    setNeedSync();

    Q_EMIT presentationHasLoaded();
}

void SlidesModel::saveAsJSONFile(const QString &path) {
    QJsonDocument doc;
    QJsonObject obj = doc.object();

    QString fileToSave = path;
    fileToSave.replace(QStringLiteral("file:///"), QStringLiteral(""));
    QFileInfo fileToSaveInfo(fileToSave);

    QStringList pathsToConsider;
    pathsToConsider.append(fileToSaveInfo.absoluteDir().absolutePath());
    pathsToConsider.append(LocationSettings::cPlayMediaLocation());
    pathsToConsider.append(LocationSettings::cPlayFileLocation());
    pathsToConsider.append(LocationSettings::univiewVideoLocation());

    QJsonObject masterSlideData;
    m_masterSlide->encodeToJSON(masterSlideData, pathsToConsider);
    obj.insert(QStringLiteral("master"), masterSlideData);

    QJsonArray slidesArray;
    for (auto slide : m_slides) {
        QJsonObject slideData;
        slide->encodeToJSON(slideData, pathsToConsider);
        slidesArray.push_back(QJsonValue(slideData));
    }
    obj.insert(QString(QStringLiteral("slides")), QJsonValue(slidesArray));

    doc.setObject(obj);
    QFile jsonFile(fileToSave);
    jsonFile.open(QFile::WriteOnly);
    jsonFile.write(doc.toJson());
    jsonFile.close();

    QFileInfo fileInfo(jsonFile);
    setSlidesName(fileInfo.baseName());
    setSlidesPath(fileToSave);
    updateRecentLoadedPresentations(fileToSave);

    setSlidesNeedsSave(false);
}

void SlidesModel::runStartAfterPresentationLoad() {
    // Let's start all layers that have a visibility
    for (int i = -1; i < numberOfSlides(); i++) {
        const Layers& slideLayers = slide(i)->getLayers();
        for (auto layer : slideLayers) {
            if (layer.first->alpha() > 0.f) {
                layer.first->start();
            }

        }
    }
    setNeedSync();
}

void SlidesModel::runUpdateAudioOutputOnLayers() {
    for (int i = -1; i < numberOfSlides(); i++) {
        const Layers& slideLayers = slide(i)->getLayers();
        for (auto layer : slideLayers) {
            layer.first->updateAudioOutput();
        }
    }
}

void SlidesModel::runUpdateVolumeOnLayers(int volume) {
    m_volumeScaling = static_cast<float>(volume) / 100.f;
    for (int i = -1; i < numberOfSlides(); i++) {
        const Layers& slideLayers = slide(i)->getLayers();
        for (auto layer : slideLayers) {
            layer.first->setVolumeScaling(m_volumeScaling);
            float volLevelF = static_cast<float>(layer.first->volume()) * m_volumeScaling;
            layer.first->setVolume(static_cast<int>(volLevelF), false);
        }
    }
}

void SlidesModel::checkMasterLayersRunBasedOnMediaVisibility(int mediaVisibility) {
    // Start/stop master layers that are visible dependent on media visibility
    const Layers& slideLayers = masterSlide()->getLayers();
    for (int i = 0; i < slideLayers.size(); i++) {
        if (PresentationSettings::mediaVisibilityControlMasterLayers()) {
            slideLayers[i].first->setAlpha(static_cast<float>(100 - mediaVisibility) / 100.f);
            masterSlide()->updateLayer(i);
        }
        else {
            if (slideLayers[i].first->alpha() > 0.f) {
                if (mediaVisibility < 100 && slideLayers[i].first->pause()) {
                    slideLayers[i].first->start();
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
    if (!pauseLayerUpdate()) {
        for (int i = -1; i < numberOfSlides(); i++) {
            if (pauseLayerUpdate()) {
                break;
            }
            if (slide(i)->runRenderOnLayersThatShouldUpdate(updateRendering, preLoadLayers())) {
                updateSlide(i);
            }
        }
    }
}

void SlidesModel::setNeedSync() {
    m_needSync = true;
    m_syncIteration = PresentationSettings::networkSyncIterations();
}

void SlidesModel::updateRecentLoadedPresentations(QString path) {
    QStringList recentPresentations = PresentationSettings::recentLoadedPresentations();
    recentPresentations.push_front(path);
    recentPresentations.removeDuplicates();
    if (recentPresentations.size() > 8)
        recentPresentations.pop_back();
    PresentationSettings::setRecentLoadedPresentations(recentPresentations);
    Q_EMIT recentPresentationsChanged();
    PresentationSettings::self()->save();
}