/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "tcpcontrolmanager.h"
#include "controleventfeed.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostAddress>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>
#include <cmath>

namespace {
QByteArray jsonValueBytes(const QJsonValue &value) {
    if (value.isObject()) return QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact);
    if (value.isArray()) return QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact);
    return QJsonDocument(QJsonArray{value}).toJson(QJsonDocument::Compact).mid(1).chopped(1);
}

QString singleLine(QString text) {
    return text.replace(QLatin1Char('\r'), QLatin1Char(' ')).replace(QLatin1Char('\n'), QLatin1Char(' '));
}

QByteArray eventValueBytes(const QJsonValue &value, int charsPerItem) {
    QJsonValue rows = value;
    if (value.isObject() && value.toObject().value(QStringLiteral("playlist")).isArray())
        rows = value.toObject().value(QStringLiteral("playlist"));
    if (charsPerItem > 0 && rows.isArray()) {
        QString formatted;
        const auto items = rows.toArray();
        for (int i = 0; i < items.size(); ++i) {
            const auto item = items.at(i);
            QString label;
            if (item.isString()) label = item.toString();
            else if (item.isObject()) {
                const auto row = item.toObject();
                for (const auto &key : {QStringLiteral("listTitle"), QStringLiteral("shortText"),
                    QStringLiteral("title"), QStringLiteral("name"), QStringLiteral("text"),
                    QStringLiteral("language"), QStringLiteral("video"), QStringLiteral("path")}) {
                    if (!row.value(key).toString().isEmpty()) { label = row.value(key).toString(); break; }
                }
                if (label.isEmpty()) label = QString::fromUtf8(jsonValueBytes(item));
            } else label = QString::fromUtf8(jsonValueBytes(item));
            // Width is measured in Unicode code points, without splitting surrogate pairs.
            auto characters = singleLine(QString::number(i + 1) + QStringLiteral(". ") + label).toUcs4();
            if (characters.size() > charsPerItem) {
                characters.resize(charsPerItem >= 4 ? charsPerItem - 4 : charsPerItem);
                label = QString::fromUcs4(characters.constData(), characters.size());
                if (charsPerItem >= 4) label += QStringLiteral("... ");
            } else {
                label = QString::fromUcs4(characters.constData(), characters.size());
                label += QString(charsPerItem - characters.size(), QLatin1Char(' '));
            }
            formatted += label;
        }
        return formatted.toUtf8();
    }
    if (value.isString()) return singleLine(value.toString()).toUtf8();
    if (value.isBool()) return value.toBool() ? QByteArray("1") : QByteArray("0");
    return jsonValueBytes(value);
}
}

