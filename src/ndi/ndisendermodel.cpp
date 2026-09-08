/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ndisendermodel.h"
#include "ndisender.h"

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
        m_sender->setSource(NdiSender::sourceFromMpvObject(m_mpv));
        m_sender->start(m_senderName.toStdString());
    } else {
        // The actual sender and the OpenGL resources are released on the
        // render thread in renderFrame/cleanupGL, stop only flags the intent.
        m_sender->stop();
        if (m_lastSending) {
            m_lastSending = false;
            Q_EMIT sendingChanged();
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

int NdiSenderModel::width() const {
    return m_sender->width();
}

int NdiSenderModel::height() const {
    return m_sender->height();
}

void NdiSenderModel::setMpvObject(MpvObject *mpv) {
    m_mpv = mpv;

    if (m_enabled)
        m_sender->setSource(NdiSender::sourceFromMpvObject(m_mpv));
}

void NdiSenderModel::renderFrame() {
    if (!m_enabled) {
        // Release the sender and the PBOs while we still have a context.
        if (m_sender->isSending() || m_sender->width() != 0)
            m_sender->cleanupGL();

        if (m_lastSending) {
            m_lastSending = false;
            Q_EMIT sendingChanged();
        }
        return;
    }

    m_sender->captureAndSend();

    const bool isSending = m_sender->isSending();
    if (isSending != m_lastSending) {
        m_lastSending = isSending;
        Q_EMIT sendingChanged();
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
