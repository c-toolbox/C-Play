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
#include <QTime>
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

// Seconds as text in a QTime format (like REST `format=`); negative times keep a leading minus.
QByteArray formatSeconds(double seconds, const QString &format) {
    const qint64 ms = std::llround(std::abs(seconds) * 1000.0);
    const QString text = QTime(0, 0).addMSecs(ms).toString(format);
    return ((seconds < 0 && ms > 0 ? QStringLiteral("-") : QString()) + text).toUtf8();
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
                label = QString::fromUcs4(reinterpret_cast<const char32_t *>(characters.constData()), characters.size());
                if (charsPerItem >= 4) label += QStringLiteral("... ");
            } else {
                label = QString::fromUcs4(reinterpret_cast<const char32_t *>(characters.constData()), characters.size());
                label += QString(charsPerItem - characters.size(), QLatin1Char(' '));
            }
            // Rows are LF separated like the REST list endpoints.
            if (i > 0) formatted += QLatin1Char('\n');
            formatted += label;
        }
        return formatted.toUtf8();
    }
    if (value.isString()) return singleLine(value.toString()).toUtf8();
    if (value.isBool()) return value.toBool() ? QByteArray("1") : QByteArray("0");
    return jsonValueBytes(value);
}
}

// Topics a profile subscribes its clients to on connect: eventTopics, with * minus excludedTopics.
static QStringList connectTopics(const QVariantMap &profile) {
    auto topics = profile.value(QStringLiteral("eventTopics")).toStringList();
    const auto excluded = profile.value(QStringLiteral("excludedTopics")).toStringList();
    if (topics.contains(QStringLiteral("*")) && !excluded.isEmpty()) {
        topics = ControlEventFeed::availableTopics();
        for (const auto &topic : excluded) topics.removeAll(topic);
    }
    return topics;
}