TcpControlManager::TcpControlManager(QObject *parent, const QString &path)
    : QObject(parent), m_listener(new TcpControlServer(this)), m_configurationPath(path) {
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
    const auto topics = input.value(QStringLiteral("eventTopics")).toStringList();
    if (!ControlEventFeed::validateTopics(topics)) {
        error(tr("Unknown TCP event topic"));
        return false;
    }
    const QString receiveHex = input.value(QStringLiteral("receiveTerminatorHex"), QStringLiteral("0a")).toString();
    const QString sendHex = input.value(QStringLiteral("sendTerminatorHex"), QStringLiteral("0a")).toString();
    static const QRegularExpression hex(QStringLiteral("^(?:[0-9a-fA-F]{2}){0,256}$"));
    auto o = options(input);
    o.host = o.host.trimmed().toLower();
    if (o.host.startsWith(QLatin1Char('[')) && o.host.endsWith(QLatin1Char(']')))
        o.host = o.host.mid(1, o.host.size() - 2);
    QHostAddress address;
    const bool numericAddress = address.setAddress(o.host);
    if (numericAddress) o.host = address.toString().toLower();
    const QString endpointHost = o.host.contains(QLatin1Char(':'))
        ? QStringLiteral("[") + o.host + QStringLiteral("]") : o.host;
    const QString id = endpointHost + QLatin1Char(':') + QString::number(o.port);
    TcpControlClient validator;
    if (input.value(QStringLiteral("name")).toString().trimmed().isEmpty()
        || (o.framing == TcpControlClient::Delimiter
            && (!hex.match(receiveHex).hasMatch() || !hex.match(sendHex).hasMatch()))
        || !validator.configure(o)) {
        error(tr("Server requires a name, valid host/port, framing, and connection limits"));
        return false;
    }
    output = {
        {QStringLiteral("id"), id}, {QStringLiteral("name"), input.value(QStringLiteral("name")).toString().trimmed()},
        {QStringLiteral("host"), o.host.trimmed()}, {QStringLiteral("port"), o.port},
        {QStringLiteral("enabled"), input.value(QStringLiteral("enabled"), true).toBool()},
        {QStringLiteral("eventTopics"), topics},
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
    const auto topics = input.value(QStringLiteral("eventTopics")).toStringList();
    QString mode = input.value(QStringLiteral("eventPayload"), QStringLiteral("fixed")).toString();
    // Earlier event models used change counters; migrate them to actual values.
    const bool wasChange = mode == QStringLiteral("change");
    if (wasChange) mode = QStringLiteral("value");
    bool widthValid = false;
    const double width = input.value(QStringLiteral("charsPerItem"), wasChange ? 40 : 0).toDouble(&widthValid);
    if (!widthValid || !std::isfinite(width) || width < 0 || width > 4096 || std::floor(width) != width) {
        error(tr("charsPerItem must be an integer from 0 to 4096"));
        return false;
    }
    if ((mode != QStringLiteral("fixed") && mode != QStringLiteral("value"))
        || (mode != QStringLiteral("fixed") && (topics.size() != 1 || topics.contains(QStringLiteral("*"))
            || input.value(QStringLiteral("encoding"), QStringLiteral("text")).toString() != QStringLiteral("text")))) {
        error(tr("Event payload requires text encoding and exactly one topic; mode must be fixed or value"));
        return false;
    }
    if (!ControlEventFeed::validateTopics(topics)) {
        error(tr("Unknown TCP command event topic"));
        return false;
    }
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
        {QStringLiteral("payload"), input.value(QStringLiteral("payload")).toString()}, {QStringLiteral("raw"), input.value(QStringLiteral("raw"), false).toBool()},
        {QStringLiteral("eventTopics"), topics}, {QStringLiteral("eventPayload"), mode},
        {QStringLiteral("charsPerItem"), static_cast<int>(width)} };
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
    file.close(); // Release the Windows handle before atomically rewriting a migrated file.
    const auto root = doc.object();
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()
        || root.value(QStringLiteral("version")).toInt() != 1 || !root.value(QStringLiteral("servers")).isArray() || !root.value(QStringLiteral("commands")).isArray()) {
        error(tr("Invalid TCP configuration format"));
        return false;
    }
    QVariantList servers, commands;
    QSet<QString> ids;
    QHash<QString, QString> migratedIds;
    bool idsChanged = false;
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
        const QString oldId = value.toObject().value(QStringLiteral("id")).toString();
        if (!oldId.isEmpty()) {
            if (migratedIds.contains(oldId)) {
                error(tr("Duplicate TCP server ID"));
                return false;
            }
            migratedIds.insert(oldId, id);
        }
        idsChanged = idsChanged || oldId != id;
        servers.append(server);
    }
    ids.clear();
    for (const auto value : root.value(QStringLiteral("commands")).toArray()) {
        QVariantMap command;
        auto input = value.toObject().toVariantMap();
        const QString oldServerId = input.value(QStringLiteral("serverId")).toString();
        if (migratedIds.contains(oldServerId)) input[QStringLiteral("serverId")] = migratedIds.value(oldServerId);
        if (!value.isObject() || !normalizeCommand(input, servers, command))
            return false;
        const QString id = command.value(QStringLiteral("id")).toString();
        if (id.isEmpty() || ids.contains(id)) {
            error(tr("Missing or duplicate TCP command ID"));
            return false;
        }
        ids.insert(id);
        commands.append(command);
    }
    if (idsChanged && sourcePath.isEmpty() && !persist(servers, commands)) return false;
    m_servers = servers;
    m_commands = commands;
    m_manualStops.clear();
    m_subscriptions.clear();
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
    const QString id = server.value(QStringLiteral("id")).toString();
    const QString oldId = profile.value(QStringLiteral("id")).toString();
    const int index = oldId.isEmpty() ? -1 : find(m_servers, oldId);
    if (!oldId.isEmpty() && index < 0 && oldId != id) {
        error(tr("TCP server no longer exists"));
        return {};
    }
    const int duplicate = find(m_servers, id);
    if (duplicate >= 0 && duplicate != index) {
        error(tr("A TCP server with this host and port already exists"));
        return {};
    }
    auto servers = m_servers;
    auto commands = m_commands;
    if (index >= 0 && oldId != id) {
        for (auto &entry : commands) {
            auto command = entry.toMap();
            if (command.value(QStringLiteral("serverId")).toString() == oldId) {
                command[QStringLiteral("serverId")] = id;
                entry = command;
            }
        }
    }
    if (index < 0) servers.append(server); else servers[index] = server;
    if (!persist(servers, commands))
        return {};
    m_servers = servers;
    m_commands = commands;
    if (oldId != id) {
        m_manualStops.remove(oldId);
        m_subscriptions.remove(oldId);
    }
    m_manualStops.remove(id);
    if (!server.value(QStringLiteral("acceptControlCommands")).toBool()) m_subscriptions.remove(id);
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
    m_subscriptions.remove(id);
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
    if (!active) m_listener->stop();
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
                if (state != TcpControlClient::Connected) m_subscriptions.remove(id);
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
                    if (!handleEventRequest(id, payload)) Q_EMIT controlMessageReceived(id, payload);
            });
        }
        if (!m_active || !server.value(QStringLiteral("enabled")).toBool())
            client->stop();
        if (client->configure(options(server)) && m_active && server.value(QStringLiteral("enabled")).toBool()
            && !m_manualStops.contains(id))
            client->start();
    }
}

