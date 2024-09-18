/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SLIDESMODEL_H
#define SLIDESMODEL_H

#include <QAbstractTableModel>

class LayersModel;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#ifndef OPAQUE_PTR_LayersModel
#define OPAQUE_PTR_SLayersModel
Q_DECLARE_OPAQUE_POINTER(LayersModel *)
#endif
#endif

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

    Q_INVOKABLE int addSlide();
    Q_INVOKABLE void removeSlide(int i);
    Q_INVOKABLE void moveSlideUp(int i);
    Q_INVOKABLE void moveSlideDown(int i);
    Q_INVOKABLE void updateSlide(int i);
    Q_INVOKABLE void updateSelectedSlide();
    Q_INVOKABLE void clearSlides();

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
    void needsSyncChanged();
    void slidesNameChanged();

private:
    QList<LayersModel *> m_slides;
    LayersModel *m_masterSlide;
    int m_selectedSlideIdx = -1; // Means master
    int m_previousSelectedSlideIdx = -2;
    int m_triggeredSlideIdx = -2;
    int m_previousTriggeredSlideIdx = -2;
    int m_slidesNeedsSave = false;
    bool m_needsSync;
    QString m_slidesName;
    QString m_slidesPath;
};

#endif // SLIDESMODEL_H
