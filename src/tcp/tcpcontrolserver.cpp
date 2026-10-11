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
        }
    }
    connect(&m_server, &QTcpServer::newConnection, this, [this] {
        while (auto *socket = m_server.nextPendingConnection()) {
            if (m_buffers.size() >= MaximumClients) { socket->abort(); socket->deleteLater(); continue; }
            socket->setReadBufferSize(MaximumMessageBytes + 1);
            m_buffers.insert(socket, {});
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] { receive(socket); });
            connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                m_buffers.remove(socket); socket->deleteLater(); Q_EMIT stateChanged();
            });
            Q_EMIT stateChanged();
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
    const auto sockets = m_buffers.keys();
    for (auto *socket : sockets) { socket->abort(); socket->deleteLater(); }
    m_buffers.clear();
    Q_EMIT stateChanged();
}

void TcpControlServer::receive(QTcpSocket *socket) {
    auto &buffer = m_buffers[socket];
    buffer += socket->read(MaximumMessageBytes + 1 - buffer.size());
    while (true) {
        const auto newline = buffer.indexOf('\n');
        if (newline < 0) break;
        QByteArray line = buffer.left(newline);
        buffer.remove(0, newline + 1);
        if (line.endsWith('\r')) line.chop(1);
        const auto reply = QJsonDocument(execute(line)).toJson(QJsonDocument::Compact) + '\n';
        // A callback can stop/reconfigure the listener, so do not touch its buffer afterwards.
        if (!m_buffers.contains(socket)) return;
        if (socket->bytesToWrite() + reply.size() > MaximumMessageBytes) { socket->abort(); return; }
        socket->write(reply);
    }
    if (buffer.size() > MaximumMessageBytes) { socket->abort(); return; }
    const QPointer<QTcpSocket> guarded(socket);
    if (socket->bytesAvailable()) QMetaObject::invokeMethod(this, [this, guarded] {
        if (guarded && m_buffers.contains(guarded.data())) receive(guarded.data());
    }, Qt::QueuedConnection);
}

QJsonObject TcpControlServer::execute(const QByteArray &line) {
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
    return m_handler ? m_handler(path, parameters) : failure(tr("Player is not ready"));
}
