/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ndisendermodel.h"
#include "ndisender.h"

#include <layersrendererqtitem.h>
#include <mpvobject.h>

NdiSenderModel *NdiSenderModel::_instance = nullptr;

NdiSenderModel::NdiSenderModel(QObject *parent)
    : QObject(parent),
      m_sender(std::make_unique<NdiSender>()),
      m_senderName(QStringLiteral("C-Play")) {
    if (!_instance)
        _instance = this;
}

NdiSenderModel::~NdiSenderModel() {
    if (_instance == this)
        _instance = nullptr;
}

NdiSenderModel *NdiSenderModel::instance() {
    return _instance;
}

bool NdiSenderModel::available() const {
    return NdiSender::isSupported();
}

bool NdiSenderModel::enabled() const {
    return m_enabled;
}

void NdiSenderModel::setEnabled(bool enabled) {
    if (m_enabled == enabled)
        return;

    if (enabled && !available())
        return;

    m_enabled = enabled;

    if (m_enabled) {
        updateSource();
    } else {
        // The actual sender and the OpenGL resources are released on the
        // render thread in renderFrame/cleanupGL, stop only flags the intent.
        m_sender->stop();
        if (m_layersRenderer)
            m_layersRenderer->setNdiCaptureEnabled(false);
        if (m_lastSending) {
            m_lastSending = false;
            Q_EMIT sendingChanged();
        }
        if (!m_ndiName.isEmpty()) {
            m_ndiName.clear();
            Q_EMIT ndiNameChanged();
        }
        if (m_lastWidth != 0 || m_lastHeight != 0) {
            m_lastWidth = 0;
            m_lastHeight = 0;
            Q_EMIT resolutionChanged();
        }
    }

    Q_EMIT enabledChanged();
}

bool NdiSenderModel::sending() const {
    return m_sender->isSending();
}

QString NdiSenderModel::senderName() const {
    return m_senderName;
}

void NdiSenderModel::setSenderName(const QString &name) {
    if (m_senderName == name || name.isEmpty())
        return;

    m_senderName = name;
    Q_EMIT senderNameChanged();

    // Re-create the sender under the new name if it is currently running.
    if (m_enabled) {
        setEnabled(false);
        setEnabled(true);
    }
}

QString NdiSenderModel::ndiName() const {
    return m_ndiName;
}

int NdiSenderModel::width() const {
    return m_sender->width();
}

int NdiSenderModel::height() const {
    return m_sender->height();
}

int NdiSenderModel::mainViewMode() const {
    return m_mainViewMode;
}

void NdiSenderModel::setMainViewMode(int mode) {
    if (m_mainViewMode == mode)
        return;

    m_mainViewMode = mode;
    Q_EMIT mainViewModeChanged();

    // Switch between the main video and the 3D view without interrupting the sender.
    if (m_enabled)
        updateSource();
}

bool NdiSenderModel::capturesThreeDView() const {
    return m_mainViewMode > 0 && m_layersRenderer != nullptr;
}

void NdiSenderModel::updateSource() {
    const bool captureThreeD = capturesThreeDView();

    if (m_layersRenderer)
        m_layersRenderer->setNdiCaptureEnabled(m_enabled && captureThreeD);

    if (!m_enabled)
        return;

    if (captureThreeD)
        m_sender->setSource(NdiSender::sourceFromLayersRenderer(m_layersRenderer));
    else
        m_sender->setSource(NdiSender::sourceFromMpvObject(m_mpv));

    m_sender->start(m_senderName.toStdString());
}

void NdiSenderModel::setMpvObject(MpvObject *mpv) {
    m_mpv = mpv;

    if (m_enabled)
        updateSource();
}

void NdiSenderModel::setLayersRendererItem(LayersRendererQtItem *renderer) {
    if (m_layersRenderer == renderer)
        return;

    if (m_layersRenderer)
        m_layersRenderer->setNdiCaptureEnabled(false);

    m_layersRenderer = renderer;

    updateSource();
}

void NdiSenderModel::renderFrameFromMpv() {
    if (m_enabled && capturesThreeDView())
        return;

    captureFrame();
}

void NdiSenderModel::renderFrameFrom3D() {
    if (!m_enabled || !capturesThreeDView())
        return;

    captureFrame();
}

void NdiSenderModel::captureFrame() {
    if (!m_enabled) {
        // Release the sender and the PBOs while we still have a context.
        if (m_sender->isSending() || m_sender->width() != 0)
            m_sender->cleanupGL();

        if (m_lastSending) {
            m_lastSending = false;
            Q_EMIT sendingChanged();
        }
        if (!m_ndiName.isEmpty()) {
            m_ndiName.clear();
            Q_EMIT ndiNameChanged();
        }
        return;
    }

    m_sender->captureAndSend();

    const bool isSending = m_sender->isSending();
    if (isSending != m_lastSending) {
        m_lastSending = isSending;
        Q_EMIT sendingChanged();
    }

    const QString ndiName = QString::fromStdString(m_sender->ndiName());
    if (ndiName != m_ndiName) {
        m_ndiName = ndiName;
        Q_EMIT ndiNameChanged();
    }

    if (m_sender->width() != m_lastWidth || m_sender->height() != m_lastHeight) {
        m_lastWidth = m_sender->width();
        m_lastHeight = m_sender->height();
        Q_EMIT resolutionChanged();
    }
}

void NdiSenderModel::cleanupGL() {
    m_sender->cleanupGL();
}
