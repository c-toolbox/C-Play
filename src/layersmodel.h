/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LAYERSMODEL_H
#define LAYERSMODEL_H

#include <QAbstractListModel>
#include <layers/baselayer.h>
#include <QtQml/qqmlregistration.h>

using Layers = QList<QSharedPointer<BaseLayer>>;

class LayersTypeModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(LayersTypeModel)

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
    QML_NAMED_ELEMENT(LayersModel)

public:
    explicit LayersModel(QObject *parent = nullptr);
    ~LayersModel();

    enum {
        TitleRole = Qt::UserRole,
        PathRole,
        TypeRole,
        PageRole,
        StereoRole,
        GridRole,
        StatusRole,
        VisibilityRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Layers getLayers() const;
    void setLayers(const Layers &layers);

    BaseLayer::LayerHierarchy hierarchy() const;
    void setHierarchy(BaseLayer::LayerHierarchy h);

    int numberOfLayers();
    bool needsSync();
    void setHasSynced();

    Q_INVOKABLE BaseLayer *layer(int i);
    Q_INVOKABLE int layerStatus(int i);
    Q_INVOKABLE int minLayerStatus();
    Q_INVOKABLE int maxLayerStatus();

    Q_INVOKABLE int addLayer(QString title, int type, QString filepath, int stereoMode, int gridMode);
    Q_INVOKABLE void removeLayer(int i);
    Q_INVOKABLE void moveLayerTop(int i);
    Q_INVOKABLE void moveLayerUp(int i);
    Q_INVOKABLE void moveLayerDown(int i);
    Q_INVOKABLE void moveLayerBottom(int i);
    Q_INVOKABLE void updateLayer(int i);
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
    BaseLayer* getLayerToCopy();
    void addCopyOfLayer(BaseLayer* srcLayer);
    void overwriteLayerProperties(BaseLayer* srcLayer, int dstLayerIdx);

    LayersTypeModel *layersTypeModel();

    Q_PROPERTY(QString layersName
                   READ getLayersName
                       WRITE setLayersName
                           NOTIFY layersNameChanged)

    Q_INVOKABLE void setLayersName(QString name);
    Q_INVOKABLE QString getLayersName() const;

    Q_INVOKABLE void setLayersPath(QString path);
    Q_INVOKABLE QString getLayersPath() const;
    Q_INVOKABLE QUrl getLayersPathAsURL() const;

    Q_INVOKABLE QString checkAndCorrectPath(const QString &filePath, const QStringList &searchPaths);
    Q_INVOKABLE QString makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider);

    void decodeFromJSON(QJsonObject &obj, const QStringList &forRelativePaths);
    void encodeToJSON(QJsonObject &obj, const QStringList &forRelativePaths);

    bool runRenderOnLayersThatShouldUpdate(bool updateRendering, bool preload);

Q_SIGNALS:
    void layersModelChanged();
    void layersTypeModelChanged();
    void layersVisibilityChanged();
    void layersNeedsSaveChanged();
    void layersNameChanged();
    void layerToCopyIdxChanged();

private:
    void setNeedSync();

    Layers m_layers;
    QList<int> m_layersStatus;
    LayersTypeModel *m_layerTypeModel;
    BaseLayer::LayerHierarchy m_layerHierachy;
    int m_layersVisibility = 0;
    int m_layerToCopyIdx = -1;
    bool m_layersNeedsSave = false;
    bool m_needSync;
    int m_syncIteration;
    QString m_layersName;
    QString m_layersPath;
};

#endif // LAYERSMODEL_H
