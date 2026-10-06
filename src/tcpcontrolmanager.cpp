/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tcpcontrolmanager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

TcpControlManager::TcpControlManager(QObject *parent, const QString &path)
    : QObject(parent), m_configurationPath(path) {
    if (!path.isEmpty() && QFileInfo::exists(path))
        loadConfiguration();
}

int TcpControlManager::find(const QVariantList &list, const QString &id) {
    for (int i = 0; i < list.size(); ++i)
        if (list.at(i).toMap().value(QStringLiteral("id")).toString() == id)
            return i;
    return -1;
}

void TcpControlManager::error(const QString &description) {
    m_lastError = description;
    Q_EMIT lastErrorChanged();
}

TcpControlClient::Options TcpControlManager::options(const QVariantMap &s) {
    TcpControlClient::Options o;
    o.host = s.value(QStringLiteral("host")).toString();
    o.port = s.value(QStringLiteral("port")).toInt();
    o.autoReconnect = s.value(QStringLiteral("autoReconnect"), true).toBool();
    o.connectTimeoutMs = s.value(QStringLiteral("connectTimeoutMs"), 5000).toInt();
    o.retryInitialMs = s.value(QStringLiteral("retryInitialMs"), 1000).toInt();
    o.retryMaximumMs = s.value(QStringLiteral("retryMaximumMs"), 30000).toInt();
    o.stableConnectionMs = s.value(QStringLiteral("stableConnectionMs"), 10000).toInt();
    o.framing = static_cast<TcpControlClient::FramingMode>(s.value(QStringLiteral("framing"), 0).toInt());
    o.receiveTerminator = QByteArray::fromHex(s.value(QStringLiteral("receiveTerminatorHex"), QStringLiteral("0a")).toByteArray());
    o.sendTerminator = QByteArray::fromHex(s.value(QStringLiteral("sendTerminatorHex"), QStringLiteral("0a")).toByteArray());
    o.maximumMessageBytes = s.value(QStringLiteral("maximumMessageBytes"), 1048576).toInt();
    o.maximumPendingWriteBytes = s.value(QStringLiteral("maximumPendingWriteBytes"), 1048576).toInt();
    return o;
}

bool TcpControlManager::normalizeServer(const QVariantMap &input, QVariantMap &output) {
    const QString receiveHex = input.value(QStringLiteral("receiveTerminatorHex"), QStringLiteral("0a")).toString();
    const QString sendHex = input.value(QStringLiteral("sendTerminatorHex"), QStringLiteral("0a")).toString();
    static const QRegularExpression hex(QStringLiteral("^(?:[0-9a-fA-F]{2}){0,256}$"));
    const auto o = options(input);
    TcpControlClient validator;
    if (input.value(QStringLiteral("name")).toString().trimmed().isEmpty()
        || (o.framing == TcpControlClient::Delimiter
            && (!hex.match(receiveHex).hasMatch() || !hex.match(sendHex).hasMatch()))
        || !validator.configure(o)) {
        error(tr("Server requires a name, valid host/port, framing, and connection limits"));
        return false;
    }
    output = {
        {QStringLiteral("id"), input.value(QStringLiteral("id")).toString()}, {QStringLiteral("name"), input.value(QStringLiteral("name")).toString().trimmed()},
        {QStringLiteral("host"), o.host.trimmed()}, {QStringLiteral("port"), o.port},
        {QStringLiteral("enabled"), input.value(QStringLiteral("enabled"), true).toBool()},
        {QStringLiteral("acceptControlCommands"), input.value(QStringLiteral("acceptControlCommands"), false).toBool()},
        {QStringLiteral("autoReconnect"), o.autoReconnect}, {QStringLiteral("connectTimeoutMs"), o.connectTimeoutMs},
        {QStringLiteral("retryInitialMs"), o.retryInitialMs}, {QStringLiteral("retryMaximumMs"), o.retryMaximumMs},
        {QStringLiteral("stableConnectionMs"), o.stableConnectionMs}, {QStringLiteral("framing"), int(o.framing)},
        {QStringLiteral("receiveTerminatorHex"), receiveHex}, {QStringLiteral("sendTerminatorHex"), sendHex},
        {QStringLiteral("maximumMessageBytes"), o.maximumMessageBytes}, {QStringLiteral("maximumPendingWriteBytes"), o.maximumPendingWriteBytes}
    };
    return true;
}

bool TcpControlManager::normalizeCommand(const QVariantMap &input, const QVariantList &servers, QVariantMap &output) {
    const QString encoding = input.value(QStringLiteral("encoding"), QStringLiteral("text")).toString();
    static const QRegularExpression hex(QStringLiteral("^(?:[0-9a-fA-F]{2})*$"));
    if (input.value(QStringLiteral("name")).toString().trimmed().isEmpty()
        || find(servers, input.value(QStringLiteral("serverId")).toString()) < 0
        || (encoding != QStringLiteral("text") && encoding != QStringLiteral("hex"))
        || (encoding == QStringLiteral("hex") && !hex.match(input.value(QStringLiteral("payload")).toString()).hasMatch())) {
        error(tr("Command requires a name, existing server, and text or hexadecimal byte pairs"));
        return false;
    }
    output = { {QStringLiteral("id"), input.value(QStringLiteral("id")).toString()}, {QStringLiteral("name"), input.value(QStringLiteral("name")).toString().trimmed()},
        {QStringLiteral("serverId"), input.value(QStringLiteral("serverId")).toString()}, {QStringLiteral("encoding"), encoding},
        {QStringLiteral("payload"), input.value(QStringLiteral("payload")).toString()}, {QStringLiteral("raw"), input.value(QStringLiteral("raw"), false).toBool()} };
    return true;
}

