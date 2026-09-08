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

    // Connects the output to the main player. Called once the MpvObject exists.
    void setMpvObject(MpvObject *mpv);

    // Called from the render thread, with the OpenGL context of the source
    // current, once per presented frame.
    void renderFrame();

    // Called from the render thread when the OpenGL context goes away.
    void cleanupGL();

Q_SIGNALS:
    void enabledChanged();
    void sendingChanged();
    void senderNameChanged();
    void resolutionChanged();

private:
    std::unique_ptr<NdiSender> m_sender;
    MpvObject *m_mpv = nullptr;
    QString m_senderName;
    std::atomic_bool m_enabled = false;
    bool m_lastSending = false;
    int m_lastWidth = 0;
    int m_lastHeight = 0;

    static NdiSenderModel *_instance;
};

#endif // NDISENDERMODEL_H
