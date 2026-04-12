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
#include <layers/textlayer.h>
#include <layers/mpvlayer.h>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QQuickView>
#include <QMimeDatabase>
#include <QTimer>

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
    m_timelineTimer = new QTimer(this);
    m_timelineTimer->setInterval(16); // ~60 fps ticks
    connect(m_timelineTimer, &QTimer::timeout, this, &LayersModel::onTimelineTick);
}

LayersModel::~LayersModel() {
    m_layers.clear();
}

int LayersModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_layers.size();
}

QVariant LayersModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_layers.empty())
        return QVariant();

    const BaseLayer *layerItem = m_layers.at(index.row()).first.get();

    int stereoVideo = layerItem->stereoMode();
    int gridToMapOn = layerItem->gridMode();

    switch (role) {
    case TitleRole:
        return QVariant(QString::fromStdString(layerItem->title()));
    case PathRole:
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
        return QVariant(m_layers.at(index.row()).second);
    case VisibilityRole:
        return QVariant(static_cast<int>(layerItem->alpha() * 100.f));
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
    return roles;
}

Layers LayersModel::getLayers() const {
    return m_layers;
}

void LayersModel::setLayers(const Layers &layers) {
    beginResetModel();
    m_layers = layers;
    endResetModel();
}

BaseLayer::LayerHierarchy LayersModel::hierarchy() const {
    return m_layerHierachy;
}

void LayersModel::setHierarchy(BaseLayer::LayerHierarchy h) {
    m_layerHierachy = h;

    for (auto l : m_layers)
        l.first->setHierarchy(h);

    setNeedSync();
}

int LayersModel::numberOfLayers() {
    return m_layers.size();
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
    if (i >= 0 && m_layers.size() > i)
        return m_layers[i].first.get();
    else
        return nullptr;
}

QString LayersModel::layerTitle(int i) const {
    if (i >= 0 && m_layers.size() > i)
        return QString::fromStdString(m_layers[i].first->title());
    return QString();
}

int LayersModel::layerIdx(std::string title) {
    // Returning first with correct name
    QString titleLowCase = QString::fromStdString(title).toLower();
    for (int i = 0; i < m_layers.size(); i++) {
        if (QString::fromStdString(m_layers[i].first->title()).toLower() == titleLowCase) {
            return i;
        }
    }
    return -1;
}

int LayersModel::layerStatus(int i) {
    if (i >= 0 && m_layers.size() > i)
        return m_layers[i].second;
    else
        return -1;
}

int LayersModel::minLayerStatus() {
    if (m_layers.isEmpty())
        return -1;

    int minStatus = 2;
    for (int i = 0; i < m_layers.size(); i++) {
        minStatus = std::min(minStatus, m_layers[i].second);
    }
    return minStatus;
}

int LayersModel::maxLayerStatus() {
    if (m_layers.isEmpty())
        return -1;

    int maxStatus = 0;
    for (int i = 0; i < m_layers.size(); i++) {
        maxStatus = std::max(maxStatus, m_layers[i].second);
    }
    return maxStatus;
}

int LayersModel::addLayer(QString title, int type, QString filepath, int stereoMode, int gridMode) {
    beginInsertRows(QModelIndex(), m_layers.size(), m_layers.size());

    // Create new layer
    BaseLayer *newLayer = BaseLayer::createLayer(true, type, get_proc_address_qopengl_v1, get_proc_address_qopengl_v2, title.toStdString());

    if (newLayer) {
        newLayer->setHierarchy(hierarchy());
        newLayer->setTitle(title.toStdString());
        if (type == BaseLayer::TEXT) {
            TextLayer* newTextLayer = static_cast<TextLayer*>(newLayer);
            newTextLayer->setFont(SubtitleSettings::subtitleFontFamily().toStdString());
            newTextLayer->setFontSize(SubtitleSettings::subtitleFontSize());
            newTextLayer->setTextureSize(SubtitleSettings::subtitleTextureWidth(), SubtitleSettings::subtitleTextureHeight());
            newTextLayer->setAlignment(SubtitleSettings::subtitleAlignment());
            QColor textColor(SubtitleSettings::subtitleColor());
            newTextLayer->setColor(textColor.name().toStdString(), textColor.redF(), textColor.greenF(), textColor.blueF());
            newTextLayer->setText(filepath.toStdString());
        }
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
        newLayer->initialize();
        m_layers.push_back(QPair(QSharedPointer<BaseLayer>(newLayer), 0));
        setLayersNeedsSave(true);
        setNeedSync();
    }

    endInsertRows();

    Q_EMIT layersModelChanged();

    return m_layers.size() - 1;
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
    int type = getLayerTypeBasedOnMime(fileUrl);
    if (type == BaseLayer::LayerType::INVALID)
        return -1;

    QString filePath = fileUrl.toLocalFile();
    if (filePath.isEmpty())
        return -1;

    QFileInfo fileInfo(filePath);
    return addLayer(fileInfo.baseName(), type, filePath, ImageSettings::stereoModeForBackground(), ImageSettings::gridToMapOnForBackground());
}

