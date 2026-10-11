/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TCPCONTROLMANAGER_H
#define TCPCONTROLMANAGER_H
#include "tcpcontrolclient.h"
#include "tcpcontrolserver.h"
#include <QHash>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonObject>
#include <QPointer>
class ControlEventFeed;

// Servers are identified by host:port (IPv6 uses [address]:port).
// Commands keep stable IDs; endpoint edits remap their server references.
class TcpControlManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(TcpControlServer *listener READ listener CONSTANT)
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(QVariantList commands READ commands NOTIFY commandsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString configurationPath READ configurationPath CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QStringList eventTopics READ eventTopics CONSTANT)
public:
    explicit TcpControlManager(QObject *parent = nullptr, const QString &configurationPath = {});
    TcpControlServer *listener() const { return m_listener; }
    QVariantList servers() const;
    QVariantList commands() const { return m_commands; }
    QString lastError() const { return m_lastError; }
    QString configurationPath() const { return m_configurationPath; }
    bool active() const { return m_active; }
    void setActive(bool active);
    QStringList eventTopics() const;
    void setEventFeed(ControlEventFeed *feed);
    bool loadConfiguration(const QString &sourcePath = {});
    TcpControlClient *clientForServer(const QString &id) const { return m_clients.value(id); }

    // Server IDs derive from the endpoint; a supplied old ID selects the profile
    // to edit. Command IDs remain UUIDs when not explicitly provided.
    Q_INVOKABLE QString saveServer(const QVariantMap &profile);
    Q_INVOKABLE bool removeServer(const QString &id);
    Q_INVOKABLE QString saveCommand(const QVariantMap &command);
    Q_INVOKABLE bool removeCommand(const QString &id);
    Q_INVOKABLE bool triggerCommand(const QString &id);
    Q_INVOKABLE QVariantMap triggerAll(const QString &serverId);
    Q_INVOKABLE bool setCommandCharsPerItem(const QString &id, int charsPerItem);
    Q_INVOKABLE bool importEventModel(const QString &serverId, const QString &path = QStringLiteral("./data/EventModel.json"));
    Q_INVOKABLE bool connectServer(const QString &id);
    Q_INVOKABLE bool disconnectServer(const QString &id);
    Q_INVOKABLE QVariantMap command(const QString &id) const;
    Q_INVOKABLE QString commandDescription(const QString &id) const;

Q_SIGNALS:
    void serversChanged();
    void commandsChanged();
    void lastErrorChanged();
    void activeChanged();
    void messageReceived(const QString &serverId, const QByteArray &payload);
    void controlMessageReceived(const QString &serverId, const QByteArray &payload);
    void connectionStateChanged(const QString &serverId, TcpControlClient::ConnectionState state);
    void commandTriggered(const QString &commandId, bool accepted);
    void serverError(const QString &serverId, const QString &description);

private:
    static int find(const QVariantList &list, const QString &id);
    bool normalizeServer(const QVariantMap &input, QVariantMap &output);
    bool normalizeCommand(const QVariantMap &input, const QVariantList &servers, QVariantMap &output);
    static TcpControlClient::Options options(const QVariantMap &server);
    bool persist(const QVariantList &servers, const QVariantList &commands);
    void reconcile();
    void error(const QString &description);
    void publishEvent(const QJsonObject &event);
    bool dispatchCommand(const QString &id, const QJsonObject &event);
    void sendEventMessage(const QString &serverId, const QJsonObject &message);
    bool handleEventRequest(const QString &serverId, const QByteArray &payload);

    TcpControlServer *m_listener;
    QString m_configurationPath;
    QString m_lastError;
    QVariantList m_servers;
    QVariantList m_commands;
    QHash<QString, TcpControlClient *> m_clients;
    QSet<QString> m_manualStops;
    QHash<QString, QStringList> m_subscriptions;
    QPointer<ControlEventFeed> m_eventFeed;
    bool m_active = false;
};
#endif
