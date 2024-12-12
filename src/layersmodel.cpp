/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layersmodel.h"
#include "gridsettings.h"
#include "locationsettings.h"
#include "presentationsettings.h"
#ifdef NDI_SUPPORT
#include <ndi/ndilayer.h>
#endif
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QOpenGLContext>
#include <QQuickView>

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
      m_needsSync(false),
      m_layersName(QStringLiteral("Untitled")),
      m_layersPath(QStringLiteral("")),
      m_layerHierachy(BaseLayer::LayerHierarchy::FRONT) {
}

LayersModel::~LayersModel() {
    for (auto l : m_layers)
        delete l;
    m_layers.clear();
    m_layersStatus.clear();
}

int LayersModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_layers.size();
}

QVariant LayersModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_layers.empty())
        return QVariant();

    const BaseLayer *layerItem = m_layers.at(index.row());

    int stereoVideo = layerItem->stereoMode();
    int gridToMapOn = layerItem->gridMode();

    switch (role) {
    case TitleRole:
        return QVariant(QString::fromStdString(layerItem->title()));
    case PathRole:
        return QVariant(QString::fromStdString(layerItem->filepath()));
    case TypeRole:
        return QVariant(QString::fromStdString(layerItem->typeName()));
    case PageRole:
#ifdef PDF_SUPPORT
        if (layerItem->type() == BaseLayer::PDF){
            return QVariant(QStringLiteral(" ") + QString::number(layerItem->page())
                + QStringLiteral("/") + QString::number(layerItem->numPages())  + QStringLiteral(" "));
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
        return QVariant(m_layersStatus.at(index.row()));
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
        l->setHierarchy(h);

    m_needsSync = true;
}

int LayersModel::numberOfLayers() {
    return m_layers.size();
}

bool LayersModel::needsSync() {
    return m_needsSync;
}

void LayersModel::setHasSynced() {
    m_needsSync = false;
}

BaseLayer *LayersModel::layer(int i) {
    if (i >= 0 && m_layers.size() > i)
        return m_layers[i];
    else
        return nullptr;
}

int LayersModel::layerStatus(int i) {
    if (i >= 0 && m_layersStatus.size() > i)
        return m_layersStatus[i];
    else
        return -1;
}

int LayersModel::minLayerStatus() {
    if (m_layersStatus.isEmpty())
        return -1;

    int minStatus = 2;
    for (int i = 0; i < m_layersStatus.size(); i++) {
        minStatus = std::min(minStatus, m_layersStatus[i]);
    }
    return minStatus;
}

int LayersModel::maxLayerStatus() {
    if (m_layersStatus.isEmpty())
        return -1;

    int maxStatus = 0;
    for (int i = 0; i < m_layersStatus.size(); i++) {
        maxStatus = std::max(maxStatus, m_layersStatus[i]);
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
        newLayer->setFilePath(filepath.toStdString());
        newLayer->setStereoMode(stereoMode);
        newLayer->setGridMode(gridMode);
        newLayer->setAlpha(static_cast<float>(PresentationSettings::defaultLayerVisibility()) * 0.01f);
        newLayer->setPlaneElevation(GridSettings::plane_Elevation_Degrees());
        newLayer->setPlaneDistance(GridSettings::plane_Distance_CM());
        newLayer->setPlaneSize(glm::vec2(GridSettings::plane_Width_CM(), GridSettings::plane_Height_CM()), 
            GridSettings::plane_Calculate_Size_Based_on_Video());
        newLayer->initialize();
        m_layers.push_back(newLayer);
        m_layersStatus.push_back(0);
        setLayersNeedsSave(true);
        m_needsSync = true;
    }

    endInsertRows();

    Q_EMIT layersModelChanged();

    return m_layers.size() - 1;
}

void LayersModel::removeLayer(int i) {
    if (i < 0 || i >= m_layers.size())
        return;

    beginRemoveRows(QModelIndex(), i, i);
    delete m_layers[i];
    m_layers.removeAt(i);
    m_layersStatus.removeAt(i);
    endRemoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerTop(int i) {
    if (i == 0)
        return;
    beginMoveRows(QModelIndex(), i, i, QModelIndex(), 0);
    m_layers.move(i, 0);
    m_layersStatus.move(i, 0);
    endMoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerUp(int i) {
    if (i == 0)
        return;
    beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1);
    m_layers.move(i, i - 1);
    m_layersStatus.move(i, i - 1);
    endMoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerDown(int i) {
    if (i == (m_layers.size() - 1))
        return;
    beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i);
    m_layers.move(i, i + 1);
    endMoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;

    Q_EMIT layersModelChanged();
}

void LayersModel::moveLayerBottom(int i) {
    if (i == (m_layers.size() - 1))
        return;
    beginMoveRows(QModelIndex(), m_layers.size() - 1, m_layers.size() - 1, QModelIndex(), i);
    m_layers.move(i, m_layers.size() - 1);
    m_layersStatus.move(i, m_layers.size() - 1);
    endMoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;

    Q_EMIT layersModelChanged();
}

void LayersModel::updateLayer(int i) {
    Q_EMIT dataChanged(index(i, 0), index(i, 0));
    setLayersNeedsSave(true);
}

void LayersModel::clearLayers() {
    beginRemoveRows(QModelIndex(), 0, m_layers.size() - 1);
    for (auto l : m_layers)
        delete l;
    m_layers.clear();
    m_layersStatus.clear();
    endRemoveRows();
    setLayersNeedsSave(false);
    m_needsSync = true;

    Q_EMIT layersModelChanged();
}

void LayersModel::setLayersVisibility(int value, bool propagateDown) {
    m_layersVisibility = value;
    for (int i = 0; i < m_layers.size(); i++) {
        if (propagateDown) {
            m_layers[i]->setAlpha(static_cast<float>(value) * 0.01f);
        }
        updateLayer(i);
    }
    Q_EMIT layersVisibilityChanged();
    setLayersNeedsSave(true);
    m_needsSync = true;
}

int LayersModel::getLayersVisibility() {
    return m_layersVisibility;
}

void LayersModel::setLayersNeedsSave(bool value) {
    m_layersNeedsSave = value;
    Q_EMIT layersNeedsSaveChanged();
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
        return m_layers[m_layerToCopyIdx];

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
        m_layers.push_back(newLayer);
        m_layersStatus.push_back(0);
        setLayersNeedsSave(true);
        m_needsSync = true;
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

    if(m_layers[dstLayerIdx]->type() == srcLayer->type())
        srcLayer->encodeTypeProperties(data);

    unsigned int pos = 0;
    m_layers[dstLayerIdx]->decodeBaseProperties(data, pos);

    if (m_layers[dstLayerIdx]->type() == srcLayer->type())
        m_layers[dstLayerIdx]->decodeTypeProperties(data, pos);

    updateLayer(dstLayerIdx);
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

void LayersModel::setLayersPath(QString path) {
    m_layersPath = path;
}

QString LayersModel::getLayersPath() const {
    return m_layersPath;
}

QUrl LayersModel::getLayersPathAsURL() const {
    return QUrl(QStringLiteral("file:///") + m_layersPath);
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
#ifdef NDI_SUPPORT
                    if (type != BaseLayer::NDI) {
                        path = checkAndCorrectPath(path, forRelativePaths);
                    }
#else
                    path = checkAndCorrectPath(path, forRelativePaths);
#endif

                    int grid = PresentationSettings::defaultGridModeForLayers();
                    QString gridStr = o.value(QStringLiteral("grid")).toString();
                    if (gridStr == QStringLiteral("none") || gridStr == QStringLiteral("pre-split")) {
                        grid = BaseLayer::GridMode::None;
                    } else if (gridStr == QStringLiteral("plane") || gridStr == QStringLiteral("flat")) {
                        grid = BaseLayer::GridMode::Plane;
                    } else if (gridStr == QStringLiteral("dome")) {
                        grid = BaseLayer::GridMode::Dome;
                    } else if (gridStr == QStringLiteral("sphere") || gridStr == QStringLiteral("eqr") || gridStr == QStringLiteral("sphere-eqr")) {
                        grid = BaseLayer::GridMode::Sphere_EQR;
                    } else if (gridStr == QStringLiteral("eac") || gridStr == QStringLiteral("sphere-eac")) {
                        grid = BaseLayer::GridMode::Sphere_EAC;
                    }

                    int stereo = PresentationSettings::defaultStereoModeForLayers();
                    QString stereoStr = o.value(QStringLiteral("stereoscopic")).toString();
                    if (stereoStr == QStringLiteral("no") || stereoStr == QStringLiteral("mono")) {
                        stereo = BaseLayer::StereoMode::No_2D;
                    } else if (stereoStr == QStringLiteral("yes") || stereoStr == QStringLiteral("sbs") || stereoStr == QStringLiteral("side-by-side")) {
                        stereo = BaseLayer::StereoMode::SBS_3D;
                    } else if (stereoStr == QStringLiteral("tb") || stereoStr == QStringLiteral("top-bottom")) {
                        stereo = BaseLayer::StereoMode::TB_3D;
                    } else if (stereoStr == QStringLiteral("tbf") || stereoStr == QStringLiteral("top-bottom-flip")) {
                        stereo = BaseLayer::StereoMode::TBF_3D;
                    }

                    int idx = addLayer(title, type, path, stereo, grid);

                    if (o.contains(QStringLiteral("numPages"))) {
                        int numPages = o.value(QStringLiteral("numPages")).toInt();
                        m_layers[idx]->setNumPages(numPages);
                    }

                    if (o.contains(QStringLiteral("page"))) {
                        int page = o.value(QStringLiteral("page")).toInt();
                        m_layers[idx]->setPage(page);
                    }

                    if (o.contains(QStringLiteral("visibility"))) {
                        int visibility = o.value(QStringLiteral("visibility")).toInt();
                        m_layers[idx]->setAlpha(static_cast<float>(visibility) * 0.01f);
                    }

                    if (o.contains(QStringLiteral("volume"))) {
                        int volume = o.value(QStringLiteral("volume")).toInt();
                        m_layers[idx]->setVolume(volume);
                    }

                    if (o.contains(QStringLiteral("audioId"))) {
                        int audioId = o.value(QStringLiteral("audioId")).toInt();
                        m_layers[idx]->setAudioId(audioId);
                    }

                    if (o.contains(QStringLiteral("keepVisibilityForNumSlides"))) {
                        int keepVisibilityForNumSlides = o.value(QStringLiteral("keepVisibilityForNumSlides")).toInt();
                        m_layers[idx]->setKeepVisibilityForNumSlides(keepVisibilityForNumSlides);
                    }

                    if (grid == BaseLayer::GridMode::Plane && o.contains(QStringLiteral("plane"))) {
                        QJsonValue planeValues = o.value(QStringLiteral("plane"));
                        QJsonArray planeArray = planeValues.toArray();
                        for (auto pa : planeArray) {
                            QJsonObject po = pa.toObject();
                            if (po.contains(QStringLiteral("aspectRatio"))) {
                                int planeAR = po.value(QStringLiteral("aspectRatio")).toInt();
                                m_layers[idx]->setPlaneAspectRatio(planeAR);
                            }
                            if (po.contains(QStringLiteral("width"))) {
                                double planeW = po.value(QStringLiteral("width")).toDouble();
                                m_layers[idx]->setPlaneWidth(planeW);
                            }
                            if (po.contains(QStringLiteral("height"))) {
                                double planeH = po.value(QStringLiteral("height")).toDouble();
                                m_layers[idx]->setPlaneHeight(planeH);
                            }
                            if (po.contains(QStringLiteral("distance"))) {
                                double planeD = po.value(QStringLiteral("distance")).toDouble();
                                m_layers[idx]->setPlaneDistance(planeD);
                            }
                            if (po.contains(QStringLiteral("elevation"))) {
                                double planeE = po.value(QStringLiteral("elevation")).toDouble();
                                m_layers[idx]->setPlaneElevation(planeE);
                            }
                            if (po.contains(QStringLiteral("azimuth"))) {
                                double planeA = po.value(QStringLiteral("azimuth")).toDouble();
                                m_layers[idx]->setPlaneAzimuth(planeA);
                            }
                            if (po.contains(QStringLiteral("roll"))) {
                                double planeR = po.value(QStringLiteral("roll")).toDouble();
                                m_layers[idx]->setPlaneRoll(planeR);
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
                                m_layers[idx]->setRoiEnabled(roiEnabled);
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
                            m_layers[idx]->setRoi(roi);
                        }
                    }
                }
            }
        }
    }
    m_needsSync = true;
}

