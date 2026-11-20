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
#include <functional>
#include <optional>
class ControlEventFeed;

// Servers are identified by host:port (IPv6 uses [address]:port).
// Commands keep stable IDs; endpoint edits remap their server references.
// Incoming client profiles are identified by the client address (or * for any client);
// they select how subscribed updates are sent to clients of the TCP listener.
class TcpControlManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(TcpControlServer *listener READ listener CONSTANT)
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(QVariantList commands READ commands NOTIFY commandsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString configurationPath READ configurationPath CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(QStringList eventTopics READ eventTopics CONSTANT)
    Q_PROPERTY(QVariantList incomingClients READ incomingClients NOTIFY incomingClientsChanged)
    Q_PROPERTY(QVariantList incomingSessions READ incomingSessions NOTIFY incomingSessionsChanged)
public:
    explicit TcpControlManager(QObject *parent = nullptr, const QString &configurationPath = {});
    TcpControlServer *listener() const { return m_listener; }
    QVariantList servers() const;
    QVariantList commands() const { return m_commands; }
    QVariantList incomingClients() const { return m_incomingClients; }
    // Clients connected to the listener with their address, profile and subscribed topics.
    QVariantList incomingSessions() const;
    QString lastError() const { return m_lastError; }
    QString configurationPath() const { return m_configurationPath; }
    bool active() const { return m_active; }
    void setActive(bool active);
    QStringList eventTopics() const;
    void setEventFeed(ControlEventFeed *feed);
    bool loadConfiguration(const QString &sourcePath = {});
    TcpControlClient *clientForServer(const QString &id) const { return m_clients.value(id); }
    // Formats a list topic like the matching REST list endpoint; nullopt falls back to generic formatting.
    using ListFormatter = std::function<std::optional<QByteArray>(const QString &topic, int charsPerItem)>;
    void setListFormatter(ListFormatter formatter) { m_listFormatter = std::move(formatter); }
    // Formatted commands for `topic` are resent when `source` changes (layers follow triggered_slide).
    void addTopicDependency(const QString &topic, const QString &source) { m_topicDependencies[source].append(topic); }

    // Server IDs derive from the endpoint; a supplied old ID selects the profile
    // to edit. Command IDs remain UUIDs when not explicitly provided.
    Q_INVOKABLE QString saveServer(const QVariantMap &profile);
    Q_INVOKABLE bool removeServer(const QString &id);
    Q_INVOKABLE QString saveCommand(const QVariantMap &command);
    Q_INVOKABLE bool removeCommand(const QString &id);
    Q_INVOKABLE bool triggerCommand(const QString &id);
    Q_INVOKABLE QVariantMap triggerAll(const QString &serverId);
    Q_INVOKABLE bool setCommandCharsPerItem(const QString &id, int charsPerItem);
    // Empty sends seconds; otherwise a QTime format such as "hh:mm:ss" or "hh:mm:ss/zz".
    Q_INVOKABLE bool setCommandTimeFormat(const QString &id, const QString &timeFormat);
    // Topics whose value is a time in seconds and that accept a command timeFormat.
    static QStringList timeTopics();
    Q_INVOKABLE bool importEventModel(const QString &serverId, const QString &path = QStringLiteral("./data/EventModel.json"));
    Q_INVOKABLE bool connectServer(const QString &id);
    Q_INVOKABLE bool disconnectServer(const QString &id);
    Q_INVOKABLE QVariantMap command(const QString &id) const;
    Q_INVOKABLE QString commandDescription(const QString &id) const;
    // Profile IDs derive from the client address; a supplied old ID selects the profile to edit.
    Q_INVOKABLE QString saveIncomingClient(const QVariantMap &profile);
    Q_INVOKABLE bool removeIncomingClient(const QString &id);
    // Enabled profile for a client address: an exact address match, else the default profile.
    Q_INVOKABLE QVariantMap incomingClientProfile(const QString &address) const;
    // ID and address of the default profile, which always exists and cannot be removed.
    static QString defaultIncomingClientId() { return QStringLiteral("*"); }

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
    void incomingClientsChanged();
    void incomingSessionsChanged();

private:
    static int find(const QVariantList &list, const QString &id);
    bool normalizeServer(const QVariantMap &input, QVariantMap &output);
    bool normalizeCommand(const QVariantMap &input, const QVariantList &servers, QVariantMap &output);
    bool normalizeIncomingClient(const QVariantMap &input, const QVariantList &servers, QVariantMap &output);
    // Built-in default profile: JSON notifications with LF, no automatic subscription.
    static QVariantMap builtInIncomingClient();
    // Adds the built-in default profile first when the list has no * profile.
    static QVariantList withDefaultIncomingClient(QVariantList incomingClients);
    static TcpControlClient::Options options(const QVariantMap &server);
    bool persist(const QVariantList &servers, const QVariantList &commands) { return persist(servers, commands, m_incomingClients); }
    bool persist(const QVariantList &servers, const QVariantList &commands, const QVariantList &incomingClients);
    void reconcile();
    // Servers with connectOnStartup=false start stopped until connectServer().
    void resetManualStops();
    void error(const QString &description);
    void publishEvent(const QJsonObject &event);
    // Automatic value updates skip payloads already sent by that command. Explicit triggers
    // and full updates always send; fixed commands remain actions and are never deduplicated.
    bool dispatchCommand(const QString &id, const QJsonObject &event, bool skipUnchanged = false);
    void clearCommandPayloads(const QString &id = {});
    // Wire payload of a command for an event (an empty event reads the cached value); nullopt when
    // the value is unavailable.
    std::optional<QByteArray> commandPayload(const QVariantMap &item, const QJsonObject &event);
    void sendEventMessage(const QString &serverId, const QJsonObject &message);
    bool handleEventRequest(const QString &serverId, const QByteArray &payload);

    // Listener sessions: subscriptions and updates in the format of the client's profile.
    struct IncomingSession {
        QString address;
        int port = 0;
        QVariantMap profile;
        QStringList topics;
        QHash<QString, QByteArray> lastValuePayloads;
    };
    void applyIncomingProfiles();
    void applyIncomingProfile(quint64 session);
    std::optional<QJsonObject> handleIncomingRequest(quint64 session, const QString &path, const QJsonObject &parameters);
    void subscribeIncoming(quint64 session, const QStringList &topics);
    void sendIncomingUpdate(quint64 session, const QStringList &topics);
    void sendIncomingEvent(quint64 session, const QJsonObject &event);
    void sendIncomingCommand(quint64 session, const QVariantMap &command, const QJsonObject &event, bool skipUnchanged);
    QVariantList incomingCommands(quint64 session) const;

    TcpControlServer *m_listener;
    QString m_configurationPath;
    QString m_lastError;
    QVariantList m_servers;
    QVariantList m_commands;
    QVariantList m_incomingClients;
    QHash<quint64, IncomingSession> m_sessions;
    QHash<QString, TcpControlClient *> m_clients;
    QSet<QString> m_manualStops;
    QHash<QString, QStringList> m_subscriptions;
    QHash<QString, QStringList> m_topicDependencies;
    QPointer<ControlEventFeed> m_eventFeed;
    ListFormatter m_listFormatter;
    QHash<QString, QByteArray> m_lastValuePayloads;
    bool m_active = false;
};
#endif
