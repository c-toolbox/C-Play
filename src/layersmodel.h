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
class ofxNDIreceive;

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

private:
    QStringList m_layerTypes;

};

#ifdef NDI_SUPPORT
class NDISendersModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit NDISendersModel(QObject* parent = nullptr);

    enum {
        textRole = Qt::UserRole
    };

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE int updateSendersList();

private:
    QStringList m_NDIsenders;
    ofxNDIreceive* m_NDIreceiver;

};
#endif

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
        GridRole,
        VisibilityRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Layers getLayers() const;
    void setLayers(const Layers& layers);

    int numberOfLayers();
    bool needsSync();
    void setHasSynced();

    Q_INVOKABLE BaseLayer* layer(int i);

    Q_INVOKABLE int addLayer(QString title, int type, QString filepath, int stereoMode, int gridMode);
    Q_INVOKABLE void removeLayer(int i);
    Q_INVOKABLE void moveLayerUp(int i);
    Q_INVOKABLE void moveLayerDown(int i);
    Q_INVOKABLE void updateLayer(int i);
    Q_INVOKABLE void clearLayers();

    Q_PROPERTY(LayersTypeModel* layersTypeModel
        READ layersTypeModel
        WRITE setLayersTypeModel
        NOTIFY layersTypeModelChanged)

    Q_PROPERTY(bool layersNeedsSave
        MEMBER m_layersNeedsSave
        READ getLayersNeedsSave
        WRITE setLayersNeedsSave
        NOTIFY layersNeedsSaveChanged)

    Q_INVOKABLE void setLayersNeedsSave(bool value);
    Q_INVOKABLE bool getLayersNeedsSave();

    LayersTypeModel* layersTypeModel();
    void setLayersTypeModel(LayersTypeModel* model);

#ifdef NDI_SUPPORT
    Q_PROPERTY(NDISendersModel* ndiSendersModel
        READ ndiSendersModel
        WRITE setNdiSendersModel
        NOTIFY ndiSendersModelChanged)

    NDISendersModel* ndiSendersModel();
    void setNdiSendersModel(NDISendersModel* model);
#endif

Q_SIGNALS:
    void layersTypeModelChanged();
    void layersNeedsSaveChanged();
#ifdef NDI_SUPPORT
    void ndiSendersModelChanged();
#endif

private:
    Layers m_layers;
    LayersTypeModel* m_layerTypeModel;
#ifdef NDI_SUPPORT
    NDISendersModel* m_ndiSendersModel;
#endif
    int m_layersNeedsSave = false;
    bool m_needsSync;
};

#endif // LAYERSMODEL_H