void LayersModel::encodeToJSON(QJsonObject &obj, const QStringList &forRelativePaths) {
    obj.insert(QStringLiteral("name"), QJsonValue(getLayersName()));

    QJsonArray layersArray;
    for (auto layer : m_layers) {
        QJsonObject layerData;

        layerData.insert(QStringLiteral("type"), QJsonValue(QString::fromStdString(layer->typeName())));

        layerData.insert(QStringLiteral("title"), QJsonValue(QString::fromStdString(layer->title())));

#ifdef NDI_SUPPORT
        if (layer->type() == BaseLayer::NDI) {
            layerData.insert(QStringLiteral("path"), QJsonValue(QString::fromStdString(layer->filepath())));
        }
        else {
            QString checkedFilePath = makePathRelativeTo(QString::fromStdString(layer->filepath()), forRelativePaths);
            layerData.insert(QStringLiteral("path"), QJsonValue(checkedFilePath));
        }
#else
        QString checkedFilePath = makePathRelativeTo(QString::fromStdString(layer->filepath()), forRelativePaths);
        layerData.insert(QStringLiteral("path"), QJsonValue(checkedFilePath));
#endif
#ifdef PDF_SUPPORT
        if (layer->type() == BaseLayer::PDF) {
            layerData.insert(QStringLiteral("page"), QJsonValue(layer->page()));
            layerData.insert(QStringLiteral("numPages"), QJsonValue(layer->numPages()));
        }
#endif
        if (layer->type() == BaseLayer::VIDEO || layer->type() == BaseLayer::AUDIO) {
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
        } else { // 0
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
        } else { // 0
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
            planeData.insert(QStringLiteral("distance"), QJsonValue(layer->planeDistance()));
            planeData.insert(QStringLiteral("elevation"), QJsonValue(layer->planeElevation()));
            planeData.insert(QStringLiteral("azimuth"), QJsonValue(layer->planeAzimuth()));
            planeData.insert(QStringLiteral("roll"), QJsonValue(layer->planeRoll()));
            planeArray.push_back(QJsonValue(planeData));
            layerData.insert(QString(QStringLiteral("plane")), QJsonValue(planeArray));
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

        layersArray.push_back(QJsonValue(layerData));
    }

    obj.insert(QString(QStringLiteral("layers")), QJsonValue(layersArray));

    setLayersNeedsSave(false);
}

bool LayersModel::runRenderOnLayersThatShouldUpdate(bool updateRendering, bool preload) {
    bool statusHasUpdated = false;
    for (int i = 0; i < m_layers.size(); i++) {
        auto layer = m_layers[i];
        if (layer->shouldUpdate() || (preload && !layer->ready())) {
            if (!layer->hasInitialized()) {
                layer->initialize();
            }
            layer->update(updateRendering);
        }
        if (layer->ready() && layer->alpha() > 0.f) {
            if (m_layersStatus[i] != 2) {
                m_layersStatus[i] = 2;
                updateLayer(i);
                statusHasUpdated = true;
            }
        }
        else if (layer->ready()) {
            if (m_layersStatus[i] != 1) {
                m_layersStatus[i] = 1;
                updateLayer(i);
                statusHasUpdated = true;
            }
        }
        else {
            if (m_layersStatus[i] != 0) {
                m_layersStatus[i] = 0;
                updateLayer(i);
                statusHasUpdated = true;
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