bool TcpControlManager::connectServer(const QString &id) {
    const int index = find(m_servers, id);
    if (m_active && index >= 0 && m_servers.at(index).toMap().value(QStringLiteral("enabled")).toBool())
    {
        m_manualStops.remove(id);
        m_clients.value(id)->start();
        m_lastError.clear();
        Q_EMIT lastErrorChanged();
        return true;
    }
    error(tr("TCP connect rejected: missing server, disabled server or inactive player"));
    return false;
}

bool TcpControlManager::disconnectServer(const QString &id) {
    if (auto *client = m_clients.value(id))
    {
        m_manualStops.insert(id);
        client->stop();
        m_lastError.clear();
        Q_EMIT lastErrorChanged();
        return true;
    }
    error(tr("TCP server does not exist"));
    return false;
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
    return dispatchCommand(id, {});
}

bool TcpControlManager::setCommandCharsPerItem(const QString &id, int charsPerItem) {
    auto item = command(id);
    if (item.isEmpty()) { error(tr("TCP command does not exist")); return false; }
    item[QStringLiteral("charsPerItem")] = charsPerItem;
    return !saveCommand(item).isEmpty();
}

QVariantMap TcpControlManager::triggerAll(const QString &serverId) {
    auto *client = m_clients.value(serverId);
    if (!m_active || !m_eventFeed || !client || !client->isConnected()) {
        error(tr("Full update requires an active event feed and a connected TCP server"));
        return {{QStringLiteral("success"), false}, {QStringLiteral("accepted"), 0},
            {QStringLiteral("rejected"), 0}, {QStringLiteral("message"), m_lastError}};
    }
    const auto values = m_eventFeed->read({QStringLiteral("*")}).value(QStringLiteral("values")).toObject();
    const auto commands = m_commands;
    int accepted = 0, rejected = 0;
    QString failure;
    for (const auto &entry : commands) {
        const auto item = entry.toMap();
        if (item.value(QStringLiteral("serverId")).toString() != serverId
            || item.value(QStringLiteral("eventTopics")).toStringList().isEmpty()) continue;
        // Manual-only device/task actions are not part of a state refresh.
        const auto topics = item.value(QStringLiteral("eventTopics")).toStringList();
        const QJsonObject event{{QStringLiteral("topic"), topics.first()},
            {QStringLiteral("value"), values.value(topics.first())}};
        if (dispatchCommand(item.value(QStringLiteral("id")).toString(), event)) ++accepted;
        else { ++rejected; if (failure.isEmpty()) failure = m_lastError; }
    }
    if (!failure.isEmpty()) error(failure);
    return {{QStringLiteral("success"), rejected == 0}, {QStringLiteral("accepted"), accepted},
        {QStringLiteral("rejected"), rejected}, {QStringLiteral("message"), failure}};
}

