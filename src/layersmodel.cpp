/*
 * SPDX-FileCopyrightText: 
 * 2024 Erik Sundén <eriksunden85@gmail.com> 
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layersmodel.h"
#include <layers/baselayer.h>
#include <layers/imagelayer.h>
#include <layers/mpvlayer.h>
#include <QQuickView>

LayersModel::LayersModel(QObject *parent)
    : QAbstractListModel(parent),
    m_layerTypeModel(new LayersTypeModel(this))
{
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
    return roles;
}

Layers LayersModel::getLayers() const
{
    return m_layers;
}

void LayersModel::setLayers(const Layers &layers)
{
    beginInsertRows(QModelIndex(), 0, layers.size() - 1);
    m_layers = layers;
    endInsertRows();
}

void LayersModel::addLayer(QString title, int type, QString filepath, int stereoMode, int gridMode)
{
    //Need to offest type ID by 1 (to avoid BASE);
    int layerType = type + 1;

    beginInsertRows(QModelIndex(), m_layers.size(), m_layers.size());
    BaseLayer* newLayer = nullptr;

    switch (layerType) {
        case static_cast<int>(BaseLayer::LayerType::IMAGE): {
            ImageLayer* newImg = new ImageLayer(title.toStdString());
            newLayer = newImg;
            break;
        }
        case static_cast<int>(BaseLayer::LayerType::VIDEO): {
            MpvLayer* newMpv = new MpvLayer();
            newLayer = newMpv;
            break;
        }
        default:
            break;
    }

    if (newLayer) {
        newLayer->setTitle(title.toStdString());
        newLayer->setFilePath(filepath.toStdString());
        newLayer->setStereoMode(stereoMode);
        newLayer->setGridMode(gridMode);
        m_layers.push_back(newLayer);
    }

    endInsertRows();
}

void LayersModel::removeLayer(int i) {
    beginRemoveRows(QModelIndex(), i, i);
    m_layers.removeAt(i);
    endRemoveRows();
}

void LayersModel::moveLayerUp(int i) {
    if (i == 0) return;
    beginMoveRows(QModelIndex(), i, i, QModelIndex(), i - 1);
    m_layers.move(i, i - 1);
    endMoveRows();
}

void LayersModel::moveLayerDown(int i) {
    if (i == (m_layers.size() - 1)) return;
    beginMoveRows(QModelIndex(), i + 1, i + 1, QModelIndex(), i);
    m_layers.move(i, i + 1);
    endMoveRows();
}

LayersTypeModel* LayersModel::layersTypeModel() {
    return m_layerTypeModel;
}

void LayersModel::setLayersTypeModel(LayersTypeModel* model) {
    m_layerTypeModel = model;
}

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