bool TcpControlManager::persist(const QVariantList &servers, const QVariantList &commands) {
    if (!m_configurationPath.isEmpty()) {
        if (!QDir().mkpath(QFileInfo(m_configurationPath).absolutePath())) {
            error(tr("Cannot create TCP configuration directory"));
            return false;
        }
        QSaveFile file(m_configurationPath);
        const auto bytes = QJsonDocument(QJsonObject {
            {QStringLiteral("version"), 1}, {QStringLiteral("servers"), QJsonArray::fromVariantList(servers)},
            {QStringLiteral("commands"), QJsonArray::fromVariantList(commands)} }).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            error(tr("Cannot save TCP configuration: %1").arg(file.errorString()));
            return false;
        }
    }
    m_lastError.clear();
    Q_EMIT lastErrorChanged();
    return true;
}

bool TcpControlManager::loadConfiguration(const QString &sourcePath) {
    QFile file(sourcePath.isEmpty() ? m_configurationPath : sourcePath);
    if (!file.open(QIODevice::ReadOnly)) {
        error(tr("Cannot read TCP configuration: %1").arg(file.errorString()));
        return false;
    }
    QJsonParseError parseError;
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto root = doc.object();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()
        || root.value(QStringLiteral("version")).toInt() != 1 || !root.value(QStringLiteral("servers")).isArray() || !root.value(QStringLiteral("commands")).isArray()) {
        error(tr("Invalid TCP configuration format"));
        return false;
    }
    QVariantList servers, commands;
    QSet<QString> ids;
    for (const auto value : root.value(QStringLiteral("servers")).toArray()) {
        QVariantMap server;
        if (!value.isObject() || !normalizeServer(value.toObject().toVariantMap(), server))
            return false;
        const QString id = server.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || ids.contains(id)) {
            error(tr("Missing or duplicate TCP server ID"));
            return false;
        }
        ids.insert(id);
        servers.append(server);
    }
    ids.clear();
    for (const auto value : root.value(QStringLiteral("commands")).toArray()) {
        QVariantMap command;
        if (!value.isObject() || !normalizeCommand(value.toObject().toVariantMap(), servers, command))
            return false;
        const QString id = command.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || ids.contains(id)) {
            error(tr("Missing or duplicate TCP command ID"));
            return false;
        }
        ids.insert(id);
        commands.append(command);
    }
    m_servers = servers;
    m_commands = commands;
    m_manualStops.clear();
    m_lastError.clear();
    reconcile();
    Q_EMIT serversChanged();
    Q_EMIT commandsChanged();
    Q_EMIT lastErrorChanged();
    return true;
}

QString TcpControlManager::saveServer(const QVariantMap &profile) {
    QVariantMap server;
    if (!normalizeServer(profile, server))
        return {};
    QString id = server.value(QStringLiteral("id")).toString();
    const int index = find(m_servers, id);
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        server[QStringLiteral("id")] = id;
    } else if (index < 0) {
        error(tr("TCP server no longer exists"));
        return {};
    }
    auto servers = m_servers;
    if (index < 0) servers.append(server); else servers[index] = server;
    if (!persist(servers, m_commands))
        return {};
    m_servers = servers;
    m_manualStops.remove(id);
    reconcile();
    Q_EMIT serversChanged();
    Q_EMIT commandsChanged(); // Server names appear in command descriptions.
    return id;
}

bool TcpControlManager::removeServer(const QString &id) {
    const int index = find(m_servers, id);
    if (index < 0)
        return false;
    for (const auto &value : m_commands) {
        if (value.toMap().value(QStringLiteral("serverId")).toString() == id) {
            error(tr("Remove or reassign this server's commands before deleting the server"));
            return false;
        }
    }
    auto servers = m_servers;
    servers.removeAt(index);
    if (!persist(servers, m_commands))
        return false;
    m_servers = servers;
    m_manualStops.remove(id);
    reconcile();
    Q_EMIT serversChanged();
    return true;
}

QString TcpControlManager::saveCommand(const QVariantMap &input) {
    QVariantMap item;
    if (!normalizeCommand(input, m_servers, item))
        return {};
    QString id = item.value(QStringLiteral("id")).toString();
    const int index = find(m_commands, id);
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        item[QStringLiteral("id")] = id;
    } else if (index < 0) {
        error(tr("TCP command no longer exists"));
        return {};
    }
    auto commands = m_commands;
    if (index < 0) commands.append(item); else commands[index] = item;
    if (!persist(m_servers, commands))
        return {};
    m_commands = commands;
    Q_EMIT commandsChanged();
    return id;
}

