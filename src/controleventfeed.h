/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef CONTROLEVENTFEED_H
#define CONTROLEVENTFEED_H
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <functional>
#include <map>
class QWebSocketServer;
class QWebSocket;

// Readers and sampling run on the Qt thread. Network workers read only the
// cached values and bounded journal; they never access player/model objects.
class ControlEventFeed : public QObject {
    Q_OBJECT
public:
    explicit ControlEventFeed(QObject *parent = nullptr);
    void addTopic(const QString &topic, std::function<QJsonValue()> reader);
    void setActive(bool active);
    void sample();
    void stop();
    void setWebSocketPort(quint16 port);
    static QStringList availableTopics();
    static bool validateTopics(const QStringList &topics);
    // An empty cursor gives an initial snapshot. A cursor enables long polling.
    QJsonObject read(const QStringList &topics, const QString &since = {}, int waitMs = 0);
Q_SIGNALS:
    void eventPublished(const QJsonObject &event);
private:
    QTimer m_timer;
    std::map<QString, std::function<QJsonValue()>> m_readers;
    QMutex m_mutex;
    QWaitCondition m_changed;
    QJsonObject m_values;
    QJsonArray m_history;
    quint64 m_sequence = 0;
    bool m_stopped = false;
    quint16 m_webSocketPort = 0;
    QWebSocketServer *m_webSocketServer;
    QHash<QWebSocket *, QStringList> m_webSockets;
};
#endif
