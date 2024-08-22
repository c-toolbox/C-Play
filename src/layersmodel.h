/*
 * SPDX-FileCopyrightText: 
 * 2024 Erik Sundén <eriksunden85@gmail.com> 
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERSMODEL_H
#define LAYERSMODEL_H

#include <QAbstractTableModel>

class BaseLayer;

using Layers = QList<BaseLayer*>;

class LayersTypeModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit LayersTypeModel(QObject* parent = nullptr);

    enum {
        textRole = Qt::UserRole
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:

private:
    QStringList m_layerTypes;

};

class LayersModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit LayersModel(QObject *parent = nullptr);

    enum {
        TitleRole = Qt::UserRole,
        PathRole,
        TypeRole,
        StereoRole,
        GridRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Layers getLayers() const;
    void setLayers(const Layers& layers);

    Q_INVOKABLE BaseLayer* layer(int i);

    Q_INVOKABLE void addLayer(QString title, int type, QString filepath, int stereoMode, int gridMode);
    Q_INVOKABLE void removeLayer(int i);
    Q_INVOKABLE void moveLayerUp(int i);
    Q_INVOKABLE void moveLayerDown(int i);

    Q_PROPERTY(LayersTypeModel* layersTypeModel
        READ layersTypeModel
        WRITE setLayersTypeModel
        NOTIFY layersTypeModelChanged)

    LayersTypeModel* layersTypeModel();
    void setLayersTypeModel(LayersTypeModel* model);

Q_SIGNALS:
    void layersTypeModelChanged();

private:
    Layers m_layers;
    LayersTypeModel* m_layerTypeModel;

};

#endif // LAYERSMODEL_H