bool TcpControlManager::removeCommand(const QString &id) {
    const int index = find(m_commands, id);
    if (index < 0)
        return false;
    auto commands = m_commands;
    commands.removeAt(index);
    if (!persist(m_servers, commands))
        return false;
    m_commands = commands;
    Q_EMIT commandsChanged();
    return true;
}

QVariantList TcpControlManager::servers() const {
    QVariantList result;
    for (const auto &value : m_servers) {
        auto profile = value.toMap();
        auto *client = m_clients.value(profile.value(QStringLiteral("id")).toString());
        profile[QStringLiteral("state")] = client ? client->stateText() : tr("Stopped");
        profile[QStringLiteral("connected")] = client && client->isConnected();
        profile[QStringLiteral("lastError")] = client ? client->lastError() : QString();
        result.append(profile);
    }
    return result;
}

void TcpControlManager::setActive(bool active) {
    if (m_active == active)
        return;
    m_active = active;
    if (active)
        m_manualStops.clear();
    reconcile();
    Q_EMIT activeChanged();
}

void TcpControlManager::reconcile() {
    for (const auto &id : m_clients.keys()) {
        if (find(m_servers, id) < 0) {
            auto *client = m_clients.take(id);
            client->disconnect(this);
            client->stop();
            client->deleteLater();
        }
    }
    for (const auto &value : m_servers) {
        const auto server = value.toMap();
        const QString id = server.value(QStringLiteral("id")).toString();
        auto *client = m_clients.value(id);
        if (!client) {
            client = new TcpControlClient(this);
            m_clients.insert(id, client);
            connect(client, &TcpControlClient::connectionStateChanged, this, [this, id](auto state) {
                Q_EMIT serversChanged();
                Q_EMIT connectionStateChanged(id, state);
            });
            connect(client, &TcpControlClient::lastErrorChanged, this, &TcpControlManager::serversChanged);
            connect(client, &TcpControlClient::errorOccurred, this, [this, id](const QString &reason) {
                Q_EMIT serverError(id, reason);
            });
            connect(client, &TcpControlClient::messageReceived, this, [this, id](const QByteArray &payload) {
                Q_EMIT messageReceived(id, payload);
                const int index = find(m_servers, id);
                if (m_active && index >= 0 && m_servers.at(index).toMap().value(QStringLiteral("acceptControlCommands")).toBool())
                    Q_EMIT controlMessageReceived(id, payload);
            });
        }
        if (!m_active || !server.value(QStringLiteral("enabled")).toBool())
            client->stop();
        if (client->configure(options(server)) && m_active && server.value(QStringLiteral("enabled")).toBool()
            && !m_manualStops.contains(id))
            client->start();
    }
}

void TcpControlManager::connectServer(const QString &id) {
    const int index = find(m_servers, id);
    if (m_active && index >= 0 && m_servers.at(index).toMap().value(QStringLiteral("enabled")).toBool())
    {
        m_manualStops.remove(id);
        m_clients.value(id)->start();
    }
}

void TcpControlManager::disconnectServer(const QString &id) {
    if (auto *client = m_clients.value(id))
    {
        m_manualStops.insert(id);
        client->stop();
    }
}

QVariantMap TcpControlManager::command(const QString &id) const {
    const int index = find(m_commands, id);
    return index >= 0 ? m_commands.at(index).toMap() : QVariantMap();
}

QString TcpControlManager::commandDescription(const QString &id) const {
    const auto item = command(id);
    const int server = find(m_servers, item.value(QStringLiteral("serverId")).toString());
    return item.isEmpty() ? tr("Missing command (%1)").arg(id)
        : item.value(QStringLiteral("name")).toString() + QStringLiteral(" → ")
            + (server >= 0 ? m_servers.at(server).toMap().value(QStringLiteral("name")).toString() : tr("Missing server"));
}

bool TcpControlManager::triggerCommand(const QString &id) {
    const auto item = command(id);
    const QString serverId = item.value(QStringLiteral("serverId")).toString();
    auto *client = m_clients.value(serverId);
    const int serverIndex = find(m_servers, serverId);
    bool accepted = false;
    if (!m_active || item.isEmpty() || !client || serverIndex < 0
        || !m_servers.at(serverIndex).toMap().value(QStringLiteral("enabled")).toBool() || !client->isConnected()) {
        error(tr("TCP command rejected: missing command or server is disabled/disconnected"));
    } else {
        const QByteArray payload = item.value(QStringLiteral("encoding")).toString() == QStringLiteral("hex")
            ? QByteArray::fromHex(item.value(QStringLiteral("payload")).toByteArray()) : item.value(QStringLiteral("payload")).toString().toUtf8();
        accepted = item.value(QStringLiteral("raw")).toBool() ? client->sendBinary(payload) : client->sendMessage(payload);
        if (!accepted) error(client->lastError());
        else { m_lastError.clear(); Q_EMIT lastErrorChanged(); }
    }
    Q_EMIT commandTriggered(id, accepted);
    return accepted;
}