bool TcpControlManager::dispatchCommand(const QString &id, const QJsonObject &event) {
    const auto item = command(id);
    const QString serverId = item.value(QStringLiteral("serverId")).toString();
    auto *client = m_clients.value(serverId);
    const int serverIndex = find(m_servers, serverId);
    bool accepted = false;
    if (!m_active || item.isEmpty() || !client || serverIndex < 0
        || !m_servers.at(serverIndex).toMap().value(QStringLiteral("enabled")).toBool() || !client->isConnected()) {
        error(tr("TCP command rejected: missing command or server is disabled/disconnected"));
    } else {
        QByteArray payload = item.value(QStringLiteral("encoding")).toString() == QStringLiteral("hex")
            ? QByteArray::fromHex(item.value(QStringLiteral("payload")).toByteArray()) : item.value(QStringLiteral("payload")).toString().toUtf8();
        const QString mode = item.value(QStringLiteral("eventPayload")).toString();
        if (mode == QStringLiteral("value")) {
            QJsonValue value = event.value(QStringLiteral("value"));
            if (event.isEmpty() && m_eventFeed) {
                const auto topics = item.value(QStringLiteral("eventTopics")).toStringList();
                value = m_eventFeed->read(topics).value(QStringLiteral("values")).toObject().value(topics.first());
            }
            if (value.isUndefined()) {
                error(tr("Event value is unavailable"));
                Q_EMIT commandTriggered(id, false);
                return false;
            }
            payload += eventValueBytes(value, item.value(QStringLiteral("charsPerItem")).toInt());
        }
        accepted = item.value(QStringLiteral("raw")).toBool() ? client->sendBinary(payload) : client->sendMessage(payload);
        if (!accepted) error(client->lastError());
        else { m_lastError.clear(); Q_EMIT lastErrorChanged(); }
    }
    Q_EMIT commandTriggered(id, accepted);
    return accepted;
}

QStringList TcpControlManager::eventTopics() const {
    return ControlEventFeed::availableTopics();
}

void TcpControlManager::setEventFeed(ControlEventFeed *feed) {
    if (m_eventFeed) disconnect(m_eventFeed.data(), nullptr, this, nullptr);
    m_eventFeed = feed;
    if (feed) connect(feed, &ControlEventFeed::eventPublished, this, &TcpControlManager::publishEvent);
}

void TcpControlManager::sendEventMessage(const QString &id, const QJsonObject &message) {
    auto *client = m_clients.value(id);
    if (!m_active || !client || !client->isConnected()) return;
    if (!client->sendMessage(QJsonDocument(message).toJson(QJsonDocument::Compact)))
        Q_EMIT serverError(id, client->lastError());
}

