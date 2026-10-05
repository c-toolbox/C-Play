/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tcpcontrolclient.h"

#include <QStringDecoder>
#include <QtEndian>
#include <algorithm>

TcpControlClient::TcpControlClient(QObject *parent) : QObject(parent) {
    for (auto *timer : { &m_connectTimer, &m_retryTimer, &m_stableTimer, &m_stopTimer }) {
        timer->setParent(this);
        timer->setSingleShot(true);
    }
    connect(&m_connectTimer, &QTimer::timeout, this, [this] {
        failed(tr("TCP connection timed out"));
    });
    connect(&m_retryTimer, &QTimer::timeout, this, &TcpControlClient::connectSocket);
    connect(&m_stableTimer, &QTimer::timeout, this, [this] {
        m_retryDelayMs = m_options.retryInitialMs;
    });
    connect(&m_stopTimer, &QTimer::timeout, this, [this] {
        releaseSocket();
        setState(Stopped);
    });
}

TcpControlClient::~TcpControlClient() {
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
}

bool TcpControlClient::validate(const Options &o) {
    if (o.host.trimmed().isEmpty() || o.port < 1 || o.port > 65535
        || o.connectTimeoutMs < 1 || o.retryInitialMs < 1
        || o.retryMaximumMs < o.retryInitialMs || o.stableConnectionMs < 1
        || o.maximumMessageBytes < 1 || o.maximumMessageBytes > 64 * 1024 * 1024
        || o.maximumPendingWriteBytes < 1 || o.maximumPendingWriteBytes > 64 * 1024 * 1024
        || (o.framing != Delimiter && o.framing != LengthPrefix)
        || (o.framing == Delimiter && (o.receiveTerminator.isEmpty()
            || o.receiveTerminator.size() > 256 || o.sendTerminator.size() > 256))) {
        reportError(tr("Invalid TCP endpoint, framing, timeout, or buffer limits"));
        return false;
    }
    return true;
}

bool TcpControlClient::configure(const Options &options) {
    if (!validate(options)) {
        m_running = false;
        releaseSocket();
        m_options = options;
        setState(Stopped);
        return false;
    }
    if (options == m_options)
        return true;
    const bool restart = m_running;
    m_running = false;
    releaseSocket();
    m_options = options;
    m_retryDelayMs = options.retryInitialMs;
    setState(Stopped);
    if (restart)
        start();
    return true;
}

void TcpControlClient::start() {
    if (m_running || !validate(m_options))
        return;
    releaseSocket();
    m_running = true;
    m_retryDelayMs = m_options.retryInitialMs;
    connectSocket();
}

void TcpControlClient::stop() {
    m_running = false;
    m_retryTimer.stop();
    m_connectTimer.stop();
    m_stableTimer.stop();
    if (!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) {
        releaseSocket();
        setState(Stopped);
        return;
    }
    m_stopTimer.start(m_options.connectTimeoutMs);
    const auto session = m_session;
    setState(Disconnecting);
    if (m_socket && session == m_session)
        m_socket->disconnectFromHost();
}

void TcpControlClient::releaseSocket() {
    ++m_session;
    m_connectTimer.stop();
    m_retryTimer.stop();
    m_stableTimer.stop();
    m_stopTimer.stop();
    m_receiveBuffer.clear();
    if (m_socket) {
        auto *socket = m_socket;
        m_socket = nullptr;
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
}

void TcpControlClient::connectSocket() {
    if (!m_running)
        return;
    releaseSocket();
    m_socket = new QTcpSocket(this);
    m_socket->setReadBufferSize(m_options.maximumMessageBytes + 260LL);
    connect(m_socket, &QTcpSocket::connected, this, [this] {
        const auto session = m_session;
        m_connectTimer.stop();
        m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
        clearError();
        if (!m_socket || !m_running || session != m_session)
            return;
        m_stableTimer.start(m_options.stableConnectionMs);
        setState(Connected);
    });
    connect(m_socket, &QTcpSocket::readyRead, this, &TcpControlClient::receive);
    connect(m_socket, &QTcpSocket::bytesWritten, this, &TcpControlClient::bytesWritten);
    connect(m_socket, &QTcpSocket::disconnected, this, [this] {
        if (m_running)
            failed(tr("TCP peer disconnected"));
        else {
            releaseSocket();
            setState(Stopped);
        }
    });
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        // Qt emits RemoteHostClosedError before disconnected. Drain any final
        // complete frames, then let disconnected schedule the single retry.
        if (error == QAbstractSocket::RemoteHostClosedError) {
            receive();
            return;
        }
        failed(m_socket->errorString());
    });
    m_connectTimer.start(m_options.connectTimeoutMs);
    const auto session = m_session;
    setState(Connecting);
    if (m_socket && m_running && session == m_session)
        m_socket->connectToHost(m_options.host.trimmed(), quint16(m_options.port));
}

