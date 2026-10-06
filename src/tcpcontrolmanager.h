/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TCPCONTROLMANAGER_H
#define TCPCONTROLMANAGER_H
#include "tcpcontrolclient.h"
#include <QHash>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>

// Owns one persistent transport per server. Profiles and commands use stable IDs,
// so editing names and endpoints does not invalidate presentation references.
class TcpControlManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(QVariantList commands READ commands NOTIFY commandsChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString configurationPath READ configurationPath CONSTANT)
    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
public:
    explicit TcpControlManager(QObject *parent = nullptr, const QString &configurationPath = {});
    QVariantList servers() const;
    QVariantList commands() const { return m_commands; }
    QString lastError() const { return m_lastError; }
    QString configurationPath() const { return m_configurationPath; }
    bool active() const { return m_active; }
    void setActive(bool active);
    bool loadConfiguration(const QString &sourcePath = {});
    TcpControlClient *clientForServer(const QString &id) const { return m_clients.value(id); }

    // Empty id creates a new UUID. A supplied id updates an existing profile.
    Q_INVOKABLE QString saveServer(const QVariantMap &profile);
    Q_INVOKABLE bool removeServer(const QString &id);
    Q_INVOKABLE QString saveCommand(const QVariantMap &command);
    Q_INVOKABLE bool removeCommand(const QString &id);
    Q_INVOKABLE bool triggerCommand(const QString &id);
    Q_INVOKABLE void connectServer(const QString &id);
    Q_INVOKABLE void disconnectServer(const QString &id);
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

    QString m_configurationPath;
    QString m_lastError;
    QVariantList m_servers;
    QVariantList m_commands;
    QHash<QString, TcpControlClient *> m_clients;
    QSet<QString> m_manualStops;
    bool m_active = false;
};
#endif
