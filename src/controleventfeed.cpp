/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "controleventfeed.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <algorithm>
#include <QWebSocketServer>
#include <QWebSocket>
#include <QUrlQuery>
#include <QJsonDocument>

ControlEventFeed::ControlEventFeed(QObject *parent) : QObject(parent) {
    m_webSocketServer = new QWebSocketServer(QStringLiteral("C-Play events"), QWebSocketServer::NonSecureMode, this);
    m_webSocketServer->setMaxPendingConnections(16);
    connect(m_webSocketServer, &QWebSocketServer::newConnection, this, [this] {
        auto *socket = m_webSocketServer->nextPendingConnection();
        socket->setParent(this);
        const QUrl url = socket->requestUrl();
        const QUrlQuery query(url);
        const QStringList topics = query.hasQueryItem(QStringLiteral("topics"))
            ? query.queryItemValue(QStringLiteral("topics")).split(QLatin1Char(',')) : QStringList{QStringLiteral("*")};
        connect(socket, &QWebSocket::disconnected, this, [this, socket] {
            m_webSockets.remove(socket);
            socket->deleteLater();
        });
        if (url.path() != QStringLiteral("/events") || !validateTopics(topics) || m_webSockets.size() >= 16) {
            socket->close(QWebSocketProtocol::CloseCodePolicyViolated, QStringLiteral("Invalid event subscription or client limit reached"));
            return;
        }
        m_webSockets.insert(socket, topics);
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(read(topics)).toJson(QJsonDocument::Compact)));
    });
    connect(this, &ControlEventFeed::eventPublished, this, [this](const QJsonObject &event) {
        const auto sockets = m_webSockets;
        for (auto it = sockets.begin(); it != sockets.end(); ++it) {
            if (!it.value().contains(QStringLiteral("*")) && !it.value().contains(event.value(QStringLiteral("topic")).toString())) continue;
            const auto bytes = QJsonDocument(event).toJson(QJsonDocument::Compact);
            if (it.key()->bytesToWrite() + bytes.size() > 1048576)
                it.key()->close(QWebSocketProtocol::CloseCodeTooMuchData, QStringLiteral("Event consumer is too slow"));
            else it.key()->sendTextMessage(QString::fromUtf8(bytes));
        }
    });
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, &ControlEventFeed::sample);
}

QStringList ControlEventFeed::availableTopics() {
    return {QStringLiteral("position"), QStringLiteral("remaining"), QStringLiteral("duration"),
        QStringLiteral("pause"), QStringLiteral("auto_play"), QStringLiteral("speed"),
        QStringLiteral("volume"), QStringLiteral("mute"), QStringLiteral("visibility"),
        QStringLiteral("sync_image_volume_fade"), QStringLiteral("stereo_mode"),
        QStringLiteral("grid_mode"), QStringLiteral("eof_mode"), QStringLiteral("loop_mode"),
        QStringLiteral("media_title"), QStringLiteral("audiotracks"), QStringLiteral("playing_in_audiotracks"),
        QStringLiteral("playlist"), QStringLiteral("playing_in_playlist"),
        QStringLiteral("sections"), QStringLiteral("playing_in_sections"),
        QStringLiteral("slides"), QStringLiteral("slide_name"), QStringLiteral("playing_in_slides"),
        QStringLiteral("triggered_slide"), QStringLiteral("layers"), QStringLiteral("main_video_load")
#ifdef WEB_VIEWS_SUPPORT
        , QStringLiteral("presentation"), QStringLiteral("movie")
#endif
    };
}

bool ControlEventFeed::validateTopics(const QStringList &topics) {
    const auto available = availableTopics();
    return std::all_of(topics.begin(), topics.end(), [&available](const QString &topic) {
        return topic == QStringLiteral("*") || available.contains(topic);
    });
}

void ControlEventFeed::addTopic(const QString &topic, std::function<QJsonValue()> reader) {
    Q_ASSERT(availableTopics().contains(topic));
    m_readers[topic] = std::move(reader);
}