void TcpControlManager::publishEvent(const QJsonObject &event) {
    if (!m_active) return;
    const QString topic = event.value(QStringLiteral("topic")).toString();
    const auto matches = [&topic](const QStringList &topics) {
        return topics.contains(QStringLiteral("*")) || topics.contains(topic);
    };
    for (const auto &value : m_servers) {
        const auto server = value.toMap();
        const QString id = server.value(QStringLiteral("id")).toString();
        if (server.value(QStringLiteral("enabled")).toBool()
            && (matches(server.value(QStringLiteral("eventTopics")).toStringList()) || matches(m_subscriptions.value(id))))
            sendEventMessage(id, event);
    }
    // Work on a copy: commandTriggered handlers can edit configuration.
    const auto commands = m_commands;
    for (const auto &value : commands) {
        const auto command = value.toMap();
        if (matches(command.value(QStringLiteral("eventTopics")).toStringList()))
            dispatchCommand(command.value(QStringLiteral("id")).toString(), event);
    }
}

bool TcpControlManager::importEventModel(const QString &serverId, const QString &path) {
    if (find(m_servers, serverId) < 0) {
        error(tr("Select a saved TCP server before importing an EventModel"));
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error(tr("Cannot read EventModel: %1").arg(file.errorString()));
        return false;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    const auto root = document.object();
    if (parseError.error != QJsonParseError::NoError || !document.isObject()
        || root.value(QStringLiteral("version")).toInt() != 1 || !root.value(QStringLiteral("commands")).isArray()) {
        error(tr("Invalid EventModel format"));
        return false;
    }
    auto commands = m_commands;
    QSet<QString> ids;
    for (const auto value : root.value(QStringLiteral("commands")).toArray()) {
        auto input = value.toObject().toVariantMap();
        const QString modelId = input.value(QStringLiteral("id")).toString();
        if (!value.isObject() || modelId.isEmpty() || ids.contains(modelId)) {
            error(tr("Missing or duplicate EventModel command ID"));
            return false;
        }
        ids.insert(modelId);
        QString id = serverId + QStringLiteral(":") + modelId;
        for (const auto &entry : commands) {
            const auto saved = entry.toMap();
            const QString savedId = saved.value(QStringLiteral("id")).toString();
            if (saved.value(QStringLiteral("serverId")).toString() == serverId
                && (savedId == modelId || savedId.endsWith(QStringLiteral(":") + modelId))) {
                // Imported command IDs stay stable even when their endpoint changes.
                id = savedId;
                break;
            }
        }
        input[QStringLiteral("id")] = id;
        input[QStringLiteral("serverId")] = serverId;
        QVariantMap command;
        if (!normalizeCommand(input, m_servers, command)) return false;
        const int index = find(commands, id);
        if (index < 0) commands.append(command); else commands[index] = command;
    }
    if (!persist(m_servers, commands)) return false;
    m_commands = commands;
    Q_EMIT commandsChanged();
    return true;
}

bool TcpControlManager::handleEventRequest(const QString &id, const QByteArray &payload) {
    const auto document = QJsonDocument::fromJson(payload);
    if (!document.isObject()) return false;
    const auto object = document.object();
    const QString operation = object.value(QStringLiteral("operation")).toString();
    if (operation != QStringLiteral("Subscribe") && operation != QStringLiteral("Unsubscribe")
        && operation != QStringLiteral("Get")) return false;
    QStringList topics;
    bool valid = object.value(QStringLiteral("topics")).isArray() && object.size() == 2;
    for (const auto &topic : object.value(QStringLiteral("topics")).toArray()) {
        valid = valid && topic.isString();
        topics.append(topic.toString());
    }
    if (!valid || !ControlEventFeed::validateTopics(topics) || !m_eventFeed) {
        sendEventMessage(id, {{QStringLiteral("type"), QStringLiteral("error")},
            {QStringLiteral("message"), tr("Expected operation and an array of supported event topics")}});
        return true;
    }
    if (operation == QStringLiteral("Subscribe")) m_subscriptions[id] = topics;
    if (operation == QStringLiteral("Unsubscribe")) {
        if (topics.contains(QStringLiteral("*"))) m_subscriptions.remove(id);
        else for (const auto &topic : topics) m_subscriptions[id].removeAll(topic);
        sendEventMessage(id, {{QStringLiteral("type"), QStringLiteral("unsubscribed")}});
    } else {
        sendEventMessage(id, m_eventFeed->read(topics));
    }
    return true;
}
