/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef SLIDESQTITEM_H
#define SLIDESQTITEM_H

#include <QOpenGLFunctions>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>

class SlidesModel;

class SlidesQtItemRenderer : public QObject, protected QOpenGLFunctions {
    Q_OBJECT
public:
    ~SlidesQtItemRenderer();

    void setWindow(QQuickWindow *window);

    SlidesModel* slidesModel() const;
    void setSlidesModel(SlidesModel* sm);

    Q_INVOKABLE void init();
    Q_INVOKABLE void paint();

Q_SIGNALS:
    void viewChanged();

private:
    SlidesModel* m_slidesModel = nullptr;
    QQuickWindow *m_window = nullptr;
};

class SlidesQtItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

public:
    SlidesQtItem();

    Q_PROPERTY(SlidesModel* slides 
        READ slidesModel 
        WRITE setSlidesModel 
        NOTIFY slidesModelChanged)

    Q_INVOKABLE void sync();
    Q_INVOKABLE void cleanup();

Q_SIGNALS:
    void slidesModelChanged();

private:
    SlidesModel* slidesModel() const;
    void setSlidesModel(SlidesModel* sm);

    Q_INVOKABLE void handleWindowChanged(QQuickWindow *win);
    void releaseResources() override;

    SlidesQtItemRenderer *m_renderer = nullptr;
    QTimer *m_timer = nullptr;
};

#endif // SLIDESQTITEM_H
