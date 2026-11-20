/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "tcpcontrolserver.h"
#include <QTcpSocket>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHostAddress>
#include <QStringDecoder>
#include <QUrl>
#include <QUrlQuery>
#include <QPointer>

namespace {
QJsonObject failure(const QString &message) {
    return {{QStringLiteral("accepted"), false}, {QStringLiteral("message"), message}};
}
}

TcpControlServer::TcpControlServer(QObject *parent) : QObject(parent) {
    QFile catalog(QStringLiteral(":/tcp-control/actions.json"));
    if (catalog.open(QIODevice::ReadOnly)) {
        for (const auto &entry : QJsonDocument::fromJson(catalog.readAll()).object()
                .value(QStringLiteral("commands")).toArray()) {
            const auto action = entry.toObject();
            m_actions.insert(action.value(QStringLiteral("path")).toString(), action);
            // Former paths of renamed actions keep working and run the action under its current path.
            for (const auto &alias : action.value(QStringLiteral("aliases")).toArray())
                m_actions.insert(alias.toString(), action);
        }
    }
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (auto *socket = m_server.nextPendingConnection()) {
            if (m_sessions.size() >= MaximumClients) { socket->abort(); socket->deleteLater(); continue; }
            socket->setReadBufferSize(MaximumMessageBytes + 1);
            Session session;
            session.id = ++m_nextSession;
            m_sessions.insert(socket, session);
            m_sockets.insert(session.id, socket);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { receive(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] { drop(socket); });
            QHostAddress peer = socket->peerAddress();
            bool ipv4 = false;
            const quint32 address = peer.toIPv4Address(&ipv4);
            if (ipv4) peer = QHostAddress(address);
            peer.setScopeId({});
            Q_EMIT stateChanged();
            Q_EMIT clientConnected(session.id, peer.toString().toLower(), socket->peerPort());
        }
    });
}

bool TcpControlServer::start(const QString &address, int requestedPort) {
    QHostAddress host;
    if (!host.setAddress(address) || requestedPort < 1 || requestedPort > 65535 || m_actions.isEmpty()) {
        stop();
        m_lastError = tr("TCP listener requires a numeric bind address, port 1-65535 and action catalog");
        Q_EMIT stateChanged(); return false;
    }
    if (listening() && m_server.serverAddress() == host && port() == requestedPort) return true;
    stop();
    const bool ok = m_server.listen(host, static_cast<quint16>(requestedPort));
    m_lastError = ok ? QString() : m_server.errorString();
    Q_EMIT stateChanged(); return ok;
}

void TcpControlServer::stop() {
    m_server.close();
    const auto sockets = m_sessions.keys();
    for (auto *socket : sockets) drop(socket);
    Q_EMIT stateChanged();
}

void TcpControlServer::drop(QTcpSocket *socket) {
    const auto it = m_sessions.find(socket);
    if (it == m_sessions.end()) return;
    const quint64 id = it->id;
    m_sessions.erase(it);
    m_sockets.remove(id);
    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
    Q_EMIT clientDisconnected(id);
    Q_EMIT stateChanged();
}

bool TcpControlServer::send(quint64 session, const QByteArray &payload, bool raw) {
    auto *socket = m_sockets.value(session);
    const auto it = m_sessions.constFind(socket);
    if (!socket || it == m_sessions.cend()) return false;
    const QByteArray frame = raw ? payload : payload + it->terminator;
    if (socket->bytesToWrite() + frame.size() > MaximumPendingWriteBytes || socket->write(frame) != frame.size()) {
        drop(socket);
        return false;
    }
    return true;
}

void TcpControlServer::setTerminator(quint64 session, const QByteArray &terminator) {
    const auto it = m_sessions.find(m_sockets.value(session));
    if (it != m_sessions.end()) it->terminator = terminator;
}

void TcpControlServer::receive(QTcpSocket *socket) {
    auto it = m_sessions.find(socket);
    if (it == m_sessions.end()) return;
    const quint64 id = it->id;
    it->buffer += socket->read(MaximumMessageBytes + 1 - it->buffer.size());
    while (true) {
        // A callback can drop this client or stop the listener; look the session up again.
        it = m_sessions.find(socket);
        if (it == m_sessions.end()) return;
        const auto newline = it->buffer.indexOf('\n');
        if (newline < 0) break;
        QByteArray line = it->buffer.left(newline);
        it->buffer.remove(0, newline + 1);
        if (line.endsWith('\r')) line.chop(1);
        if (!send(id, QJsonDocument(execute(id, line)).toJson(QJsonDocument::Compact))) return;
    }
    if (it->buffer.size() > MaximumMessageBytes) { drop(socket); return; }
    const QPointer<QTcpSocket> guarded(socket);
    if (socket->bytesAvailable()) QMetaObject::invokeMethod(this, [this, guarded] {
        if (guarded && m_sessions.contains(guarded.data())) receive(guarded.data());
    }, Qt::QueuedConnection);
}

QJsonObject TcpControlServer::execute(quint64 session, const QByteArray &line) {
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    const QString text = decoder.decode(line);
    if (decoder.hasError()) return failure(tr("Command must be UTF-8"));
    QString path;
    QJsonObject parameters;
    if (text.startsWith(QStringLiteral("POST /"))) {
        const QUrl url(text.mid(5), QUrl::StrictMode);
        if (!url.isValid() || !url.isRelative() || url.hasFragment()) return failure(tr("Invalid action URL"));
        path = url.path();
        const auto items = QUrlQuery(url).queryItems(QUrl::FullyDecoded);
        for (const auto &item : items) {
            if (parameters.contains(item.first)) return failure(tr("Duplicate parameter"));
            parameters.insert(item.first, item.second);
        }
    } else {
        const auto document = QJsonDocument::fromJson(line);
        const auto object = document.object();
        if (!document.isObject() || !object.value(QStringLiteral("path")).isString()
            || (object.size() != 1 && object.size() != 2)
            || (object.size() == 2 && !object.value(QStringLiteral("parameters")).isObject()))
            return failure(tr("Expected path and optional parameters object, or POST /action?parameters"));
        path = object.value(QStringLiteral("path")).toString();
        parameters = object.value(QStringLiteral("parameters")).toObject();
    }
    if (!m_actions.contains(path)) return failure(tr("Unsupported action (information queries are unavailable)"));
    const auto action = m_actions.value(path);
    path = action.value(QStringLiteral("path")).toString();
    for (const auto &name : action.value(QStringLiteral("required")).toArray()) {
        if (!parameters.contains(name.toString()) || parameters.value(name.toString()).toString().isEmpty())
            return failure(tr("Missing action parameter: %1").arg(name.toString()));
    }
    const auto anyOf = action.value(QStringLiteral("anyOf")).toArray();
    if (!anyOf.isEmpty()) {
        bool found = false;
        for (const auto &name : anyOf) found = found || parameters.contains(name.toString());
        if (!found) return failure(tr("At least one plane property is required"));
    }
    for (auto it = parameters.begin(); it != parameters.end(); ++it) {
        if (!it.value().isString()) return failure(tr("Parameter values must be strings"));
    }
    if (m_sessionHandler) {
        if (auto reply = m_sessionHandler(session, path, parameters)) return *reply;
    }
    return m_handler ? m_handler(path, parameters) : failure(tr("Player is not ready"));
}
