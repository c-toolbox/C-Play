/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SLIDESMODEL_H
#define SLIDESMODEL_H

#include <qqml.h>
#include <QAbstractListModel>
#include <QAbstractTableModel>

class BaseLayer;
class LayersModel;
class QTimer;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#ifndef OPAQUE_PTR_LayersModel
#define OPAQUE_PTR_SLayersModel
Q_DECLARE_OPAQUE_POINTER(LayersModel *)
#endif
#endif

class SlideVisibilityModel : public QAbstractTableModel
{
    Q_OBJECT
    QML_ELEMENT
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QML_ADDED_IN_VERSION(1, 1)
#else
    QML_ADDED_IN_MINOR_VERSION(1)
#endif

public:
    explicit SlideVisibilityModel(QList<LayersModel*> *slideList, QObject* parent = nullptr);
    ~SlideVisibilityModel();

    enum {
        DisplayRole = Qt::DisplayRole,
        ColorRole,
        TextRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void cellClicked(int column, int row);

private:
    BaseLayer* findLayer(int row);
    int calculateLocalIndex(int column, int row, bool ignoreKeepValue = false) const;
    QString cellColor(int column, int row) const;
    QString cellText(int column, int row) const;

    QList<LayersModel*>* m_slideList;
};

class SlidesModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit SlidesModel(QObject *parent = nullptr);
    ~SlidesModel();

    enum {
        NameRole = Qt::UserRole,
        PathRole,
        LayersRole,
        VisibilityRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    int numberOfSlides();

    Q_PROPERTY(bool needsSync
                   READ needsSync
                       WRITE setNeedsSync
                           NOTIFY needsSyncChanged)

    bool needsSync();
    void setNeedsSync(bool value);
    void setHasSynced();

    Q_PROPERTY(int selectedSlideIdx
                   READ selectedSlideIdx
                       WRITE setSelectedSlideIdx
                           NOTIFY selectedSlideChanged)

    Q_INVOKABLE int selectedSlideIdx();
    Q_INVOKABLE void setSelectedSlideIdx(int value);

    Q_INVOKABLE int previousSlideIdx();
    Q_INVOKABLE int nextSlideIdx();

    Q_PROPERTY(int triggeredSlideIdx
                   READ triggeredSlideIdx
                       WRITE setTriggeredSlideIdx
                           NOTIFY triggeredSlideChanged)

    Q_INVOKABLE int triggeredSlideIdx();
    Q_INVOKABLE void setTriggeredSlideIdx(int value);

    int previousTriggeredIdx();

    Q_PROPERTY(int triggeredSlideVisibility
                   READ triggeredSlideVisibility
                       WRITE setTriggeredSlideVisibility
                           NOTIFY triggeredSlideVisibilityChanged)

    Q_INVOKABLE int triggeredSlideVisibility();
    Q_INVOKABLE void setTriggeredSlideVisibility(int value);

    Q_PROPERTY(LayersModel *selected
                   READ selectedSlide
                       NOTIFY selectedSlideChanged)

    Q_INVOKABLE LayersModel *masterSlide();
    Q_INVOKABLE LayersModel *slide(int i);
    Q_INVOKABLE LayersModel *selectedSlide();

    Q_PROPERTY(SlideVisibilityModel *visibilityModel
        READ visibilityModel
        NOTIFY visibilityModelChanged)
    Q_INVOKABLE SlideVisibilityModel *visibilityModel();

    Q_INVOKABLE int addSlide();
    Q_INVOKABLE void removeSlide(int i);
    Q_INVOKABLE void moveSlideUp(int i);
    Q_INVOKABLE void moveSlideDown(int i);
    Q_INVOKABLE void updateSlide(int i);
    Q_INVOKABLE void updateSelectedSlide();
    Q_INVOKABLE void clearSlides();

    Q_INVOKABLE void copyLayer();
    Q_INVOKABLE void clearCopyLayer();
    Q_INVOKABLE bool copyIsAvailable();
    Q_INVOKABLE void pasteLayer();
    Q_INVOKABLE void pasteLayerAsProperties(int layerIdx);

    Q_PROPERTY(int slideToPaste
        READ getSlideToPasteIdx
        WRITE setSlideToPasteIdx
        NOTIFY slideToPasteIdxChanged)

    Q_INVOKABLE void setSlideToPasteIdx(int value);
    Q_INVOKABLE int getSlideToPasteIdx();

    Q_PROPERTY(bool slidesNeedsSave
                   READ getSlidesNeedsSave
                       WRITE setSlidesNeedsSave
                           NOTIFY slidesNeedsSaveChanged)

    Q_INVOKABLE void setSlidesNeedsSave(bool value);
    Q_INVOKABLE bool getSlidesNeedsSave();

    Q_PROPERTY(QString slidesName
                   READ getSlidesName
                       WRITE setSlidesName
                           NOTIFY slidesNameChanged)

    Q_INVOKABLE void setSlidesName(QString name);
    Q_INVOKABLE QString getSlidesName() const;

    Q_INVOKABLE void setSlidesPath(QString path);
    Q_INVOKABLE QString getSlidesPath() const;
    Q_INVOKABLE QUrl getSlidesPathAsURL() const;

    Q_INVOKABLE QString makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider);
    Q_INVOKABLE void loadFromJSONFile(const QString &path);
    Q_INVOKABLE void saveAsJSONFile(const QString &path);

Q_SIGNALS:
    void selectedSlideChanged();
    void triggeredSlideVisibilityChanged();
    void slidesNeedsSaveChanged();
    void triggeredSlideChanged();
    void visibilityModelChanged();
    void needsSyncChanged();
    void slidesNameChanged();
    void slideToPasteIdxChanged();
    void copyCleared();
    void previousSlide();
    void nextSlide();

private:
    QList<LayersModel *> m_slides;
    LayersModel *m_masterSlide;
    SlideVisibilityModel* m_visibilityModel;
    BaseLayer *m_layerToCopyFrom;
    int m_selectedSlideIdx = -1; // Means master
    int m_previousSelectedSlideIdx = -1;
    int m_triggeredSlideIdx = -1;
    int m_previousTriggeredSlideIdx = -1;
    int m_slideToPasteIdx = -2;
    int m_slidesNeedsSave = false;
    bool m_needsSync;
    QString m_slidesName;
    QString m_slidesPath;
    QTimer* m_clearCopyTimer;
};

#endif // SLIDESMODEL_H