TcpControlManager::TcpControlManager(QObject *parent, const QString &path)
    : QObject(parent), m_listener(new TcpControlServer(this)), m_configurationPath(path) {
    connect(m_listener, &TcpControlServer::clientConnected, this, [this](quint64 id, const QString &address, int port) {
        IncomingSession session;
        session.address = address;
        session.port = port;
        m_sessions.insert(id, session);
        applyIncomingProfile(id);
        // A profile can subscribe its clients on connect, like a server's notification topics.
        // Without an initial update, frequently reconnecting clients request values with /full_update.
        const auto profile = m_sessions.value(id).profile;
        const auto topics = connectTopics(profile);
        if (!topics.isEmpty())
            subscribeIncoming(id, topics, profile.value(QStringLiteral("sendInitialUpdate"), true).toBool());
        Q_EMIT incomingSessionsChanged();
    });
    connect(m_listener, &TcpControlServer::clientDisconnected, this, [this](quint64 id) {
        if (m_sessions.remove(id)) Q_EMIT incomingSessionsChanged();
    });
    m_listener->setSessionHandler([this](quint64 id, const QString &path, const QJsonObject &parameters) {
        return handleIncomingRequest(id, path, parameters);
    });
    m_incomingClients = withDefaultIncomingClient({});
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
        {QStringLiteral("connectOnStartup"), input.value(QStringLiteral("connectOnStartup"), true).toBool()},
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
    const QString timeFormat = input.value(QStringLiteral("timeFormat")).toString();
    if (!timeFormat.isEmpty() && (mode != QStringLiteral("value") || !timeTopics().contains(topics.value(0))
        || timeFormat.size() > 64 || timeFormat.contains(QLatin1Char('\r')) || timeFormat.contains(QLatin1Char('\n')))) {
        error(tr("Time format requires an appended value of %1, at most 64 characters on one line")
            .arg(timeTopics().join(QStringLiteral(", "))));
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
    if (!timeFormat.isEmpty()) output.insert(QStringLiteral("timeFormat"), timeFormat);
    return true;
}

bool TcpControlManager::normalizeIncomingClient(const QVariantMap &input, const QVariantList &servers, QVariantMap &output) {
    QString address = input.value(QStringLiteral("address")).toString().trimmed().toLower();
    if (address.startsWith(QLatin1Char('[')) && address.endsWith(QLatin1Char(']')))
        address = address.mid(1, address.size() - 2);
    if (address != QStringLiteral("*")) {
        QHostAddress host;
        if (!host.setAddress(address)) {
            error(tr("Incoming client requires a numeric IPv4/IPv6 address, or * for any client"));
            return false;
        }
        // Clients are matched by the canonical address the listener reports.
        bool ipv4 = false;
        const quint32 v4 = host.toIPv4Address(&ipv4);
        if (ipv4) host = QHostAddress(v4);
        host.setScopeId({});
        address = host.toString().toLower();
    }
    // Profiles subscribe to all topics on connect unless eventTopics says otherwise.
    QStringList topics = input.contains(QStringLiteral("eventTopics"))
        ? input.value(QStringLiteral("eventTopics")).toStringList() : QStringList{QStringLiteral("*")};
    if (topics.contains(QStringLiteral("*"))) topics = {QStringLiteral("*")};
    // excludedTopics removes single topics from *, so topics added later are still subscribed.
    QStringList excluded;
    if (topics.contains(QStringLiteral("*")))
        for (const auto &value : input.value(QStringLiteral("excludedTopics")).toStringList())
            if (!value.trimmed().isEmpty() && !excluded.contains(value.trimmed())) excluded.append(value.trimmed());
    if (!ControlEventFeed::validateTopics(topics) || excluded.contains(QStringLiteral("*"))
        || !ControlEventFeed::validateTopics(excluded)) {
        error(tr("Unknown incoming client event topic"));
        return false;
    }
    const QString format = input.value(QStringLiteral("eventFormat"), QStringLiteral("json")).toString();
    const QString commandServerId = input.value(QStringLiteral("commandServerId")).toString();
    if (format != QStringLiteral("json") && format != QStringLiteral("commands")) {
        error(tr("Incoming client event format must be json or commands"));
        return false;
    }
    if (format == QStringLiteral("commands") && find(servers, commandServerId) < 0) {
        error(tr("Command structure requires an existing TCP server whose commands are sent"));
        return false;
    }
    const QString sendHex = input.value(QStringLiteral("sendTerminatorHex"), QStringLiteral("0a")).toString();
    static const QRegularExpression hex(QStringLiteral("^(?:[0-9a-fA-F]{2}){0,256}$"));
    const bool isDefault = address == defaultIncomingClientId();
    QString name = input.value(QStringLiteral("name")).toString().trimmed();
    if (isDefault && name.isEmpty()) name = builtInIncomingClient().value(QStringLiteral("name")).toString();
    if (name.isEmpty() || !hex.match(sendHex).hasMatch()) {
        error(tr("Incoming client requires a name and a hexadecimal send terminator"));
        return false;
    }
    // The default profile always applies to clients without an enabled profile of their own.
    output = { {QStringLiteral("id"), address}, {QStringLiteral("name"), name},
        {QStringLiteral("address"), address}, {QStringLiteral("enabled"), isDefault || input.value(QStringLiteral("enabled"), true).toBool()},
        {QStringLiteral("eventTopics"), topics}, {QStringLiteral("eventFormat"), format},
        {QStringLiteral("sendTerminatorHex"), sendHex.toLower()},
        {QStringLiteral("sendReplies"), input.value(QStringLiteral("sendReplies"), true).toBool()},
        {QStringLiteral("sendInitialUpdate"), input.value(QStringLiteral("sendInitialUpdate"), true).toBool()} };
    if (!excluded.isEmpty()) output.insert(QStringLiteral("excludedTopics"), excluded);
    if (format == QStringLiteral("commands")) output.insert(QStringLiteral("commandServerId"), commandServerId);
    return true;
}

QVariantMap TcpControlManager::builtInIncomingClient() {
    return { {QStringLiteral("id"), defaultIncomingClientId()}, {QStringLiteral("name"), tr("Default profile")},
        {QStringLiteral("address"), defaultIncomingClientId()}, {QStringLiteral("enabled"), true},
        {QStringLiteral("eventTopics"), QStringList{QStringLiteral("*")}}, {QStringLiteral("eventFormat"), QStringLiteral("json")},
        {QStringLiteral("sendTerminatorHex"), QStringLiteral("0a")}, {QStringLiteral("sendReplies"), true},
        {QStringLiteral("sendInitialUpdate"), true} };
}

QVariantList TcpControlManager::withDefaultIncomingClient(QVariantList incomingClients) {
    if (find(incomingClients, defaultIncomingClientId()) < 0) incomingClients.prepend(builtInIncomingClient());
    return incomingClients;
}

bool TcpControlManager::persist(const QVariantList &servers, const QVariantList &commands, const QVariantList &incomingClients) {
    if (!m_configurationPath.isEmpty()) {
        if (!QDir().mkpath(QFileInfo(m_configurationPath).absolutePath())) {
            error(tr("Cannot create TCP configuration directory"));
            return false;
        }
        QSaveFile file(m_configurationPath);
        const auto bytes = QJsonDocument(QJsonObject {
            {QStringLiteral("version"), 1}, {QStringLiteral("servers"), QJsonArray::fromVariantList(servers)},
            {QStringLiteral("commands"), QJsonArray::fromVariantList(commands)},
            {QStringLiteral("incomingClients"), QJsonArray::fromVariantList(incomingClients)} }).toJson();
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
        || root.value(QStringLiteral("version")).toInt() != 1 || !root.value(QStringLiteral("servers")).isArray() || !root.value(QStringLiteral("commands")).isArray()
        || (root.contains(QStringLiteral("incomingClients")) && !root.value(QStringLiteral("incomingClients")).isArray())) {
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
    ids.clear();
    QVariantList incomingClients;
    for (const auto value : root.value(QStringLiteral("incomingClients")).toArray()) {
        QVariantMap profile;
        auto input = value.toObject().toVariantMap();
        const QString oldServerId = input.value(QStringLiteral("commandServerId")).toString();
        if (migratedIds.contains(oldServerId)) input[QStringLiteral("commandServerId")] = migratedIds.value(oldServerId);
        if (!value.isObject() || !normalizeIncomingClient(input, servers, profile))
            return false;
        const QString id = profile.value(QStringLiteral("id")).toString();
        if (ids.contains(id)) {
            error(tr("Duplicate incoming TCP client address"));
            return false;
        }
        ids.insert(id);
        incomingClients.append(profile);
    }
    if (idsChanged && sourcePath.isEmpty() && !persist(servers, commands, incomingClients)) return false;
    m_servers = servers;
    m_commands = commands;
    clearCommandPayloads();
    m_incomingClients = withDefaultIncomingClient(incomingClients);
    applyIncomingProfiles();
    resetManualStops();
    m_subscriptions.clear();
    m_lastError.clear();
    reconcile();
    Q_EMIT serversChanged();
    Q_EMIT commandsChanged();
    Q_EMIT incomingClientsChanged();
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
    auto incomingClients = m_incomingClients;
    if (index >= 0 && oldId != id) {
        for (auto &entry : commands) {
            auto command = entry.toMap();
            if (command.value(QStringLiteral("serverId")).toString() == oldId) {
                command[QStringLiteral("serverId")] = id;
                entry = command;
            }
        }
        for (auto &entry : incomingClients) {
            auto client = entry.toMap();
            if (client.value(QStringLiteral("commandServerId")).toString() == oldId) {
                client[QStringLiteral("commandServerId")] = id;
                entry = client;
            }
        }
    }
    if (index < 0) servers.append(server); else servers[index] = server;
    if (!persist(servers, commands, incomingClients))
        return {};
    // Servers without connectOnStartup keep their current connection state when edited.
    const auto *previous = m_clients.value(index >= 0 ? oldId : id);
    const bool wasRunning = previous && previous->connectionState() != TcpControlClient::Stopped
        && previous->connectionState() != TcpControlClient::Disconnecting;
    m_servers = servers;
    m_commands = commands;
    if (oldId != id) clearCommandPayloads();
    if (m_incomingClients != incomingClients) {
        m_incomingClients = incomingClients;
        applyIncomingProfiles();
        Q_EMIT incomingClientsChanged();
    }
    if (oldId != id) {
        m_manualStops.remove(oldId);
        m_subscriptions.remove(oldId);
    }
    if (server.value(QStringLiteral("connectOnStartup")).toBool() || wasRunning) m_manualStops.remove(id);
    else m_manualStops.insert(id);
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
    for (const auto &value : m_incomingClients) {
        if (value.toMap().value(QStringLiteral("commandServerId")).toString() == id) {
            error(tr("Reassign incoming clients that use this server's command structure before deleting the server"));
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
    clearCommandPayloads(id);
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
    clearCommandPayloads(id);
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
        resetManualStops();
    reconcile();
    Q_EMIT activeChanged();
}

void TcpControlManager::resetManualStops() {
    m_manualStops.clear();
    for (const auto &value : m_servers) {
        const auto server = value.toMap();
        if (!server.value(QStringLiteral("connectOnStartup"), true).toBool())
            m_manualStops.insert(server.value(QStringLiteral("id")).toString());
    }
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
                // A new or restored connection gets the full current state, like the "all" command.
                // Queued so connection-state handlers finish before the first payload is written.
                if (state == TcpControlClient::Connected)
                    QMetaObject::invokeMethod(this, [this, id] {
                        auto *current = m_clients.value(id);
                        if (m_active && m_eventFeed && current && current->isConnected()) triggerAll(id);
                    }, Qt::QueuedConnection);
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

bool TcpControlManager::setCommandTimeFormat(const QString &id, const QString &timeFormat) {
    auto item = command(id);
    if (item.isEmpty()) { error(tr("TCP command does not exist")); return false; }
    if (timeFormat.isEmpty()) item.remove(QStringLiteral("timeFormat"));
    else item[QStringLiteral("timeFormat")] = timeFormat;
    return !saveCommand(item).isEmpty();
}

QStringList TcpControlManager::timeTopics() {
    return {QStringLiteral("position"), QStringLiteral("remaining"), QStringLiteral("duration")};
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

void TcpControlManager::clearCommandPayloads(const QString &id) {
    if (id.isEmpty()) m_lastValuePayloads.clear();
    else m_lastValuePayloads.remove(id);
    for (auto &session : m_sessions) {
        if (id.isEmpty()) session.lastValuePayloads.clear();
        else session.lastValuePayloads.remove(id);
    }
}

std::optional<QByteArray> TcpControlManager::commandPayload(const QVariantMap &item, const QJsonObject &event) {
    QByteArray payload = item.value(QStringLiteral("encoding")).toString() == QStringLiteral("hex")
        ? QByteArray::fromHex(item.value(QStringLiteral("payload")).toByteArray()) : item.value(QStringLiteral("payload")).toString().toUtf8();
    if (item.value(QStringLiteral("eventPayload")).toString() != QStringLiteral("value"))
        return payload;
    QJsonValue value = event.value(QStringLiteral("value"));
    if (event.isEmpty() && m_eventFeed) {
        const auto topics = item.value(QStringLiteral("eventTopics")).toStringList();
        value = m_eventFeed->read(topics).value(QStringLiteral("values")).toObject().value(topics.first());
    }
    if (value.isUndefined())
        return std::nullopt;
    const int charsPerItem = item.value(QStringLiteral("charsPerItem")).toInt();
    QString topic = event.value(QStringLiteral("topic")).toString();
    if (topic.isEmpty()) topic = item.value(QStringLiteral("eventTopics")).toStringList().value(0);
    const QString timeFormat = item.value(QStringLiteral("timeFormat")).toString();
    if (!timeFormat.isEmpty() && value.isDouble()) {
        payload += formatSeconds(value.toDouble(), timeFormat);
    } else {
        // Lists use the REST formatters so TCP and REST send identical text. The formatter
        // returns nullopt for width 0 except for topics with a text form at every width.
        const auto formatted = m_listFormatter ? m_listFormatter(topic, charsPerItem) : std::nullopt;
        payload += formatted ? *formatted : eventValueBytes(value, charsPerItem);
    }
    return payload;
}

bool TcpControlManager::dispatchCommand(const QString &id, const QJsonObject &event, bool skipUnchanged) {
    const auto item = command(id);
    const QString serverId = item.value(QStringLiteral("serverId")).toString();
    auto *client = m_clients.value(serverId);
    const int serverIndex = find(m_servers, serverId);
    bool accepted = false;
    if (!m_active || item.isEmpty() || !client || serverIndex < 0
        || !m_servers.at(serverIndex).toMap().value(QStringLiteral("enabled")).toBool() || !client->isConnected()) {
        error(tr("TCP command rejected: missing command or server is disabled/disconnected"));
    } else {
        const auto payload = commandPayload(item, event);
        if (!payload) {
            error(tr("Event value is unavailable"));
            Q_EMIT commandTriggered(id, false);
            return false;
        }
        const bool isValue = item.value(QStringLiteral("eventPayload")).toString() == QStringLiteral("value");
        if (skipUnchanged && isValue && m_lastValuePayloads.contains(id) && m_lastValuePayloads.value(id) == *payload)
            return true;
        accepted = item.value(QStringLiteral("raw")).toBool() ? client->sendBinary(*payload) : client->sendMessage(*payload);
        if (!accepted) error(client->lastError());
        else { m_lastError.clear(); Q_EMIT lastErrorChanged(); }
        if (accepted && isValue) m_lastValuePayloads.insert(id, *payload);
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
    // Automatic dispatch skips disabled or disconnected servers without an error; such a server may
    // only define the structure for incoming clients, and a full update follows every (re)connect.
    const auto serverReady = [this](const QVariantMap &command) {
        const QString serverId = command.value(QStringLiteral("serverId")).toString();
        const int index = find(m_servers, serverId);
        const auto *client = m_clients.value(serverId);
        return index >= 0 && m_servers.at(index).toMap().value(QStringLiteral("enabled")).toBool()
            && client && client->isConnected();
    };
    // Work on a copy: commandTriggered handlers can edit configuration.
    const auto commands = m_commands;
    for (const auto &value : commands) {
        const auto command = value.toMap();
        if (matches(command.value(QStringLiteral("eventTopics")).toStringList()) && serverReady(command))
            dispatchCommand(command.value(QStringLiteral("id")).toString(), event, true);
    }
    // Iterate IDs: sending can disconnect a slow client, which removes its session.
    const auto sessions = m_sessions.keys();
    for (const auto session : sessions) sendIncomingEvent(session, event);
    // Only formatted output depends on the source topic; an empty event reads the cached value.
    for (const auto &dependent : m_topicDependencies.value(topic)) {
        for (const auto &value : commands) {
            const auto command = value.toMap();
            if (command.value(QStringLiteral("eventPayload")).toString() == QStringLiteral("value")
                && command.value(QStringLiteral("charsPerItem")).toInt() > 0
                && command.value(QStringLiteral("eventTopics")).toStringList().contains(dependent) && serverReady(command))
                dispatchCommand(command.value(QStringLiteral("id")).toString(), {}, true);
        }
        for (const auto session : sessions) {
            const auto it = m_sessions.constFind(session);
            if (it == m_sessions.cend()
                || (!it->topics.contains(QStringLiteral("*")) && !it->topics.contains(dependent))) continue;
            for (const auto &value : incomingCommands(session)) {
                const auto command = value.toMap();
                if (command.value(QStringLiteral("eventPayload")).toString() == QStringLiteral("value")
                    && command.value(QStringLiteral("charsPerItem")).toInt() > 0
                    && command.value(QStringLiteral("eventTopics")).toStringList().contains(dependent))
                    sendIncomingCommand(session, command, {}, true);
            }
        }
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
    clearCommandPayloads();
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

QVariantMap TcpControlManager::incomingClientProfile(const QString &address) const {
    QVariantMap fallback = builtInIncomingClient();
    for (const auto &value : m_incomingClients) {
        const auto profile = value.toMap();
        const QString profileAddress = profile.value(QStringLiteral("address")).toString();
        if (profileAddress == defaultIncomingClientId()) fallback = profile;
        else if (profileAddress == address && profile.value(QStringLiteral("enabled")).toBool()) return profile;
    }
    return fallback;
}

QString TcpControlManager::saveIncomingClient(const QVariantMap &input) {
    QVariantMap profile;
    if (!normalizeIncomingClient(input, m_servers, profile))
        return {};
    const QString id = profile.value(QStringLiteral("id")).toString();
    const QString oldId = input.value(QStringLiteral("id")).toString();
    if (oldId == defaultIncomingClientId() && id != oldId) {
        error(tr("The default profile always has address *; add a new profile for a specific address"));
        return {};
    }
    // Saving a * profile always edits the default profile.
    const int index = id == defaultIncomingClientId() ? find(m_incomingClients, id)
        : oldId.isEmpty() ? -1 : find(m_incomingClients, oldId);
    if (!oldId.isEmpty() && index < 0 && oldId != id) {
        error(tr("Incoming TCP client no longer exists"));
        return {};
    }
    const int duplicate = find(m_incomingClients, id);
    if (duplicate >= 0 && duplicate != index) {
        error(tr("An incoming TCP client with this address already exists"));
        return {};
    }
    auto incomingClients = m_incomingClients;
    if (index < 0) incomingClients.append(profile); else incomingClients[index] = profile;
    if (!persist(m_servers, m_commands, incomingClients))
        return {};
    m_incomingClients = incomingClients;
    applyIncomingProfiles();
    Q_EMIT incomingClientsChanged();
    return id;
}

bool TcpControlManager::removeIncomingClient(const QString &id) {
    if (id == defaultIncomingClientId()) {
        error(tr("The default profile cannot be deleted; edit it instead"));
        return false;
    }
    const int index = find(m_incomingClients, id);
    if (index < 0)
        return false;
    auto incomingClients = m_incomingClients;
    incomingClients.removeAt(index);
    if (!persist(m_servers, m_commands, incomingClients))
        return false;
    m_incomingClients = incomingClients;
    applyIncomingProfiles();
    Q_EMIT incomingClientsChanged();
    return true;
}

QVariantList TcpControlManager::incomingSessions() const {
    QVariantList result;
    for (auto it = m_sessions.cbegin(); it != m_sessions.cend(); ++it) {
        const auto &profile = it->profile;
        result.append(QVariantMap {
            {QStringLiteral("session"), QString::number(it.key())}, {QStringLiteral("address"), it->address},
            {QStringLiteral("port"), it->port}, {QStringLiteral("profileId"), profile.value(QStringLiteral("id"))},
            {QStringLiteral("profileName"), profile.value(QStringLiteral("name"))},
            {QStringLiteral("eventFormat"), profile.value(QStringLiteral("eventFormat"), QStringLiteral("json"))},
            {QStringLiteral("topics"), it->topics} });
    }
    return result;
}

void TcpControlManager::applyIncomingProfiles() {
    const auto sessions = m_sessions.keys();
    for (const auto session : sessions) applyIncomingProfile(session);
    if (!sessions.isEmpty()) Q_EMIT incomingSessionsChanged();
}

void TcpControlManager::applyIncomingProfile(quint64 id) {
    const auto it = m_sessions.find(id);
    if (it == m_sessions.end()) return;
    const auto profile = incomingClientProfile(it->address);
    if (profile != it->profile) it->lastValuePayloads.clear();
    // Connected clients follow a changed on-connect subscription, replacing their own subscriptions.
    const bool resubscribe = !it->profile.isEmpty() && connectTopics(profile) != connectTopics(it->profile);
    it->profile = profile;
    m_listener->setTerminator(id, QByteArray::fromHex(profile.value(QStringLiteral("sendTerminatorHex"), QStringLiteral("0a")).toByteArray()));
    // Clients such as Medialon parse every received frame against their monitored values;
    // without replies they only receive the updates.
    m_listener->setRepliesEnabled(id, profile.value(QStringLiteral("sendReplies"), true).toBool());
    if (resubscribe) {
        it->topics.clear();
        const auto topics = connectTopics(profile);
        if (!topics.isEmpty()) subscribeIncoming(id, topics);
    }
}

QVariantList TcpControlManager::incomingCommands(quint64 id) const {
    QVariantList result;
    const auto it = m_sessions.constFind(id);
    if (it == m_sessions.cend() || it->profile.value(QStringLiteral("eventFormat")).toString() != QStringLiteral("commands"))
        return result;
    // The client receives the event commands of the referenced server, as that server would.
    const QString serverId = it->profile.value(QStringLiteral("commandServerId")).toString();
    for (const auto &value : m_commands) {
        const auto command = value.toMap();
        if (command.value(QStringLiteral("serverId")).toString() == serverId
            && !command.value(QStringLiteral("eventTopics")).toStringList().isEmpty())
            result.append(command);
    }
    return result;
}

std::optional<QJsonObject> TcpControlManager::handleIncomingRequest(quint64 id, const QString &path, const QJsonObject &parameters) {
    if (path != QStringLiteral("/subscribe") && path != QStringLiteral("/unsubscribe") && path != QStringLiteral("/full_update"))
        return std::nullopt;
    const auto reply = [](int status, const QString &message, const QStringList &topics = {}) {
        QJsonObject object{{QStringLiteral("accepted"), status == 200}, {QStringLiteral("status"), status},
            {QStringLiteral("message"), message}};
        if (status == 200) object.insert(QStringLiteral("topics"), QJsonArray::fromStringList(topics));
        return object;
    };
    if (!m_active || !m_eventFeed || !m_sessions.contains(id))
        return reply(503, tr("Player is not ready"));
    // Comma-separated topics; omitted or empty selects all topics.
    QStringList topics;
    for (const auto &topic : parameters.value(QStringLiteral("topics")).toString().split(QLatin1Char(',')))
        if (!topic.trimmed().isEmpty()) topics.append(topic.trimmed());
    if (topics.isEmpty()) topics = {QStringLiteral("*")};
    if (!ControlEventFeed::validateTopics(topics))
        return reply(400, tr("Unknown event topic; see /event_topics"));
    if (path == QStringLiteral("/subscribe")) {
        subscribeIncoming(id, topics);
        return reply(200, tr("Subscribed"), m_sessions.value(id).topics);
    }
    if (path == QStringLiteral("/unsubscribe")) {
        auto &current = m_sessions[id].topics;
        if (topics.contains(QStringLiteral("*"))) current.clear();
        else {
            // Unsubscribing single topics from * keeps all other topics.
            if (current.contains(QStringLiteral("*"))) current = ControlEventFeed::availableTopics();
            for (const auto &topic : topics) current.removeAll(topic);
        }
        Q_EMIT incomingSessionsChanged();
        return reply(200, tr("Unsubscribed"), current);
    }
    QMetaObject::invokeMethod(this, [this, id, topics] { sendIncomingUpdate(id, topics); }, Qt::QueuedConnection);
    return reply(200, tr("Full update"), topics);
}

void TcpControlManager::subscribeIncoming(quint64 id, const QStringList &topics, bool sendValues) {
    const auto it = m_sessions.find(id);
    if (it == m_sessions.end()) return;
    if (topics.contains(QStringLiteral("*"))) it->topics = {QStringLiteral("*")};
    else if (!it->topics.contains(QStringLiteral("*")))
        for (const auto &topic : topics) if (!it->topics.contains(topic)) it->topics.append(topic);
    Q_EMIT incomingSessionsChanged();
    if (!sendValues) return;
    // The subscriber starts from the current values; queued so a reply is written first.
    QMetaObject::invokeMethod(this, [this, id, topics] { sendIncomingUpdate(id, topics); }, Qt::QueuedConnection);
}

void TcpControlManager::sendIncomingUpdate(quint64 id, const QStringList &topics) {
    const auto it = m_sessions.constFind(id);
    if (!m_active || !m_eventFeed || it == m_sessions.cend()) return;
    const auto snapshot = m_eventFeed->read(topics);
    if (it->profile.value(QStringLiteral("eventFormat")).toString() != QStringLiteral("commands")) {
        m_listener->send(id, QJsonDocument(snapshot).toJson(QJsonDocument::Compact));
        return;
    }
    // Like the server's full update: every event command of the requested topics, once.
    const auto values = snapshot.value(QStringLiteral("values")).toObject();
    for (const auto &value : incomingCommands(id)) {
        const auto command = value.toMap();
        const QString topic = command.value(QStringLiteral("eventTopics")).toStringList().first();
        if (!topics.contains(QStringLiteral("*")) && !topics.contains(topic)) continue;
        sendIncomingCommand(id, command, {{QStringLiteral("topic"), topic}, {QStringLiteral("value"), values.value(topic)}}, false);
    }
}

void TcpControlManager::sendIncomingEvent(quint64 id, const QJsonObject &event) {
    const auto it = m_sessions.constFind(id);
    if (it == m_sessions.cend()) return;
    const QString topic = event.value(QStringLiteral("topic")).toString();
    const auto matches = [&topic](const QStringList &topics) {
        return topics.contains(QStringLiteral("*")) || topics.contains(topic);
    };
    if (!matches(it->topics)) return;
    if (it->profile.value(QStringLiteral("eventFormat")).toString() != QStringLiteral("commands")) {
        m_listener->send(id, QJsonDocument(event).toJson(QJsonDocument::Compact));
        return;
    }
    for (const auto &value : incomingCommands(id)) {
        const auto command = value.toMap();
        if (matches(command.value(QStringLiteral("eventTopics")).toStringList()))
            sendIncomingCommand(id, command, event, true);
    }
}

void TcpControlManager::sendIncomingCommand(quint64 id, const QVariantMap &command, const QJsonObject &event, bool skipUnchanged) {
    const auto it = m_sessions.constFind(id);
    if (it == m_sessions.cend()) return;
    const QString commandId = command.value(QStringLiteral("id")).toString();
    const auto payload = commandPayload(command, event);
    if (!payload) return;
    const bool isValue = command.value(QStringLiteral("eventPayload")).toString() == QStringLiteral("value");
    if (skipUnchanged && isValue && it->lastValuePayloads.contains(commandId) && it->lastValuePayloads.value(commandId) == *payload)
        return;
    // A client that does not read its updates is disconnected, which removes its session.
    if (!m_listener->send(id, *payload, command.value(QStringLiteral("raw")).toBool())) return;
    const auto session = m_sessions.find(id);
    if (session != m_sessions.end() && isValue)
        session->lastValuePayloads.insert(commandId, *payload);
}
