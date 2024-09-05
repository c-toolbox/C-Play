/*
 * SPDX-FileCopyrightText: 
 * 2024 Erik Sundén <eriksunden85@gmail.com> 
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layersmodel.h"
#include <layers/baselayer.h>
#include "layersettings.h"
#include "locationsettings.h"
#include <QQuickView>
#include <QOpenGLContext>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#ifdef NDI_SUPPORT
#include <ndi/ofxNDI/ofxNDIreceive.h>
#endif

static void* get_proc_address_qopengl(void* ctx, const char* name)
{
    Q_UNUSED(ctx)

    QOpenGLContext* glctx = QOpenGLContext::currentContext();
    if (!glctx) return nullptr;

    return reinterpret_cast<void*>(glctx->getProcAddress(QByteArray(name)));
}

LayersModel::LayersModel(QObject *parent)
    : QAbstractListModel(parent),
    m_layerTypeModel(new LayersTypeModel(this)),
    m_needsSync(false),
    m_layersName(QStringLiteral("Untitled")),
    m_layersPath(QStringLiteral(""))
{
#ifdef NDI_SUPPORT
    m_ndiSendersModel = new NDISendersModel(this);
#endif
}

LayersModel::~LayersModel() 
{ 
    for (auto l : m_layers)
        delete l;
    m_layers.clear();
}

int LayersModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_layers.size();
}

QVariant LayersModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || m_layers.empty())
        return QVariant();

    const BaseLayer* layerItem = m_layers.at(index.row());

    int stereoVideo = layerItem->stereoMode();
    int gridToMapOn = layerItem->gridMode();

    switch (role) {
    case TitleRole:
        return QVariant(QString::fromStdString(layerItem->title()));
    case PathRole:
        return QVariant(QString::fromStdString(layerItem->filepath()));
    case TypeRole:
        return QVariant(QString::fromStdString(layerItem->typeName()));
    case StereoRole:
        if (stereoVideo == 0) {
            return QVariant(QStringLiteral("2D"));
        }
        else if (stereoVideo > 0) {
            return QVariant(QStringLiteral("3D"));
        }
        else {
            return QVariant(QStringLiteral(""));
        }
    case GridRole:
        if (gridToMapOn == 0) {
            return QVariant(QStringLiteral("Split"));
        }
        else if (gridToMapOn == 1) {
            return QVariant(QStringLiteral("Flat"));
        }
        else if (gridToMapOn == 2) {
            return QVariant(QStringLiteral("Dome"));
        }
        else if (gridToMapOn == 3 || gridToMapOn == 4) {
            return QVariant(QStringLiteral("Sphere"));
        }
        else {
            return QVariant(QStringLiteral(""));
        }
    case VisibilityRole:
        return QVariant(static_cast<int>(layerItem->alpha()*100.f));
    }

    return QVariant();
}

QHash<int, QByteArray> LayersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[TitleRole] = "title";
    roles[PathRole] = "filepath";
    roles[TypeRole] = "type";
    roles[StereoRole] = "stereoVideo";
    roles[GridRole] = "gridToMapOn";
    roles[VisibilityRole] = "visibility";
    return roles;
}

Layers LayersModel::getLayers() const
{
    return m_layers;
}

void LayersModel::setLayers(const Layers &layers)
{
    beginResetModel();
    m_layers = layers;
    endResetModel();
}

int LayersModel::numberOfLayers()
{
    return m_layers.size();
}

bool LayersModel::needsSync()
{
    return m_needsSync;
}

void LayersModel::setHasSynced()
{
    m_needsSync = false;
}

BaseLayer* LayersModel::layer(int i)
{
    if (i >= 0 && m_layers.size() > i)
        return m_layers[i];
    else
        return nullptr;
}

int LayersModel::addLayer(QString title, int type, QString filepath, int stereoMode, int gridMode)
{
    beginInsertRows(QModelIndex(), m_layers.size(), m_layers.size());

    //Create new layer
    //Need to offest type ID by 1 (to avoid BASE);
    int layerType = type + 1;
    BaseLayer* newLayer = BaseLayer::createLayer(layerType, get_proc_address_qopengl, title.toStdString());

    if (newLayer) {
        newLayer->setTitle(title.toStdString());
        newLayer->setFilePath(filepath.toStdString());
        newLayer->setStereoMode(stereoMode);
        newLayer->setGridMode(gridMode);
        newLayer->setAlpha(static_cast<float>(LayerSettings::defaultLayerVisibility()));
        m_layers.push_back(newLayer);
        setLayersNeedsSave(true);
        m_needsSync = true;
    }

    endInsertRows();

    return m_layers.size() - 1;
}

void LayersModel::removeLayer(int i) {
    if (i < 0 || i >= m_layers.size())
        return;

    beginRemoveRows(QModelIndex(), i, i);
    delete m_layers[i];
    m_layers.removeAt(i);
    endRemoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;
}

void LayersModel::moveLayerTop(int i) {
    if (i == 0) return;
    beginMoveRows(QModelIndex(), i, i, QModelIndex(), 0);
    m_layers.move(i, 0);
    endMoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;
}

void LayersModel::moveLayerUp(int i) {
    if (i == 0) return;
    beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1);
    m_layers.move(i, i - 1);
    endMoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;
}

void LayersModel::moveLayerDown(int i) {
    if (i == (m_layers.size() - 1)) return;
    beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i);
    m_layers.move(i, i + 1);
    endMoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;
}

void LayersModel::moveLayerBottom(int i) {
    if (i == (m_layers.size() - 1)) return;
    beginMoveRows(QModelIndex(), m_layers.size() - 1, m_layers.size() - 1, QModelIndex(), i);
    m_layers.move(i, m_layers.size() - 1);
    endMoveRows();
    setLayersNeedsSave(true);
    m_needsSync = true;
}

void LayersModel::updateLayer(int i)
{
    Q_EMIT dataChanged(index(i, 0), index(i, 0));
    setLayersNeedsSave(true);
}

void LayersModel::clearLayers() 
{
    beginRemoveRows(QModelIndex(), 0, m_layers.size()-1);
    for (auto l : m_layers)
        delete l;
    m_layers.clear();
    endRemoveRows();
    setLayersNeedsSave(false);
    m_needsSync = true;
}

void LayersModel::setLayersVisibility(int value) 
{
    m_layersVisibility = value;
    for (int i = 0; i < m_layers.size(); i++) {
        m_layers[i]->setAlpha(static_cast<float>(value) * 0.01f);
        updateLayer(i);
    }
    Q_EMIT layersVisibilityChanged();
    setLayersNeedsSave(true);
    m_needsSync = true;
}

int LayersModel::getLayersVisibility() 
{
    return m_layersVisibility;
}

void LayersModel::setLayersNeedsSave(bool value) 
{
    m_layersNeedsSave = value;
    Q_EMIT layersNeedsSaveChanged();
}

bool LayersModel::getLayersNeedsSave()
{
    return m_layersNeedsSave;
}

LayersTypeModel* LayersModel::layersTypeModel() {
    return m_layerTypeModel;
}

void LayersModel::setLayersName(QString name)
{
    m_layersName = name;
    Q_EMIT layersNameChanged();
}

QString LayersModel::getLayersName() const
{
    return m_layersName;
}

void LayersModel::setLayersPath(QString path)
{
    m_layersPath = path;
}

QString LayersModel::getLayersPath() const
{
    return m_layersPath;
}

QUrl LayersModel::getLayersPathAsURL() const
{
    return QUrl(QStringLiteral("file:///") + m_layersPath);
}

QString LayersModel::checkAndCorrectPath(const QString& filePath, const QStringList& searchPaths) {
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

QString LayersModel::makePathRelativeTo(const QString& filePath, const QStringList& pathsToConsider) {
    // Assuming filePath is absolute
    for (int i = 0; i < pathsToConsider.size(); i++) {
        if (filePath.startsWith(pathsToConsider[i])) {
            QDir foundDir(pathsToConsider[i]);
            return foundDir.relativeFilePath(filePath);
        }
    }
    return filePath;
}

void LayersModel::decodeFromJSON(QJsonObject& obj, const QStringList& forRelativePaths) {
    if(obj.contains(QStringLiteral("name"))) {
        QString name = obj.value(QStringLiteral("name")).toString();
        setLayersName(name);
    }

    if (obj.contains(QStringLiteral("layers"))) {
        clearLayers();
        QJsonValue value = obj.value(QStringLiteral("layers"));
        QJsonArray array = value.toArray();
        for (auto v : array) {
            QJsonObject o = v.toObject();
            if (o.contains(QStringLiteral("type"))){
                int type = 0;
                QString typeStr = o.value(QStringLiteral("type")).toString();
                for (int i = 1; i != (int)BaseLayer::LayerType::INVALID; i++) {
                    if (typeStr == QString::fromStdString(BaseLayer::typeDescription((BaseLayer::LayerType)i))) {
                        type = i;
                    }
                }

                if (type > 0 && o.contains(QStringLiteral("title"))
                    && o.contains(QStringLiteral("path"))
                    && o.contains(QStringLiteral("grid"))
                    && o.contains(QStringLiteral("stereoscopic"))) {
                    QString title = o.value(QStringLiteral("title")).toString();

                    QString path = o.value(QStringLiteral("path")).toString();
                    path = checkAndCorrectPath(path, forRelativePaths);

                    int grid = LayerSettings::defaultGridModeForLayers();
                    QString gridStr = o.value(QStringLiteral("grid")).toString();
                    if (gridStr == QStringLiteral("none") || gridStr == QStringLiteral("pre-split")) {
                        grid = 0;
                    }
                    else if (gridStr == QStringLiteral("plane") || gridStr == QStringLiteral("flat")) {
                        grid = 1;
                    }
                    else if (gridStr == QStringLiteral("dome")) {
                        grid = 2;
                    }
                    else if (gridStr == QStringLiteral("sphere") || gridStr == QStringLiteral("eqr") || gridStr == QStringLiteral("sphere-eqr")) {
                        grid = 3;
                    }
                    else if (gridStr == QStringLiteral("eac") || gridStr == QStringLiteral("sphere-eac")) {
                        grid = 4;
                    }

                    int stereo = LayerSettings::defaultStereoModeForLayers();
                    QString stereoStr = o.value(QStringLiteral("stereoscopic")).toString();
                    if (stereoStr == QStringLiteral("no") || stereoStr == QStringLiteral("mono")) {
                        stereo = 0;
                    }
                    else if (stereoStr == QStringLiteral("yes") || stereoStr == QStringLiteral("sbs") || stereoStr == QStringLiteral("side-by-side")) {
                        stereo = 1;
                    }
                    else if (stereoStr == QStringLiteral("tb") || stereoStr == QStringLiteral("top-bottom")) {
                        stereo = 2;
                    }
                    else if (stereoStr == QStringLiteral("tbf") || stereoStr == QStringLiteral("top-bottom-flip")) {
                        stereo = 3;
                    }

                    int idx = addLayer(title, type, path, stereo, grid);

                    if (o.contains(QStringLiteral("visibility"))) {
                        int visibility = o.value(QStringLiteral("visibility")).toInt();
                        m_layers[idx]->setAlpha(static_cast<float>(visibility) * 0.01f);
                    }
                }
            }
        }
    }
}

void LayersModel::encodeToJSON(QJsonObject& obj, const QStringList& forRelativePaths) {
    obj.insert(QStringLiteral("name"), QJsonValue(getLayersName()));

    QJsonArray layersArray;
    for (auto layer : m_layers)
    {
        QJsonObject layerData;

        layerData.insert(QStringLiteral("type"), QJsonValue(QString::fromStdString(layer->typeName())));

        layerData.insert(QStringLiteral("title"), QJsonValue(QString::fromStdString(layer->title())));

        QString checkedFilePath = makePathRelativeTo(QString::fromStdString(layer->filepath()), forRelativePaths);
        layerData.insert(QStringLiteral("path"), QJsonValue(checkedFilePath));

        QString grid;
        int gridIdx = layer->gridMode();
        if (gridIdx == 1) {
            grid = QStringLiteral("plane");
        }
        else if (gridIdx == 2) {
            grid = QStringLiteral("dome");
        }
        else if (gridIdx == 3) {
            grid = QStringLiteral("sphere-eqr");
        }
        else if (gridIdx == 4) {
            grid = QStringLiteral("sphere-eac");
        }
        else { // 0
            grid = QStringLiteral("pre-split");
        }
        layerData.insert(QStringLiteral("grid"), QJsonValue(grid));

        QString sv;
        int stereoVideoIdx = layer->stereoMode();
        if (stereoVideoIdx == 1) {
            sv = QStringLiteral("side-by-side");
        }
        else if (stereoVideoIdx == 2) {
            sv = QStringLiteral("top-bottom");
        }
        else if (stereoVideoIdx == 3) {
            sv = QStringLiteral("top-bottom-flip");
        }
        else { // 0
            sv = QStringLiteral("no");
        }
        layerData.insert(QStringLiteral("stereoscopic"), QJsonValue(sv));

        layerData.insert(QStringLiteral("visibility"), QJsonValue(static_cast<int>(layer->alpha()*100.f)));

        layersArray.push_back(QJsonValue(layerData));
    }

    obj.insert(QString(QStringLiteral("layers")), QJsonValue(layersArray));

    setLayersNeedsSave(false);
}

#ifdef NDI_SUPPORT
NDISendersModel* LayersModel::ndiSendersModel() {
    return m_ndiSendersModel;
}

void LayersModel::setNdiSendersModel(NDISendersModel* model) {
    m_ndiSendersModel = model;
}
#endif

LayersTypeModel::LayersTypeModel(QObject* parent)
    : QAbstractListModel(parent)
{
    for (int i = 1; i != (int)BaseLayer::LayerType::INVALID; i++) {
        m_layerTypes.append(QString::fromStdString(BaseLayer::typeDescription((BaseLayer::LayerType)i)));
    }
}

int LayersTypeModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return m_layerTypes.size();
}

QVariant LayersTypeModel::data(const QModelIndex& index, int role) const
{
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

QHash<int, QByteArray> LayersTypeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[textRole] = "typeName";
    return roles;
}

#ifdef NDI_SUPPORT
NDISendersModel::NDISendersModel(QObject* parent)
    : QAbstractListModel(parent),
    m_NDIreceiver(new ofxNDIreceive())
{
    m_NDIreceiver->CreateFinder();
}

int NDISendersModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return m_NDIsenders.size();
}

QVariant NDISendersModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || m_NDIsenders.empty())
        return QVariant();

    if (!checkIndex(index)) {
        return QVariant();
    }
    if (role == textRole) {
        return m_NDIsenders.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> NDISendersModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[textRole] = "typeName";
    return roles;
}

int NDISendersModel::updateSendersList() 
{
    int senders = m_NDIreceiver->FindSenders();
    std::vector<std::string> sendersList = m_NDIreceiver->GetSenderList();

    beginResetModel();
    m_NDIsenders.clear();
    for (auto s : sendersList) {
        m_NDIsenders.append(QString::fromStdString(s));
    }
    endResetModel();

    if (senders > 0)
        return 0;
    else
        return -1;
}
#endif