/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NDISENDERMODEL_H
#define NDISENDERMODEL_H

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

class MpvObject;
class NdiSender;
class LayersRendererQtItem;

/**
 * QML facing controller for the NDI output.
 *
 * This class is always compiled, also when the application is built without
 * NDI support, so that the QML bindings never break. In that case available()
 * returns false and enabling the output is a no-op.
 */
class NdiSenderModel : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool sending READ sending NOTIFY sendingChanged)
    Q_PROPERTY(QString senderName READ senderName WRITE setSenderName NOTIFY senderNameChanged)
    Q_PROPERTY(int width READ width NOTIFY resolutionChanged)
    Q_PROPERTY(int height READ height NOTIFY resolutionChanged)
    // Mirrors window.mainViewMode: 0 renders the main video, 1 and 2 the 3D view with a
    // perspective respectively fisheye camera. Selects which source is published.
    Q_PROPERTY(int mainViewMode READ mainViewMode WRITE setMainViewMode NOTIFY mainViewModeChanged)

public:
    explicit NdiSenderModel(QObject *parent = nullptr);
    ~NdiSenderModel();

    static NdiSenderModel *instance();

    bool available() const;

    bool enabled() const;
    void setEnabled(bool enabled);

    bool sending() const;

    QString senderName() const;
    void setSenderName(const QString &name);

    int width() const;
    int height() const;

    int mainViewMode() const;
    void setMainViewMode(int mode);

    // True while the 3D view, not the main video, is the NDI source.
    bool capturesThreeDView() const;

    // Connects the output to the main player. Called once the MpvObject exists.
    void setMpvObject(MpvObject *mpv);

    // Connects the output to the 3D view. Called once the item exists.
    void setLayersRendererItem(LayersRendererQtItem *renderer);

    // Called from the render thread, with the OpenGL context of the source
    // current, once per presented frame. Each entry point is a no-op unless the
    // matching source is the active one.
    void renderFrameFromMpv();
    void renderFrameFrom3D();

    // Called from the render thread when the OpenGL context goes away.
    void cleanupGL();

Q_SIGNALS:
    void enabledChanged();
    void sendingChanged();
    void senderNameChanged();
    void resolutionChanged();
    void mainViewModeChanged();

private:
    // Binds the sender to the source matching the current view mode and tells the 3D view
    // whether it has to render into its capture target.
    void updateSource();
    // Shared body of the two per-frame entry points.
    void captureFrame();

    std::unique_ptr<NdiSender> m_sender;
    MpvObject *m_mpv = nullptr;
    LayersRendererQtItem *m_layersRenderer = nullptr;
    QString m_senderName;
    std::atomic_bool m_enabled = false;
    std::atomic_int m_mainViewMode = 0;
    bool m_lastSending = false;
    int m_lastWidth = 0;
    int m_lastHeight = 0;

    static NdiSenderModel *_instance;
};

#endif // NDISENDERMODEL_H