void ControlEventFeed::setActive(bool active) {
    if (active && !m_timer.isActive()) {
        // Establish a baseline without firing automation when the player starts.
        QMutexLocker lock(&m_mutex);
        for (const auto &[topic, reader] : m_readers) m_values.insert(topic, reader());
        m_stopped = false;
        m_timer.start();
        if (m_webSocketPort && !m_webSocketServer->listen(QHostAddress::Any, m_webSocketPort))
            qWarning().noquote() << "Cannot start event WebSocket server:" << m_webSocketServer->errorString();
    } else if (!active) {
        m_timer.stop();
        m_webSocketServer->close();
        const auto sockets = m_webSockets.keys();
        for (auto *socket : sockets) socket->close();
    }
}

void ControlEventFeed::sample() {
    QList<QJsonObject> events;
    {
        QMutexLocker lock(&m_mutex);
        for (const auto &[topic, reader] : m_readers) {
            const auto value = reader();
            if (m_values.contains(topic) && m_values.value(topic) == value) continue;
            QJsonObject event{{QStringLiteral("type"), QStringLiteral("change")},
                {QStringLiteral("topic"), topic}, {QStringLiteral("value"), value},
                {QStringLiteral("previous"), m_values.value(topic)},
                {QStringLiteral("sequence"), QString::number(++m_sequence)},
                {QStringLiteral("timestamp"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}};
            m_values.insert(topic, value);
            m_history.append(event);
            if (m_history.size() > 512) m_history.removeAt(0);
            events.append(event);
        }
        if (!events.isEmpty()) m_changed.wakeAll();
    }
    for (const auto &event : events) Q_EMIT eventPublished(event);
}

QJsonObject ControlEventFeed::read(const QStringList &topics, const QString &since, int waitMs) {
    const auto matches = [&topics](const QString &topic) {
        return topics.contains(QStringLiteral("*")) || topics.contains(topic);
    };
    QMutexLocker lock(&m_mutex);
    const quint64 cursor = since.toULongLong();
    QElapsedTimer elapsed;
    elapsed.start();
    waitMs = std::clamp(waitMs, 0, 25000);
    QJsonArray events;
    bool reset = false;
    do {
        reset = !since.isEmpty() && (cursor > m_sequence || (!m_history.isEmpty()
            && cursor + 1 < m_history.first().toObject().value(QStringLiteral("sequence")).toString().toULongLong()));
        events = {};
        if (!since.isEmpty() && !reset) {
            for (const auto &item : m_history) {
                const auto event = item.toObject();
                if (event.value(QStringLiteral("sequence")).toString().toULongLong() > cursor
                    && matches(event.value(QStringLiteral("topic")).toString())) events.append(event);
            }
        }
        const qint64 remaining = waitMs - elapsed.elapsed();
        if (since.isEmpty() || reset || !events.isEmpty() || m_stopped || remaining <= 0) break;
        m_changed.wait(&m_mutex, static_cast<unsigned long>(remaining));
    } while (true);
    QJsonObject values;
    if (since.isEmpty() || reset) {
        for (auto it = m_values.begin(); it != m_values.end(); ++it)
            if (matches(it.key())) values.insert(it.key(), it.value());
    }
    return {{QStringLiteral("type"), since.isEmpty() || reset ? QStringLiteral("snapshot") : QStringLiteral("events")},
        {QStringLiteral("sequence"), QString::number(m_sequence)}, {QStringLiteral("values"), values},
        {QStringLiteral("events"), events}, {QStringLiteral("reset"), reset}};
}

void ControlEventFeed::stop() {
    setActive(false);
    QMutexLocker lock(&m_mutex);
    m_stopped = true;
    m_changed.wakeAll();
}

void ControlEventFeed::setWebSocketPort(quint16 port) {
    m_webSocketPort = port;
}