void LayersModel::removeLayer(int i) {
    if (i < 0 || i >= m_layers.size())
        return;

    if (getLayersCanBeLocked() && m_layers.at(i).first->isLocked())
        return;

    beginRemoveRows(QModelIndex(), i, i);
    m_layers.removeAt(i);
    endRemoveRows();
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayer(int i, int t) {
    if (i < 0 || t < 0 || i >= m_layers.size() || t >= m_layers.size() || i == t) {
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

    m_layers.move(i, t);
    endMoveRows();

    Q_EMIT dataChanged(index(t, 0), index(t, 0));
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerTop(int i) {
    if (i < 1)
        return;
    if (!beginMoveRows(QModelIndex(), i, i, QModelIndex(), 0))
        return;
    m_layers.move(i, 0);
    endMoveRows();
    for (int j = 0; j < i; j++)
        updateLayer(j);
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerUp(int i) {
    if (i < 1)
        return;
    if (!beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1))
        return;
    m_layers.move(i, i - 1);
    endMoveRows();
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerDown(int i) {
    if (i < 0 || i == (m_layers.size() - 1))
        return;
    if (!beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i))
        return;
    m_layers.move(i, i + 1);
    endMoveRows();
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerBottom(int i) {
    if (i < 0 || i == (m_layers.size() - 1))
        return;
    if (!beginMoveRows(QModelIndex(), m_layers.size() - 1, m_layers.size() - 1, QModelIndex(), i))
        return;
    m_layers.move(i, m_layers.size() - 1);
    endMoveRows();
    for (int j = i; j < m_layers.size(); j++)
        updateLayer(j);
    setLayersNeedsSave(true);
    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::updateLayer(int i) {
    Q_EMIT dataChanged(index(i, 0), index(i, 0));
}

void LayersModel::lockLayer(int i) {
    if (i < 0 || i >= m_layers.size())
        return;

    m_layers[i].first->setIsLocked(true);
    updateLayer(i);
    setLayersNeedsSave(true);
}

void LayersModel::unlockLayer(int i) {
    if (i < 0 || i >= m_layers.size())
        return;

    m_layers[i].first->setIsLocked(false);
    updateLayer(i);
    setLayersNeedsSave(true);
}

bool LayersModel::isLocked(int i) {
    if (!getLayersCanBeLocked() || i < 0 || i >= m_layers.size())
        return false;

    return m_layers[i].first->isLocked();
}

void LayersModel::clearLayers() {
    if (getLayersCanBeLocked()) {
        //Move all locked layers above unlocked ones
        //Remove unlocked layers
        int unlockedLayers = 0;
        int lockedLayers = 0;
        for (int i = 0; i < m_layers.size(); i++) {
            if (m_layers[i].first->isLocked()) {
                if (unlockedLayers > 0) {
                    beginMoveRows(QModelIndex(), i, i, QModelIndex(), lockedLayers);
                    m_layers.move(i, lockedLayers);
                    endMoveRows();
                }
                lockedLayers++;
            }
            else {
                unlockedLayers++;
            }
        }
        beginRemoveRows(QModelIndex(), lockedLayers, m_layers.size() - 1);
        m_layers.erase(m_layers.begin() + lockedLayers, m_layers.end());
        endRemoveRows();

        if (m_layers.isEmpty())
            setLayersNeedsSave(false);
        else
            setLayersNeedsSave(true);
    }
    else {
        beginRemoveRows(QModelIndex(), 0, m_layers.size() - 1);
        m_layers.clear();
        endRemoveRows();
        setLayersNeedsSave(false);
    }

    setNeedSync();

    Q_EMIT layersModelChanged();
}

void LayersModel::setLayersVisibility(int value, bool propagateDown) {
    m_layersVisibility = value;
    for (int i = 0; i < m_layers.size(); i++) {
        if (propagateDown) {
            m_layers[i].first->setAlpha(static_cast<float>(value) * 0.01f);
        }
        updateLayer(i);
    }
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

BaseLayer* LayersModel::getLayerToCopy() {
    if (m_layerToCopyIdx >= 0 && m_layerToCopyIdx < m_layers.size())
        return m_layers[m_layerToCopyIdx].first.get();

    return nullptr;
}

void LayersModel::addCopyOfLayer(BaseLayer* srcLayer) {
    if (srcLayer == nullptr)
        return;

    beginInsertRows(QModelIndex(), m_layers.size(), m_layers.size());

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
        m_layers.push_back(QPair(QSharedPointer<BaseLayer>(newLayer), 0));
        setLayersNeedsSave(true);
        setNeedSync();
    }

    endInsertRows();

    Q_EMIT layersModelChanged();
}

void LayersModel::overwriteLayerProperties(BaseLayer* srcLayer, int dstLayerIdx) {
    if (srcLayer == nullptr)
        return;

    if (dstLayerIdx < 0 || dstLayerIdx >= m_layers.size())
        return;

    std::vector<std::byte> data;
    srcLayer->encodeBaseProperties(data);

    if(m_layers[dstLayerIdx].first->type() == srcLayer->type())
        srcLayer->encodeTypeProperties(data);

    unsigned int pos = 0;
    m_layers[dstLayerIdx].first->decodeBaseProperties(data, pos);

    if (m_layers[dstLayerIdx].first->type() == srcLayer->type())
        m_layers[dstLayerIdx].first->decodeTypeProperties(data, pos);

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
    size_t countChars = m_layersName.size();
    if (countChars < maxChars) {
        return m_layersName;
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
        if (m_layers[i].first->isLocked()) {
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

        title += m_layers[i].first->title();

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

void LayersModel::decodeFromJSON(QJsonObject &obj, const QStringList &forRelativePaths) {
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
        QJsonValue value = obj.value(QStringLiteral("layers"));
        QJsonArray array = value.toArray();
        for (auto v : array) {
            QJsonObject o = v.toObject();
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

                    int idx = addLayer(title, type, path, stereo, grid);

#ifdef SGCT_HAS_TEXT
                    if (type == BaseLayer::TEXT) {
                        QSharedPointer<TextLayer> textLayer = qSharedPointerCast<TextLayer, BaseLayer>(m_layers[idx].first);
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
                        QSharedPointer<PdfLayer> pdfLayer = qSharedPointerCast<PdfLayer, BaseLayer>(m_layers[idx].first);
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

                    if ((type == BaseLayer::VIDEO || type == BaseLayer::AUDIO)) {
                        MpvLayer* mpvLayer = static_cast<MpvLayer*>(m_layers[idx].first.get());
                        if (o.contains(QStringLiteral("end_of_file"))) {
                            int eofMode = 2; // Loop by default
                            QString eofModeText = o.value(QStringLiteral("end_of_file")).toString();
                            if (eofModeText == QStringLiteral("pause")) {
                                eofMode = 0;
                            }
                            else if (eofModeText == QStringLiteral("loop")) {
                                eofMode = 2;
                            }
                            mpvLayer->setEOFMode(eofMode);
                        }
                        if (o.contains(QStringLiteral("section_enabled"))) {
                            bool loopEnabled = o.value(QStringLiteral("section_enabled")).toBool();
                            double loopA = o.contains(QStringLiteral("time_start")) ? o.value(QStringLiteral("time_start")).toDouble() : 0.0;
                            double loopB = o.contains(QStringLiteral("time_end")) ? o.value(QStringLiteral("time_end")).toDouble() : 0.0;
                            mpvLayer->setLoopTime(loopA, loopB, loopEnabled);
                        }
                    }

                    if (o.contains(QStringLiteral("keepVisibilityForNumSlides"))) {
                        int keepVisibilityForNumSlides = o.value(QStringLiteral("keepVisibilityForNumSlides")).toInt();
                        m_layers[idx].first->setKeepVisibilityForNumSlides(keepVisibilityForNumSlides);
                    }

                    if (grid == BaseLayer::GridMode::Plane && o.contains(QStringLiteral("plane"))) {
                        QJsonValue planeValues = o.value(QStringLiteral("plane"));
                        QJsonArray planeArray = planeValues.toArray();
                        for (auto pa : planeArray) {
                            QJsonObject po = pa.toObject();
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
                                double sphereRoll = po.value(QStringLiteral("yaw")).toDouble();
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

                    // Per-layer timeline keyframes
                    if (o.contains(QStringLiteral("timelineKeyframes"))) {
                        ensureTimelineSizeMatchesLayers();
                        QJsonArray kfArray = o.value(QStringLiteral("timelineKeyframes")).toArray();
                        LayerTimeline track;
                        for (auto kv : kfArray) {
                            QJsonObject ko = kv.toObject();
                            LayerKeyframe kf;
                            kf.timeMs       = ko.value(QStringLiteral("timeMs")).toInt();
                            kf.alpha        = static_cast<float>(ko.value(QStringLiteral("alpha")).toDouble());
                            kf.hasRotate    = ko.value(QStringLiteral("hasRotate")).toBool(false);
                            kf.rotateX      = static_cast<float>(ko.value(QStringLiteral("rotateX")).toDouble(0.0));
                            kf.rotateY      = static_cast<float>(ko.value(QStringLiteral("rotateY")).toDouble(0.0));
                            kf.rotateZ      = static_cast<float>(ko.value(QStringLiteral("rotateZ")).toDouble(0.0));
                            kf.hasTranslate = ko.value(QStringLiteral("hasTranslate")).toBool(false);
                            kf.translateX   = static_cast<float>(ko.value(QStringLiteral("translateX")).toDouble(0.0));
                            kf.translateY   = static_cast<float>(ko.value(QStringLiteral("translateY")).toDouble(0.0));
                            kf.translateZ   = static_cast<float>(ko.value(QStringLiteral("translateZ")).toDouble(0.0));
                            track.keyframes.append(kf);
                        }
                        if (idx < m_layerTimelines.size())
                            m_layerTimelines[idx] = track;
                    }
                }
            }
        }
    }
    setNeedSync();
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

#ifdef SGCT_HAS_TEXT
        if (layer->type() == BaseLayer::TEXT) {
            QSharedPointer<TextLayer> textLayer = qSharedPointerCast<TextLayer, BaseLayer>(layer);
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
            QSharedPointer<PdfLayer> pdfLayer = qSharedPointerCast<PdfLayer, BaseLayer>(layer);
            layerData.insert(QStringLiteral("page"), QJsonValue(pdfLayer->page()));
            layerData.insert(QStringLiteral("numPages"), QJsonValue(pdfLayer->numPages()));
        }
#endif
#ifdef NDI_SUPPORT
        if (layer->type() == BaseLayer::NDI) {
            layerData.insert(QStringLiteral("volume"), QJsonValue(layer->volume()));
        }
#endif
        if (layer->type() == BaseLayer::VIDEO || layer->type() == BaseLayer::AUDIO) {
            if (layer->hasAudio()) {
                layerData.insert(QStringLiteral("volume"), QJsonValue(layer->volume()));
                layerData.insert(QStringLiteral("audioId"), QJsonValue(layer->audioId()));
            }
            MpvLayer* mpvLayer = static_cast<MpvLayer*>(layer.get());
            if (mpvLayer->eofMode() >= 0) {
                int eofMode = mpvLayer->eofMode();
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
            if (mpvLayer->loopTimeEnabled()) {
                layerData.insert(QStringLiteral("section_enabled"), QJsonValue(mpvLayer->loopTimeEnabled()));
                layerData.insert(QStringLiteral("time_start"), QJsonValue(mpvLayer->loopTimeA()));
                layerData.insert(QStringLiteral("time_end"), QJsonValue(mpvLayer->loopTimeB()));
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

        // Plane properties
        if (gridIdx == BaseLayer::GridMode::Plane) {
            QJsonArray planeArray;
            QJsonObject planeData;
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

        // Per-layer timeline keyframes
        if (m_hasTimeline && li < m_layerTimelines.size() && !m_layerTimelines[li].keyframes.isEmpty()) {
            QJsonArray kfArray;
            for (const LayerKeyframe &kf : m_layerTimelines[li].keyframes) {
                QJsonObject kfObj;
                kfObj.insert(QStringLiteral("timeMs"), QJsonValue(kf.timeMs));
                kfObj.insert(QStringLiteral("alpha"),  QJsonValue(static_cast<double>(kf.alpha)));
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
                kfArray.push_back(kfObj);
            }
            layerData.insert(QStringLiteral("timelineKeyframes"), kfArray);
        }

        layersArray.push_back(QJsonValue(layerData));
    }

    obj.insert(QString(QStringLiteral("layers")), QJsonValue(layersArray));

    setLayersNeedsSave(false);
}

bool LayersModel::runRenderOnLayersThatShouldUpdate(bool updateRendering, bool preload) {
    bool statusHasUpdated = false;
    for (int i = 0; i < m_layers.size(); i++) {
        auto layer = &m_layers[i].first;
        if (layer && !layer->isNull() && layer->data()) {
            if (layer->data()->shouldUpdate()
                || (preload && !layer->data()->ready()) 
                || (layer->data()->shouldPreLoad() && !layer->data()->ready())) {
                if (!layer->data()->hasInitialized()) {
                    layer->data()->initialize();
                }
                layer->data()->update(layer->data()->shouldUpdate() && updateRendering);
            }
            if (layer->data()->ready() && layer->data()->shouldUpdateFrame()) {
                // Mostly here to handle update on layers in other windows
                layer->data()->updateFrame();
            }
            if (m_layers.size() > i) {
                int currentStatus = m_layers[i].second;
                if (layer && !layer->isNull() && layer->data()->ready() && layer->data()->alpha() > 0.f) {
                    m_layers[i].second = 2;
                }
                else if (layer && !layer->isNull() && layer->data()->ready()) {
                    m_layers[i].second = 1;
                }
                else {
                    m_layers[i].second = 0;
                }
                if (currentStatus != m_layers[i].second) {
                    updateLayer(i);
                    statusHasUpdated = true;
                }
            }
        }
    }
    return statusHasUpdated;
}

LayersTypeModel::LayersTypeModel(QObject *parent)
    : QAbstractListModel(parent) {
    for (int i = 1; i != (int)BaseLayer::LayerType::INVALID; i++) {
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

// ---- Timeline implementation ------------------------------------------------

bool LayersModel::getHasTimeline() const {
    return m_hasTimeline;
}

void LayersModel::setHasTimeline(bool value) {
    if (m_hasTimeline == value)
        return;
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
        // Auto-generate default fade-in / fade-out keyframes for layers without any keyframes
        int outroMs = (m_timelineOutroStart > 0) ? m_timelineOutroStart : m_timelineDuration;
        bool anyGenerated = false;
        for (int l = 0; l < m_layers.size(); ++l) {
            if (l < m_layerTimelines.size() && m_layerTimelines[l].keyframes.isEmpty()) {
                LayerKeyframe kfStart;
                kfStart.timeMs = 0;
                kfStart.alpha  = 0.f;
                LayerKeyframe kfFull;
                kfFull.timeMs = outroMs;
                kfFull.alpha  = 1.f;
                m_layerTimelines[l].keyframes.append(kfStart);
                m_layerTimelines[l].keyframes.append(kfFull);
                if (m_timelineOutroStart > 0) {
                    LayerKeyframe kfEnd;
                    kfEnd.timeMs = m_timelineDuration;
                    kfEnd.alpha  = 0.f;
                    m_layerTimelines[l].keyframes.append(kfEnd);
                }
                anyGenerated = true;
            }
        }
        if (anyGenerated)
            Q_EMIT layersModelChanged();
    }
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
    while (m_layerTimelines.size() < m_layers.size())
        m_layerTimelines.append(LayerTimeline());
}

QVariantList LayersModel::getKeyframes(int layerIdx) const {
    QVariantList result;
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size())
        return result;
    for (const LayerKeyframe &kf : m_layerTimelines[layerIdx].keyframes) {
        QVariantMap m;
        m[QStringLiteral("timeMs")]       = kf.timeMs;
        m[QStringLiteral("alpha")]        = static_cast<double>(kf.alpha);
        m[QStringLiteral("hasRotate")]    = kf.hasRotate;
        m[QStringLiteral("rotateX")]      = static_cast<double>(kf.rotateX);
        m[QStringLiteral("rotateY")]      = static_cast<double>(kf.rotateY);
        m[QStringLiteral("rotateZ")]      = static_cast<double>(kf.rotateZ);
        m[QStringLiteral("hasTranslate")] = kf.hasTranslate;
        m[QStringLiteral("translateX")]   = static_cast<double>(kf.translateX);
        m[QStringLiteral("translateY")]   = static_cast<double>(kf.translateY);
        m[QStringLiteral("translateZ")]   = static_cast<double>(kf.translateZ);
        result.append(m);
    }
    return result;
}

void LayersModel::setKeyframes(int layerIdx, const QVariantList &keyframes) {
    ensureTimelineSizeMatchesLayers();
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size())
        return;
    LayerTimeline track;
    for (const QVariant &v : keyframes) {
        QVariantMap m = v.toMap();
        LayerKeyframe kf;
        kf.timeMs       = m.value(QStringLiteral("timeMs"),       0).toInt();
        kf.alpha        = static_cast<float>(m.value(QStringLiteral("alpha"),        0.0).toDouble());
        kf.hasRotate    = m.value(QStringLiteral("hasRotate"),    false).toBool();
        kf.rotateX      = static_cast<float>(m.value(QStringLiteral("rotateX"),      0.0).toDouble());
        kf.rotateY      = static_cast<float>(m.value(QStringLiteral("rotateY"),      0.0).toDouble());
        kf.rotateZ      = static_cast<float>(m.value(QStringLiteral("rotateZ"),      0.0).toDouble());
        kf.hasTranslate = m.value(QStringLiteral("hasTranslate"), false).toBool();
        kf.translateX   = static_cast<float>(m.value(QStringLiteral("translateX"),   0.0).toDouble());
        kf.translateY   = static_cast<float>(m.value(QStringLiteral("translateY"),   0.0).toDouble());
        kf.translateZ   = static_cast<float>(m.value(QStringLiteral("translateZ"),   0.0).toDouble());
        track.keyframes.append(kf);
    }
    m_layerTimelines[layerIdx] = track;
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::addKeyframe(int layerIdx, int timeMs, float alpha) {
    ensureTimelineSizeMatchesLayers();
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size())
        return;
    LayerKeyframe kf;
    kf.timeMs = std::max(0, std::min(timeMs, m_timelineDuration));
    kf.alpha  = std::max(0.f, std::min(alpha, 1.f));
    auto &kfs = m_layerTimelines[layerIdx].keyframes;
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
    ensureTimelineSizeMatchesLayers();
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size())
        return;
    LayerKeyframe kf;
    kf.timeMs       = std::max(0, std::min(timeMs, m_timelineDuration));
    kf.alpha        = std::max(0.f, std::min(alpha, 1.f));
    kf.hasRotate    = hasRotate;
    kf.rotateX      = rotateX; kf.rotateY = rotateY; kf.rotateZ = rotateZ;
    kf.hasTranslate = hasTranslate;
    kf.translateX   = translateX; kf.translateY = translateY; kf.translateZ = translateZ;
    auto &kfs = m_layerTimelines[layerIdx].keyframes;
    int insertPos = 0;
    while (insertPos < kfs.size() && kfs[insertPos].timeMs <= kf.timeMs)
        ++insertPos;
    kfs.insert(insertPos, kf);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::removeKeyframe(int layerIdx, int keyframeIdx) {
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size())
        return;
    auto &kfs = m_layerTimelines[layerIdx].keyframes;
    if (keyframeIdx < 0 || keyframeIdx >= kfs.size())
        return;
    kfs.removeAt(keyframeIdx);
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::updateKeyframe(int layerIdx, int keyframeIdx, int timeMs, float alpha) {
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size())
        return;
    auto &kfs = m_layerTimelines[layerIdx].keyframes;
    if (keyframeIdx < 0 || keyframeIdx >= kfs.size())
        return;
    kfs[keyframeIdx].timeMs = std::max(0, std::min(timeMs, m_timelineDuration));
    kfs[keyframeIdx].alpha  = std::max(0.f, std::min(alpha, 1.f));
    std::sort(kfs.begin(), kfs.end(), [](const LayerKeyframe &a, const LayerKeyframe &b) {
        return a.timeMs < b.timeMs;
    });
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

void LayersModel::updateKeyframe(int layerIdx, int keyframeIdx, int timeMs, float alpha,
                                 bool hasRotate,
                                 float rotateX, float rotateY, float rotateZ,
                                 bool hasTranslate,
                                 float translateX, float translateY, float translateZ) {
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size())
        return;
    auto &kfs = m_layerTimelines[layerIdx].keyframes;
    if (keyframeIdx < 0 || keyframeIdx >= kfs.size())
        return;
    kfs[keyframeIdx].timeMs       = std::max(0, std::min(timeMs, m_timelineDuration));
    kfs[keyframeIdx].alpha        = std::max(0.f, std::min(alpha, 1.f));
    kfs[keyframeIdx].hasRotate    = hasRotate;
    kfs[keyframeIdx].rotateX      = rotateX; kfs[keyframeIdx].rotateY = rotateY; kfs[keyframeIdx].rotateZ = rotateZ;
    kfs[keyframeIdx].hasTranslate = hasTranslate;
    kfs[keyframeIdx].translateX   = translateX; kfs[keyframeIdx].translateY = translateY; kfs[keyframeIdx].translateZ = translateZ;
    std::sort(kfs.begin(), kfs.end(), [](const LayerKeyframe &a, const LayerKeyframe &b) {
        return a.timeMs < b.timeMs;
    });
    setLayersNeedsSave(true);
    Q_EMIT timelineChanged();
}

float LayersModel::evaluateAlphaAt(int layerIdx, int timeMs) const {
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size())
        return 0.f;
    const auto &kfs = m_layerTimelines[layerIdx].keyframes;
    if (kfs.isEmpty())
        return 0.f;
    if (timeMs <= kfs.first().timeMs) return kfs.first().alpha;
    if (timeMs >= kfs.last().timeMs)  return kfs.last().alpha;
    for (int i = 0; i < kfs.size() - 1; ++i) {
        if (timeMs >= kfs[i].timeMs && timeMs <= kfs[i + 1].timeMs) {
            float t = static_cast<float>(timeMs - kfs[i].timeMs)
                    / static_cast<float>(kfs[i + 1].timeMs - kfs[i].timeMs);
            return kfs[i].alpha + t * (kfs[i + 1].alpha - kfs[i].alpha);
        }
    }
    return kfs.last().alpha;
}

bool LayersModel::evaluateRotateAt(int layerIdx, int timeMs, float &rx, float &ry, float &rz) const {
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size()) return false;
    const auto &kfs = m_layerTimelines[layerIdx].keyframes;
    QVector<const LayerKeyframe *> rKfs;
    for (const LayerKeyframe &kf : kfs)
        if (kf.hasRotate) rKfs.append(&kf);
    if (rKfs.isEmpty()) return false;
    if (timeMs <= rKfs.first()->timeMs) { rx = rKfs.first()->rotateX; ry = rKfs.first()->rotateY; rz = rKfs.first()->rotateZ; return true; }
    if (timeMs >= rKfs.last()->timeMs)  { rx = rKfs.last()->rotateX;  ry = rKfs.last()->rotateY;  rz = rKfs.last()->rotateZ;  return true; }
    for (int i = 0; i < rKfs.size() - 1; ++i) {
        if (timeMs >= rKfs[i]->timeMs && timeMs <= rKfs[i + 1]->timeMs) {
            float t = static_cast<float>(timeMs - rKfs[i]->timeMs)
                    / static_cast<float>(rKfs[i + 1]->timeMs - rKfs[i]->timeMs);
            rx = rKfs[i]->rotateX + t * (rKfs[i + 1]->rotateX - rKfs[i]->rotateX);
            ry = rKfs[i]->rotateY + t * (rKfs[i + 1]->rotateY - rKfs[i]->rotateY);
            rz = rKfs[i]->rotateZ + t * (rKfs[i + 1]->rotateZ - rKfs[i]->rotateZ);
            return true;
        }
    }
    rx = rKfs.last()->rotateX; ry = rKfs.last()->rotateY; rz = rKfs.last()->rotateZ;
    return true;
}

bool LayersModel::evaluateTranslateAt(int layerIdx, int timeMs, float &tx, float &ty, float &tz) const {
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size()) return false;
    const auto &kfs = m_layerTimelines[layerIdx].keyframes;
    QVector<const LayerKeyframe *> tKfs;
    for (const LayerKeyframe &kf : kfs)
        if (kf.hasTranslate) tKfs.append(&kf);
    if (tKfs.isEmpty()) return false;
    if (timeMs <= tKfs.first()->timeMs) { tx = tKfs.first()->translateX; ty = tKfs.first()->translateY; tz = tKfs.first()->translateZ; return true; }
    if (timeMs >= tKfs.last()->timeMs)  { tx = tKfs.last()->translateX;  ty = tKfs.last()->translateY;  tz = tKfs.last()->translateZ;  return true; }
    for (int i = 0; i < tKfs.size() - 1; ++i) {
        if (timeMs >= tKfs[i]->timeMs && timeMs <= tKfs[i + 1]->timeMs) {
            float t = static_cast<float>(timeMs - tKfs[i]->timeMs)
                    / static_cast<float>(tKfs[i + 1]->timeMs - tKfs[i]->timeMs);
            tx = tKfs[i]->translateX + t * (tKfs[i + 1]->translateX - tKfs[i]->translateX);
            ty = tKfs[i]->translateY + t * (tKfs[i + 1]->translateY - tKfs[i]->translateY);
            tz = tKfs[i]->translateZ + t * (tKfs[i + 1]->translateZ - tKfs[i]->translateZ);
            return true;
        }
    }
    tx = tKfs.last()->translateX; ty = tKfs.last()->translateY; tz = tKfs.last()->translateZ;
    return true;
}

bool LayersModel::layerHasKeyframes(int layerIdx) const {
    if (layerIdx < 0 || layerIdx >= m_layerTimelines.size())
        return false;
    return !m_layerTimelines[layerIdx].keyframes.isEmpty();
}

void LayersModel::applyTimelineAt(int timeMs) {
    for (int l = 0; l < m_layers.size(); ++l) {
        if (l < m_layerTimelines.size() && !m_layerTimelines[l].keyframes.isEmpty()) {
            m_layers[l].first->setAlpha(evaluateAlphaAt(l, timeMs));
            float rx = 0.f, ry = 0.f, rz = 0.f;
            if (evaluateRotateAt(l, timeMs, rx, ry, rz)) {
                glm::vec3 r(rx, ry, rz);
                m_layers[l].first->setRotate(r);
            }
            float tx = 0.f, ty = 0.f, tz = 0.f;
            if (evaluateTranslateAt(l, timeMs, tx, ty, tz)) {
                glm::vec3 t(tx, ty, tz);
                m_layers[l].first->setTranslate(t);
            }
        } else {
            // No keyframes: fade in linearly over the intro, hold 1.0 from outroStart onward
            int introEnd = hasOutro() ? m_timelineOutroStart : m_timelineDuration;
            if (introEnd > 0 && timeMs < introEnd) {
                float t = static_cast<float>(timeMs) / static_cast<float>(introEnd);
                m_layers[l].first->setAlpha(t);
            } else {
                m_layers[l].first->setAlpha(1.f);
            }
        }
        updateLayer(l);
    }
}

void LayersModel::applyTimelineOutroAt(int outroTimeMs, const QVector<float> &targetAlphaPerLayer) {
    if (!hasOutro())
        return;
    int absTimeMs = std::max(m_timelineOutroStart, std::min(m_timelineOutroStart + outroTimeMs, m_timelineDuration));
    for (int l = 0; l < m_layers.size(); ++l) {
        if (l < m_layerTimelines.size() && !m_layerTimelines[l].keyframes.isEmpty()) {
            float targetAlpha = (l < targetAlphaPerLayer.size()) ? targetAlphaPerLayer[l] : m_layers[l].first->alpha();
            m_layers[l].first->setAlpha(evaluateAlphaAt(l, absTimeMs) * targetAlpha);
            float rx = 0.f, ry = 0.f, rz = 0.f;
            if (evaluateRotateAt(l, absTimeMs, rx, ry, rz)) {
                glm::vec3 r(rx, ry, rz);
                m_layers[l].first->setRotate(r);
            }
            float tx = 0.f, ty = 0.f, tz = 0.f;
            if (evaluateTranslateAt(l, absTimeMs, tx, ty, tz)) {
                glm::vec3 t(tx, ty, tz);
                m_layers[l].first->setTranslate(t);
            }
        } else {
            // No keyframes: layer is fully visible at the outro boundary, fades to 0
            int dur = outroDuration();
            if (dur > 0) {
                float ft = static_cast<float>(std::min(outroTimeMs, dur)) / static_cast<float>(dur);
                m_layers[l].first->setAlpha(1.f - ft);
            }
        }
        updateLayer(l);
    }
}

// ---- Timeline playback (per-slide) ------------------------------------------

void LayersModel::startTimeline() {
    startTimelineFrom(0);
}

void LayersModel::startTimelineFrom(int startMs) {
    if (!m_hasTimeline)
        return;

    stopTimeline();

    m_timelinePauseOffset = std::max(0, std::min(startMs, m_timelineDuration));
    m_timelineRunning     = true;
    m_timelineReversed    = false;
    m_timelinePositionMs  = m_timelinePauseOffset;
    m_timelineElapsed.start();
    ensureTimerRunning();

    Q_EMIT timelineStarted();
    Q_EMIT timelinePositionChanged(m_timelinePauseOffset);
}

void LayersModel::startTimelineReverse() {
    if (!m_hasTimeline)
        return;

    stopTimeline();

    m_timelinePauseOffset = 0;
    m_timelineRunning     = true;
    m_timelineReversed    = true;
    m_timelinePositionMs  = m_timelineDuration;
    m_timelineElapsed.start();
    ensureTimerRunning();

    Q_EMIT timelineStarted();
    Q_EMIT timelinePositionChanged(m_timelineDuration);
}

void LayersModel::startTimelineIntro() {
    if (!m_hasTimeline)
        return;

    stopTimeline();

    m_timelinePauseOffset = 0;
    m_timelineRunning     = true;
    m_timelineReversed    = false;
    m_timelinePositionMs  = 0;
    m_timelineElapsed.start();
    ensureTimerRunning();

    Q_EMIT timelineStarted();
    Q_EMIT timelinePositionChanged(0);
}

void LayersModel::startTimelineOutro() {
    if (!m_hasTimeline || !hasOutro())
        return;

    stopOutro();

    // Snap layers to the outro-start state so target alphas are fully intro'd
    applyTimelineAt(getTimelineOutroStart());

    m_outroTargetAlpha.clear();
    for (int l = 0; l < m_layers.size(); ++l)
        m_outroTargetAlpha.append(m_layers[l].first->alpha());

    m_outroRunning = true;
    m_timelinePositionMs = getTimelineOutroStart();
    m_outroElapsed.start();
    ensureTimerRunning();

    Q_EMIT outroStarted();
    Q_EMIT timelinePositionChanged(getTimelineOutroStart());
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
    if (m_timelineTimer)
        m_timelineTimer->stop();
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
    if (!m_hasTimeline)
        return false;
    for (int l = 0; l < rowCount(); ++l) {
        if (layerHasKeyframes(l))
            return true;
    }
    return false;
}

void LayersModel::jumpToIntroStart() {
    if (!m_hasTimeline)
        return;

    stopTimeline();

    applyTimelineAt(0);
    m_timelinePositionMs = 0;

    int vis = timelineVisibility();
    if (vis >= 0)
        setLayersVisibility(vis, false);

    Q_EMIT timelinePositionChanged(0);
}

void LayersModel::jumpToOutroStart() {
    if (!m_hasTimeline || !hasOutro())
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

    int posMs = m_timelinePositionMs;
    int duration = m_timelineDuration;
    if (duration <= 0)
        return 0;

    if (hasOutro()) {
        int outroStart = m_timelineOutroStart;
        int outroDur   = outroDuration();
        if (posMs <= outroStart) {
            if (outroStart <= 0)
                return 100;
            return static_cast<int>(posMs * 100 / outroStart);
        } else {
            int outroElapsed = posMs - outroStart;
            if (outroDur <= 0)
                return 0;
            return 100 - static_cast<int>(outroElapsed * 100 / outroDur);
        }
    }

    return static_cast<int>(posMs * 100 / duration);
}

void LayersModel::ensureTimerRunning() {
    if (m_timelineTimer && !m_timelineTimer->isActive())
        m_timelineTimer->start();
}

void LayersModel::stopTimerIfIdle() {
    if (!m_timelineRunning && !m_outroRunning && m_timelineTimer)
        m_timelineTimer->stop();
}

void LayersModel::onTimelineTick() {
    // --- Advance the intro / normal track ---
    if (m_timelineRunning) {
        if (!m_hasTimeline) {
            stopTimeline();
        } else {
            int elapsed  = static_cast<int>(m_timelineElapsed.elapsed()) + m_timelinePauseOffset;
            int duration = m_timelineDuration;
            int posMs = m_timelineReversed ? std::max(0, duration - elapsed) : elapsed;
            int stopAt = (hasOutro() && !m_timelineReversed) ? introDuration() : duration;
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

            if (finished)
                stopTimeline();
        }
    }

    // --- Advance the outro track ---
    if (m_outroRunning) {
        if (!m_hasTimeline) {
            stopOutro();
        } else {
            int elapsed    = static_cast<int>(m_outroElapsed.elapsed());
            int outroDur   = outroDuration();
            int outroTimeMs = std::min(elapsed, outroDur);
            bool finished  = (outroTimeMs >= outroDur);

            if (finished) {
                applyTimelineOutroAt(outroDur, m_outroTargetAlpha);
                applyTimelineAt(0);
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
}