void TcpControlClient::failed(const QString &reason) {
    releaseSocket();
    if (m_running && m_options.autoReconnect) {
        const int delay = m_retryDelayMs;
        m_retryDelayMs = int(std::min(qint64(m_options.retryMaximumMs), qint64(delay) * 2));
        m_retryTimer.start(delay);
        const auto session = m_session;
        setState(WaitingToReconnect);
        if (m_running && session == m_session)
            Q_EMIT reconnectScheduled(delay);
    } else {
        m_running = false;
        setState(Stopped);
    }
    reportError(reason);
}

void TcpControlClient::receive() {
    if (!m_socket || !m_running)
        return;
    const auto session = m_session;
    // Bound each read as well as the retained incomplete frame. Coalesced valid
    // frames may total more than the per-message limit.
    while (m_socket && session == m_session && m_running && m_socket->bytesAvailable() > 0) {
        m_receiveBuffer.append(m_socket->read(std::min<qint64>(65536, m_socket->bytesAvailable())));
        while (session == m_session && m_running) {
            qsizetype payloadSize = 0;
            qsizetype prefixSize = 0;
            qsizetype suffixSize = 0;
            if (m_options.framing == LengthPrefix) {
                if (m_receiveBuffer.size() < 4)
                    break;
                payloadSize = qFromBigEndian<quint32>(m_receiveBuffer.constData());
                prefixSize = 4;
            } else {
                payloadSize = m_receiveBuffer.indexOf(m_options.receiveTerminator);
                suffixSize = m_options.receiveTerminator.size();
                if (payloadSize < 0) {
                    // Permit a partial multi-byte delimiter after a maximum-size payload.
                    if (m_receiveBuffer.size() > m_options.maximumMessageBytes + suffixSize - 1)
                        failed(tr("TCP message exceeds the configured limit"));
                    break;
                }
            }
            if (payloadSize > m_options.maximumMessageBytes) {
                failed(tr("TCP message exceeds the configured limit"));
                return;
            }
            const qsizetype frameSize = prefixSize + payloadSize + suffixSize;
            if (m_receiveBuffer.size() < frameSize)
                break;
            const QByteArray payload = m_receiveBuffer.mid(prefixSize, payloadSize);
            m_receiveBuffer.remove(0, frameSize);
            Q_EMIT messageReceived(payload);
            // A receiver may stop or reconfigure the transport synchronously.
            if (session != m_session || !m_running)
                return;
            QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
            const QString text = decoder.decode(payload);
            if (!decoder.hasError())
                Q_EMIT textCommandReceived(text);
        }
    }
}

bool TcpControlClient::sendBinary(const QByteArray &command) {
    if (!isConnected() || !m_socket) {
        reportError(tr("TCP command rejected: client is disconnected"));
        return false;
    }
    if (command.size() > m_options.maximumPendingWriteBytes - m_socket->bytesToWrite()) {
        reportError(tr("TCP command rejected: pending write limit exceeded"));
        return false;
    }
    const qint64 written = m_socket->write(command);
    if (written != command.size()) {
        failed(tr("TCP write failed; command delivery is unknown"));
        return false;
    }
    return true; // Accepted by Qt, not acknowledged by the remote application.
}

bool TcpControlClient::sendMessage(const QByteArray &payload) {
    if (payload.size() > m_options.maximumMessageBytes) {
        reportError(tr("TCP command rejected: message limit exceeded"));
        return false;
    }
    QByteArray frame;
    if (m_options.framing == LengthPrefix) {
        frame.resize(4);
        qToBigEndian<quint32>(quint32(payload.size()), frame.data());
        frame.append(payload);
    } else {
        frame = payload + m_options.sendTerminator;
    }
    return sendBinary(frame);
}

bool TcpControlClient::sendText(const QString &command) {
    return sendMessage(command.toUtf8());
}

void TcpControlClient::setState(ConnectionState state) {
    if (state == m_state)
        return;
    const bool wasConnected = isConnected();
    m_state = state;
    Q_EMIT connectionStateChanged(state);
    if (wasConnected != isConnected())
        Q_EMIT connectedChanged(isConnected());
}

QString TcpControlClient::stateText() const {
    switch (m_state) {
    case Connecting: return tr("Connecting");
    case Connected: return tr("Connected");
    case WaitingToReconnect: return tr("Waiting to reconnect");
    case Disconnecting: return tr("Disconnecting");
    case Stopped: return tr("Stopped");
    }
    return {};
}

void TcpControlClient::reportError(const QString &reason) {
    m_lastError = reason;
    Q_EMIT lastErrorChanged();
    Q_EMIT errorOccurred(reason);
}

void TcpControlClient::clearError() {
    if (!m_lastError.isEmpty()) {
        m_lastError.clear();
        Q_EMIT lastErrorChanged();
    }
}
