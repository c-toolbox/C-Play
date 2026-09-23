/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mediamtxmodel.h"
#include "mediamtxcredentials.h"
#ifdef OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <cpp-httplib/httplib.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#pragma warning(disable : 4996)

namespace {

const QString serversFilePath = QStringLiteral("./data/mediamtx-servers.json");
const QString predefinedStreamsFilePath = QStringLiteral("./data/predefined-streams.json");

QString percentEncode(const QString &value) {
    return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

// MediaMTX addresses are given as "host:port" or ":port".
int portFromAddress(const QString &address, int fallback) {
    int colon = address.lastIndexOf(QLatin1Char(':'));
    if (colon < 0)
        return fallback;
    bool ok = false;
    int port = address.mid(colon + 1).toInt(&ok);
    return (ok && port > 0) ? port : fallback;
}

// MediaMTX reports protocol switches as JSON booleans ("webrtc": true). A few string
// spellings are accepted as well, so a differently shaped config never silently
// disables WebRTC in the UI.
bool configEnabled(const QJsonValue &value) {
    if (value.isBool())
        return value.toBool();
    const QString s = value.toString().trimmed().toLower();
    return s == QLatin1String("true") || s == QLatin1String("yes");
}

// Encryption levels are either the strings "no"/"optional"/"strict" (RTSP/RTMP) or
// plain booleans (WebRTC). Returns true when TLS is enforced.
bool configEncryptionStrict(const QJsonValue &value) {
    if (value.isBool())
        return value.toBool();
    const QString s = value.toString().trimmed().toLower();
    return s == QLatin1String("strict") || s == QLatin1String("true") || s == QLatin1String("yes");
}

} // namespace

// --- MediaMtxServersModel ---

MediaMtxServersModel::MediaMtxServersModel(QObject *parent)
    : QAbstractListModel(parent) {
}

MediaMtxServersModel::~MediaMtxServersModel() {
}

int MediaMtxServersModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_servers.size();
}

QVariant MediaMtxServersModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || !checkIndex(index))
        return QVariant();

    const Server &s = m_servers.at(index.row());
    switch (role) {
    case nameRole:
        return s.name;
    case hostRole:
        return s.host;
    case apiPortRole:
        return s.apiPort;
    case apiSchemeRole:
        return s.apiScheme;
    case usernameRole:
        return s.username;
    case rtspPortRole:
        return s.rtspPort;
    case rtspSchemeRole:
        return s.rtspScheme;
    case rtspTransportRole:
        return s.rtspTransport;
    case autoDetectRtspRole:
        return s.autoDetectRtsp;
    case enabledRole:
        return s.enabled;
    case hasPasswordRole:
        return !s.password.isEmpty();
    case webrtcPortRole:
        return s.webrtcPort;
    case webrtcSchemeRole:
        return s.webrtcScheme;
    case autoDetectWebRtcRole:
        return s.autoDetectWebRtc;
    case srtPortRole:
        return s.srtPort;
    case autoDetectSrtRole:
        return s.autoDetectSrt;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MediaMtxServersModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[nameRole] = "name";
    roles[hostRole] = "host";
    roles[apiPortRole] = "apiPort";
    roles[apiSchemeRole] = "apiScheme";
    roles[usernameRole] = "username";
    roles[rtspPortRole] = "rtspPort";
    roles[rtspSchemeRole] = "rtspScheme";
    roles[rtspTransportRole] = "rtspTransport";
    roles[autoDetectRtspRole] = "autoDetectRtsp";
    roles[enabledRole] = "enabled";
    roles[hasPasswordRole] = "hasPassword";
    roles[webrtcPortRole] = "webrtcPort";
    roles[webrtcSchemeRole] = "webrtcScheme";
    roles[autoDetectWebRtcRole] = "autoDetectWebRtc";
    roles[srtPortRole] = "srtPort";
    roles[autoDetectSrtRole] = "autoDetectSrt";
    return roles;
}

void MediaMtxServersModel::updateServersList() {
    QFile serversFile(serversFilePath);
    if (!serversFile.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonDocument doc(QJsonDocument::fromJson(serversFile.readAll()));
    serversFile.close();
    if (!doc.isObject())
        return;

    QJsonArray arr = doc.object().value(QStringLiteral("servers")).toArray();

    // Keep already entered session passwords when the list is reloaded.
    QHash<QString, QString> previousPasswords;
    for (const Server &s : std::as_const(m_servers)) {
        if (!s.password.isEmpty())
            previousPasswords.insert(s.name, s.password);
    }

    beginResetModel();
    m_servers.clear();
    for (auto v : arr) {
        QJsonObject o = v.toObject();
        Server s;
        s.name = o.value(QStringLiteral("name")).toString();
        s.host = o.value(QStringLiteral("host")).toString();
        s.apiPort = o.value(QStringLiteral("apiPort")).toInt(9997);
        s.apiScheme = o.value(QStringLiteral("apiScheme")).toString(QStringLiteral("http"));
        s.username = o.value(QStringLiteral("username")).toString();
        s.rtspPort = o.value(QStringLiteral("rtspPort")).toInt(8554);
        s.rtspScheme = o.value(QStringLiteral("rtspScheme")).toString(QStringLiteral("rtsp"));
        s.rtspTransport = o.value(QStringLiteral("rtspTransport")).toString(QStringLiteral("tcp"));
        s.webrtcPort = o.value(QStringLiteral("webrtcPort")).toInt(8889);
        s.webrtcScheme = o.value(QStringLiteral("webrtcScheme")).toString(QStringLiteral("http"));
        s.srtPort = o.value(QStringLiteral("srtPort")).toInt(8890);
        s.autoDetectRtsp = o.value(QStringLiteral("autoDetectRtsp")).toBool(true);
        s.autoDetectWebRtc = o.value(QStringLiteral("autoDetectWebRtc")).toBool(true);
        s.autoDetectSrt = o.value(QStringLiteral("autoDetectSrt")).toBool(true);
        s.enabled = o.value(QStringLiteral("enabled")).toBool(true);
        s.password = previousPasswords.value(s.name);
        if (!s.name.isEmpty() && !s.host.isEmpty())
            m_servers.append(s);
    }
    endResetModel();
    Q_EMIT serversListChanged();
}

int MediaMtxServersModel::getNumberOfServers() const {
    return m_servers.size();
}

bool MediaMtxServersModel::isValidIndex(int index) const {
    return index >= 0 && index < m_servers.size();
}

void MediaMtxServersModel::addServer(const QString &name, const QString &host, int apiPort,
                                     const QString &apiScheme, const QString &username,
                                     int rtspPort, const QString &rtspScheme,
                                     const QString &rtspTransport, int webrtcPort,
                                     const QString &webrtcScheme, int srtPort,
                                     bool autoDetectRtsp, bool autoDetectWebRtc,
                                     bool autoDetectSrt, bool enabled) {
    Server s;
    s.name = name;
    s.host = host;
    s.apiPort = apiPort > 0 ? apiPort : 9997;
    s.apiScheme = apiScheme.isEmpty() ? QStringLiteral("http") : apiScheme;
    s.username = username;
    s.rtspPort = rtspPort > 0 ? rtspPort : 8554;
    s.rtspScheme = rtspScheme.isEmpty() ? QStringLiteral("rtsp") : rtspScheme;
    s.rtspTransport = rtspTransport.isEmpty() ? QStringLiteral("tcp") : rtspTransport;
    s.webrtcPort = webrtcPort > 0 ? webrtcPort : 8889;
    s.webrtcScheme = webrtcScheme.isEmpty() ? QStringLiteral("http") : webrtcScheme;
    s.srtPort = srtPort > 0 ? srtPort : 8890;
    s.autoDetectRtsp = autoDetectRtsp;
    s.autoDetectWebRtc = autoDetectWebRtc;
    s.autoDetectSrt = autoDetectSrt;
    s.enabled = enabled;

    beginInsertRows(QModelIndex(), m_servers.size(), m_servers.size());
    m_servers.append(s);
    endInsertRows();

    saveServersToFile();
    Q_EMIT serversListChanged();
}

void MediaMtxServersModel::updateServer(int index, const QString &name, const QString &host, int apiPort,
                                        const QString &apiScheme, const QString &username,
                                        int rtspPort, const QString &rtspScheme,
                                        const QString &rtspTransport, int webrtcPort,
                                        const QString &webrtcScheme, int srtPort,
                                        bool autoDetectRtsp, bool autoDetectWebRtc,
                                        bool autoDetectSrt, bool enabled) {
    if (!isValidIndex(index))
        return;

    Server &s = m_servers[index];
    s.name = name;
    s.host = host;
    s.apiPort = apiPort > 0 ? apiPort : 9997;
    s.apiScheme = apiScheme.isEmpty() ? QStringLiteral("http") : apiScheme;
    s.username = username;
    s.rtspPort = rtspPort > 0 ? rtspPort : 8554;
    s.rtspScheme = rtspScheme.isEmpty() ? QStringLiteral("rtsp") : rtspScheme;
    s.rtspTransport = rtspTransport.isEmpty() ? QStringLiteral("tcp") : rtspTransport;
    s.webrtcPort = webrtcPort > 0 ? webrtcPort : 8889;
    s.webrtcScheme = webrtcScheme.isEmpty() ? QStringLiteral("http") : webrtcScheme;
    s.srtPort = srtPort > 0 ? srtPort : 8890;
    s.autoDetectRtsp = autoDetectRtsp;
    s.autoDetectWebRtc = autoDetectWebRtc;
    s.autoDetectSrt = autoDetectSrt;
    s.enabled = enabled;

    Q_EMIT dataChanged(this->index(index, 0), this->index(index, 0));
    saveServersToFile();
    Q_EMIT serversListChanged();
}

void MediaMtxServersModel::removeServer(int index) {
    if (!isValidIndex(index))
        return;

    beginRemoveRows(QModelIndex(), index, index);
    m_servers.removeAt(index);
    endRemoveRows();

    saveServersToFile();
    Q_EMIT serversListChanged();
}

void MediaMtxServersModel::moveServer(int from, int to) {
    if (!isValidIndex(from) || !isValidIndex(to) || from == to)
        return;

    beginResetModel();
    m_servers.move(from, to);
    endResetModel();

    saveServersToFile();
    Q_EMIT serversListChanged();
}

QVariantMap MediaMtxServersModel::serverAt(int index) const {
    QVariantMap map;
    if (!isValidIndex(index))
        return map;

    const Server &s = m_servers.at(index);
    map.insert(QStringLiteral("name"), s.name);
    map.insert(QStringLiteral("host"), s.host);
    map.insert(QStringLiteral("apiPort"), s.apiPort);
    map.insert(QStringLiteral("apiScheme"), s.apiScheme);
    map.insert(QStringLiteral("username"), s.username);
    map.insert(QStringLiteral("rtspPort"), s.rtspPort);
    map.insert(QStringLiteral("rtspScheme"), s.rtspScheme);
    map.insert(QStringLiteral("rtspTransport"), s.rtspTransport);
    map.insert(QStringLiteral("webrtcPort"), s.webrtcPort);
    map.insert(QStringLiteral("webrtcScheme"), s.webrtcScheme);
    map.insert(QStringLiteral("srtPort"), s.srtPort);
    map.insert(QStringLiteral("autoDetectRtsp"), s.autoDetectRtsp);
    map.insert(QStringLiteral("autoDetectWebRtc"), s.autoDetectWebRtc);
    map.insert(QStringLiteral("autoDetectSrt"), s.autoDetectSrt);
    map.insert(QStringLiteral("enabled"), s.enabled);
    map.insert(QStringLiteral("hasPassword"), !s.password.isEmpty());
    return map;
}

void MediaMtxServersModel::setPassword(int index, const QString &password) {
    if (!isValidIndex(index))
        return;
    m_servers[index].password = password;
    Q_EMIT dataChanged(this->index(index, 0), this->index(index, 0));
}

bool MediaMtxServersModel::hasPassword(int index) const {
    return isValidIndex(index) && !m_servers.at(index).password.isEmpty();
}

QString MediaMtxServersModel::password(int index) const {
    return isValidIndex(index) ? m_servers.at(index).password : QString();
}

bool MediaMtxServersModel::hasStoredCredential(const QString &serverName, const QString &username) const {
    QString password;
    return MediaMtxCredentials::findStoredPassword(serverName, username, password);
}

QString MediaMtxServersModel::effectivePassword(int index) const {
    if (!isValidIndex(index))
        return QString();
    const Server &s = m_servers.at(index);
    if (!s.password.isEmpty())
        return s.password; // a manually entered password takes precedence
    QString stored;
    return MediaMtxCredentials::findStoredPassword(s.name, s.username, stored) ? stored : QString();
}

QVariantMap MediaMtxServersModel::authCredentials(int index) const {
    QVariantMap map;
    if (!isValidIndex(index))
        return map;
    map.insert(QStringLiteral("username"), username(index));
    map.insert(QStringLiteral("password"), effectivePassword(index));
    return map;
}

QString MediaMtxServersModel::apiBaseUrl(int index) const {
    if (!isValidIndex(index))
        return QString();
    const Server &s = m_servers.at(index);
    return s.apiScheme + QStringLiteral("://") + s.host + QStringLiteral(":") + QString::number(s.apiPort);
}

QString MediaMtxServersModel::username(int index) const {
    return isValidIndex(index) ? m_servers.at(index).username : QString();
}

QString MediaMtxServersModel::name(int index) const {
    return isValidIndex(index) ? m_servers.at(index).name : QString();
}

void MediaMtxServersModel::applyDetectedRtsp(int index, int port, const QString &scheme) {
    if (!isValidIndex(index))
        return;
    Server &s = m_servers[index];
    if (port > 0)
        s.rtspPort = port;
    if (!scheme.isEmpty())
        s.rtspScheme = scheme;
    Q_EMIT dataChanged(this->index(index, 0), this->index(index, 0));
}

void MediaMtxServersModel::applyDetectedWebRtc(int index, int port, const QString &scheme) {
    if (!isValidIndex(index))
        return;
    Server &s = m_servers[index];
    if (port > 0)
        s.webrtcPort = port;
    if (!scheme.isEmpty())
        s.webrtcScheme = scheme;
    Q_EMIT dataChanged(this->index(index, 0), this->index(index, 0));
}

void MediaMtxServersModel::applyDetectedSrt(int index, int port) {
    if (!isValidIndex(index))
        return;
    Server &s = m_servers[index];
    if (port > 0)
        s.srtPort = port;
    Q_EMIT dataChanged(this->index(index, 0), this->index(index, 0));
}

void MediaMtxServersModel::saveServersToFile() {
    QJsonArray arr;
    for (const Server &s : std::as_const(m_servers)) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), s.name);
        o.insert(QStringLiteral("host"), s.host);
        o.insert(QStringLiteral("apiPort"), s.apiPort);
        o.insert(QStringLiteral("apiScheme"), s.apiScheme);
        o.insert(QStringLiteral("username"), s.username);
        o.insert(QStringLiteral("rtspPort"), s.rtspPort);
        o.insert(QStringLiteral("rtspScheme"), s.rtspScheme);
        o.insert(QStringLiteral("rtspTransport"), s.rtspTransport);
        o.insert(QStringLiteral("webrtcPort"), s.webrtcPort);
        o.insert(QStringLiteral("webrtcScheme"), s.webrtcScheme);
        o.insert(QStringLiteral("srtPort"), s.srtPort);
        o.insert(QStringLiteral("autoDetectRtsp"), s.autoDetectRtsp);
        o.insert(QStringLiteral("autoDetectWebRtc"), s.autoDetectWebRtc);
        o.insert(QStringLiteral("autoDetectSrt"), s.autoDetectSrt);
        o.insert(QStringLiteral("enabled"), s.enabled);
        arr.append(o);
    }

    QJsonObject root;
    root.insert(QStringLiteral("servers"), arr);

    QFile serversFile(serversFilePath);
    if (!serversFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("Couldn't write mediamtx-servers file.");
        return;
    }
    serversFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    serversFile.close();
}

// --- MediaMtxWorker ---

MediaMtxWorker::MediaMtxWorker(QObject *parent)
    : QObject(parent) {
}

void MediaMtxWorker::doFetch(const QString &baseUrl, const QString &username, const QString &password,
                             bool includeConfiguredPaths, bool fetchGlobalConfig) {
    int statusCode = 0;
    QString error;
    QString configWarning;
    QJsonArray pathItems;
    QJsonArray configPathItems;
    QString globalConfigJson;

    if (baseUrl.isEmpty()) {
        Q_EMIT fetchFinished(0, QString(), QString(), QString(), QString(), QStringLiteral("Empty server address"));
        return;
    }

#ifndef OPENSSL_SUPPORT
    if (baseUrl.startsWith(QStringLiteral("https://"))) {
        Q_EMIT fetchFinished(0, QString(), QString(), QString(), QString(),
                             QStringLiteral("HTTPS is not supported (OpenSSL not available). Use HTTP instead."));
        return;
    }
#endif

    httplib::Client cli(baseUrl.toStdString());
    cli.set_connection_timeout(3, 0);
    cli.set_read_timeout(5, 0);
    cli.set_write_timeout(5, 0);
    cli.set_keep_alive(true);
    cli.set_follow_location(true);
    cli.set_default_headers({{"User-Agent", "C-Play/2.3"}, {"Accept", "application/json"}});
    if (!username.isEmpty())
        cli.set_basic_auth(username.toStdString(), password.toStdString());

    auto fetchList = [&](const std::string &endpoint, QJsonArray &outItems) -> bool {
        int page = 0;
        int pageCount = 1;
        while (page < pageCount) {
            std::string url = endpoint + "?page=" + std::to_string(page) + "&itemsPerPage=100";
            httplib::Result res = cli.Get(url);
            if (!res) {
                error = QString::fromStdString(httplib::to_string(res.error()));
                return false;
            }
            statusCode = res->status;
            if (statusCode < 200 || statusCode >= 300) {
                error = QStringLiteral("HTTP %1: %2").arg(statusCode).arg(QString::fromStdString(res->body).left(200));
                return false;
            }
            QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(res->body));
            if (!doc.isObject()) {
                error = QStringLiteral("Unexpected response from %1").arg(QString::fromStdString(endpoint));
                return false;
            }
            QJsonObject o = doc.object();
            for (auto v : o.value(QStringLiteral("items")).toArray())
                outItems.append(v);
            pageCount = o.value(QStringLiteral("pageCount")).toInt(1);
            page++;
            if (page > 100) // safety stop
                break;
        }
        return true;
    };

    if (!fetchList("/v3/paths/list", pathItems)) {
        Q_EMIT fetchFinished(statusCode, QString(), QString(), QString(), QString(), error);
        return;
    }

    if (includeConfiguredPaths) {
        // Configured-but-idle paths are a convenience, failures here are not fatal.
        QString savedError = error;
        if (!fetchList("/v3/config/paths/list", configPathItems)) {
            configPathItems = QJsonArray();
            error = savedError;
        }
    }

    // The global configuration drives WebRTC availability and RTSP/WebRTC auto-detect.
    // A failure here is not fatal (streams still work) but must be reported, otherwise
    // the UI would silently claim "the server has no WebRTC enabled".
    if (fetchGlobalConfig) {
        httplib::Result res = cli.Get("/v3/config/global/get");
        if (!res) {
            configWarning = QStringLiteral("request failed: %1")
                                .arg(QString::fromStdString(httplib::to_string(res.error())));
        } else if (res->status < 200 || res->status >= 300) {
            configWarning = QStringLiteral("HTTP %1").arg(res->status);
        } else {
            globalConfigJson = QString::fromStdString(res->body);
        }
    }

    Q_EMIT fetchFinished(statusCode,
                         QString::fromUtf8(QJsonDocument(pathItems).toJson(QJsonDocument::Compact)),
                         QString::fromUtf8(QJsonDocument(configPathItems).toJson(QJsonDocument::Compact)),
                         globalConfigJson,
                         configWarning,
                         QString());
}

// --- MediaMtxModel ---

MediaMtxModel::MediaMtxModel(QObject *parent)
    : QAbstractListModel(parent) {
    m_worker = new MediaMtxWorker();
    m_worker->moveToThread(&m_workerThread);
    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &MediaMtxModel::startFetch, m_worker, &MediaMtxWorker::doFetch);
    connect(m_worker, &MediaMtxWorker::fetchFinished, this, &MediaMtxModel::onFetchFinished);
    m_workerThread.start();
}

MediaMtxModel::~MediaMtxModel() {
    m_workerThread.quit();
    m_workerThread.wait();
}

void MediaMtxModel::setServersModel(MediaMtxServersModel *servers) {
    m_servers = servers;
}

int MediaMtxModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_streams.size();
}

QVariant MediaMtxModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || !checkIndex(index))
        return QVariant();

    const Stream &s = m_streams.at(index.row());
    switch (role) {
    case nameRole:
        return s.name;
    case serverNameRole:
        return s.serverName;
    case rtspUrlRole:
        return s.rtspUrl;
    case whepUrlRole:
        return s.whepUrl;
    case srtUrlRole:
        return s.srtUrl;
    case onlineRole:
        return s.online;
    case sourceTypeRole:
        return s.sourceType;
    case tracksRole:
        return s.tracks;
    case readersRole:
        return s.readers;
    case configuredOnlyRole:
        return s.configuredOnly;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MediaMtxModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[nameRole] = "name";
    roles[serverNameRole] = "serverName";
    roles[rtspUrlRole] = "rtspUrl";
    roles[whepUrlRole] = "whepUrl";
    roles[srtUrlRole] = "srtUrl";
    roles[onlineRole] = "online";
    roles[sourceTypeRole] = "sourceType";
    roles[tracksRole] = "tracks";
    roles[readersRole] = "readers";
    roles[configuredOnlyRole] = "configuredOnly";
    return roles;
}

int MediaMtxModel::getNumberOfStreams() const {
    return m_streams.size();
}

int MediaMtxModel::currentServerIndex() const {
    return m_currentServerIndex;
}

bool MediaMtxModel::refreshInProgress() const {
    return m_refreshInProgress;
}

QString MediaMtxModel::lastError() const {
    return m_lastError;
}

int MediaMtxModel::lastStatusCode() const {
    return m_lastStatusCode;
}

QString MediaMtxModel::lastSummary() const {
    return m_lastSummary;
}

bool MediaMtxModel::webRtcAvailable() const {
    return m_webrtcAvailable;
}

bool MediaMtxModel::srtAvailable() const {
    return m_srtAvailable;
}

QString MediaMtxModel::configWarning() const {
    return m_configWarning;
}

void MediaMtxModel::clear() {
    beginResetModel();
    m_streams.clear();
    endResetModel();
    Q_EMIT streamsListChanged();
}

void MediaMtxModel::refresh(int serverIndex) {
    if (!m_servers || m_refreshInProgress)
        return;

    QString baseUrl = m_servers->apiBaseUrl(serverIndex);
    if (baseUrl.isEmpty()) {
        m_lastError = QStringLiteral("No MediaMTX server selected");
        m_lastStatusCode = 0;
        m_lastSummary.clear();
        Q_EMIT responseChanged();
        return;
    }

    m_pendingServerIndex = serverIndex;
    m_testOnly = false;
    m_refreshInProgress = true;
    Q_EMIT refreshInProgressChanged();

    Q_EMIT startFetch(baseUrl, m_servers->username(serverIndex), m_servers->effectivePassword(serverIndex), true, true);
}

void MediaMtxModel::testConnection(int serverIndex) {
    if (!m_servers || m_refreshInProgress)
        return;

    QString baseUrl = m_servers->apiBaseUrl(serverIndex);
    if (baseUrl.isEmpty())
        return;

    m_pendingServerIndex = serverIndex;
    m_testOnly = true;
    m_refreshInProgress = true;
    Q_EMIT refreshInProgressChanged();

    Q_EMIT startFetch(baseUrl, m_servers->username(serverIndex), m_servers->effectivePassword(serverIndex), false, true);
}

void MediaMtxModel::onFetchFinished(int statusCode, const QString &pathsJson, const QString &configPathsJson,
                                    const QString &globalConfigJson, const QString &configWarning,
                                    const QString &error) {
    m_refreshInProgress = false;
    Q_EMIT refreshInProgressChanged();

    m_lastStatusCode = statusCode;
    m_lastError = error;
    // A failed or config-less fetch must not leave a stale "WebRTC/SRT available" state behind.
    m_webrtcAvailable = false;
    m_srtAvailable = false;
    m_configWarning = configWarning;

    const int serverIndex = m_pendingServerIndex;
    m_pendingServerIndex = -1;

    if (!error.isEmpty()) {
        m_lastSummary.clear();
        Q_EMIT responseChanged();
        return;
    }

    m_currentServerIndex = serverIndex;
    applyGlobalConfig(globalConfigJson);

    if (m_testOnly) {
        m_testOnly = false;
        m_lastSummary = QStringLiteral("Connected");
        Q_EMIT responseChanged();
        return;
    }

    const QString serverName = m_servers ? m_servers->name(serverIndex) : QString();

    beginResetModel();
    m_streams.clear();

    QJsonArray items = QJsonDocument::fromJson(pathsJson.toUtf8()).array();
    for (auto v : items) {
        QJsonObject o = v.toObject();
        Stream s;
        s.name = o.value(QStringLiteral("name")).toString();
        if (s.name.isEmpty())
            continue;
        s.serverName = serverName;
        s.online = o.value(QStringLiteral("online")).toBool(o.value(QStringLiteral("ready")).toBool());
        s.sourceType = o.value(QStringLiteral("source")).toObject().value(QStringLiteral("type")).toString();
        s.readers = o.value(QStringLiteral("readers")).toArray().size();

        QStringList codecs;
        for (auto t : o.value(QStringLiteral("tracks2")).toArray())
            codecs.append(t.toObject().value(QStringLiteral("codec")).toString());
        if (codecs.isEmpty()) {
            for (auto t : o.value(QStringLiteral("tracks")).toArray())
                codecs.append(t.toString());
        }
        codecs.removeAll(QString());
        s.tracks = codecs.join(QStringLiteral(", "));

        s.rtspUrl = buildRtspUrl(s.name, false);
        s.whepUrl = buildWhepUrl(s.name, false);
        s.srtUrl = buildSrtUrl(s.name);
        m_streams.append(s);
    }

    // Add configured paths that are currently not active, so they can still be prepared as layers.
    QStringList activeNames;
    for (const Stream &s : std::as_const(m_streams))
        activeNames.append(s.name);

    QJsonArray configItems = QJsonDocument::fromJson(configPathsJson.toUtf8()).array();
    for (auto v : configItems) {
        QJsonObject o = v.toObject();
        QString name = o.value(QStringLiteral("name")).toString();
        // Regex/wildcard path configurations cannot be turned into a concrete URL.
        if (name.isEmpty() || activeNames.contains(name) || name.startsWith(QLatin1Char('~')) || name == QStringLiteral("all") || name == QStringLiteral("all_others"))
            continue;
        Stream s;
        s.name = name;
        s.serverName = serverName;
        s.online = false;
        s.configuredOnly = true;
        s.sourceType = o.value(QStringLiteral("source")).toString();
        s.rtspUrl = buildRtspUrl(name, false);
        s.whepUrl = buildWhepUrl(name, false);
        s.srtUrl = buildSrtUrl(name);
        m_streams.append(s);
    }

    endResetModel();

    m_lastSummary = QStringLiteral("%1 stream(s) found").arg(m_streams.size());
    Q_EMIT streamsListChanged();
    Q_EMIT responseChanged();
}

void MediaMtxModel::applyGlobalConfig(const QString &globalConfigJson) {
    if (globalConfigJson.isEmpty() || !m_servers || m_currentServerIndex < 0)
        return;

    QJsonDocument doc = QJsonDocument::fromJson(globalConfigJson.toUtf8());
    if (!doc.isObject())
        return;
    QJsonObject o = doc.object();

    // Whether the server accepts WebRTC (WHEP) pulls at all. Independent of auto-detect,
    // because it only says whether the protocol is enabled on the server side.
    m_webrtcAvailable = configEnabled(o.value(QStringLiteral("webrtc")));

    // Same for SRT: "srt" is a plain JSON boolean in MediaMTX ("srt: yes").
    m_srtAvailable = configEnabled(o.value(QStringLiteral("srt")));

    QVariantMap server = m_servers->serverAt(m_currentServerIndex);

    if (server.value(QStringLiteral("autoDetectRtsp"), true).toBool()) {
        // Newer MediaMTX versions report the RTSP TLS level as "rtspEncryption"; released
        // versions only carry it in the deprecated global "encryption" field.
        QJsonValue rtspEncryption = o.value(QStringLiteral("rtspEncryption"));
        if (rtspEncryption.isUndefined())
            rtspEncryption = o.value(QStringLiteral("encryption"));
        if (configEncryptionStrict(rtspEncryption)) {
            m_servers->applyDetectedRtsp(m_currentServerIndex,
                                         portFromAddress(o.value(QStringLiteral("rtspsAddress")).toString(), 8322),
                                         QStringLiteral("rtsps"));
        } else {
            m_servers->applyDetectedRtsp(m_currentServerIndex,
                                         portFromAddress(o.value(QStringLiteral("rtspAddress")).toString(), 8554),
                                         QStringLiteral("rtsp"));
        }
    }

    if (server.value(QStringLiteral("autoDetectWebRtc"), true).toBool()) {
        // "webrtcEncryption" is a plain JSON boolean in MediaMTX. Encrypted WebRTC is served on the
        // same webrtcAddress - there is no separate TLS address - so fall back to it when
        // "webrtcsAddress" (not present in current releases) is missing.
        if (configEncryptionStrict(o.value(QStringLiteral("webrtcEncryption")))) {
            QString encAddress = o.value(QStringLiteral("webrtcsAddress")).toString();
            if (encAddress.isEmpty())
                encAddress = o.value(QStringLiteral("webrtcAddress")).toString();
            m_servers->applyDetectedWebRtc(m_currentServerIndex, portFromAddress(encAddress, 8889),
                                           QStringLiteral("https"));
        } else {
            m_servers->applyDetectedWebRtc(m_currentServerIndex,
                                           portFromAddress(o.value(QStringLiteral("webrtcAddress")).toString(), 8889),
                                           QStringLiteral("http"));
        }
    }

    if (server.value(QStringLiteral("autoDetectSrt"), true).toBool()) {
        // SRT has no TLS scheme variant; only the listen address can differ from the default.
        m_servers->applyDetectedSrt(m_currentServerIndex,
                                    portFromAddress(o.value(QStringLiteral("srtAddress")).toString(), 8890));
    }
}

QString MediaMtxModel::credentialsPrefix(bool includeCredentials) const {
    if (!includeCredentials || !m_servers || m_currentServerIndex < 0)
        return QString();

    QVariantMap server = m_servers->serverAt(m_currentServerIndex);
    const QString user = server.value(QStringLiteral("username")).toString();
    if (user.isEmpty())
        return QString();

    QString credentials = percentEncode(user);
    const QString pass = m_servers->effectivePassword(m_currentServerIndex);
    if (!pass.isEmpty())
        credentials += QStringLiteral(":") + percentEncode(pass);
    return credentials + QStringLiteral("@");
}

QString MediaMtxModel::buildRtspUrl(const QString &pathName, bool includeCredentials) const {
    if (!m_servers || m_currentServerIndex < 0)
        return QString();

    QVariantMap server = m_servers->serverAt(m_currentServerIndex);
    return server.value(QStringLiteral("rtspScheme")).toString()
           + QStringLiteral("://")
           + credentialsPrefix(includeCredentials)
           + server.value(QStringLiteral("host")).toString()
           + QStringLiteral(":")
           + QString::number(server.value(QStringLiteral("rtspPort")).toInt())
           + QStringLiteral("/")
           + pathName;
}

QString MediaMtxModel::buildWhepUrl(const QString &pathName, bool includeCredentials) const {
    if (!m_servers || m_currentServerIndex < 0)
        return QString();

    QVariantMap server = m_servers->serverAt(m_currentServerIndex);
    // MediaMTX serves WHEP at <webrtcAddress>/<path>/whep.
    return server.value(QStringLiteral("webrtcScheme")).toString()
           + QStringLiteral("://")
           + credentialsPrefix(includeCredentials)
           + server.value(QStringLiteral("host")).toString()
           + QStringLiteral(":")
           + QString::number(server.value(QStringLiteral("webrtcPort")).toInt())
           + QStringLiteral("/")
           + pathName
           + QStringLiteral("/whep");
}

QString MediaMtxModel::buildSrtUrl(const QString &pathName) const {
    if (!m_servers || m_currentServerIndex < 0)
        return QString();

    QVariantMap server = m_servers->serverAt(m_currentServerIndex);
    // libSRT/ffmpeg take the stream ID from the path part of an srt:// URL, and MediaMTX
    // matches SRT readers against the path name - so "srt://host:port/<path>" reads <path>.
    return QStringLiteral("srt://")
           + server.value(QStringLiteral("host")).toString()
           + QStringLiteral(":")
           + QString::number(server.value(QStringLiteral("srtPort")).toInt())
           + QStringLiteral("/")
           + pathName;
}

QString MediaMtxModel::rtspUrlAt(int index, bool includeCredentials) const {
    if (index < 0 || index >= m_streams.size())
        return QString();
    if (!includeCredentials)
        return m_streams.at(index).rtspUrl;
    return buildRtspUrl(m_streams.at(index).name, true);
}

QString MediaMtxModel::whepUrlAt(int index, bool includeCredentials) const {
    if (index < 0 || index >= m_streams.size())
        return QString();
    if (!includeCredentials)
        return m_streams.at(index).whepUrl;
    return buildWhepUrl(m_streams.at(index).name, true);
}

QString MediaMtxModel::srtUrlAt(int index) const {
    // SRT URLs never carry credentials (MediaMTX authenticates by IP allowlist or passphrase),
    // so the stored URL is always what gets used.
    if (index < 0 || index >= m_streams.size())
        return QString();
    return m_streams.at(index).srtUrl;
}

QString MediaMtxModel::nameAt(int index) const {
    if (index < 0 || index >= m_streams.size())
        return QString();
    return m_streams.at(index).name;
}

bool MediaMtxModel::addToPredefinedStreams(const QString &title, const QString &url) {
    if (title.isEmpty() || url.isEmpty())
        return false;

    QJsonObject root;
    QFile streamsFile(predefinedStreamsFilePath);
    if (streamsFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc(QJsonDocument::fromJson(streamsFile.readAll()));
        streamsFile.close();
        if (doc.isObject())
            root = doc.object();
    }

    QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
    bool replaced = false;
    for (int i = 0; i < streams.size(); i++) {
        QJsonObject o = streams.at(i).toObject();
        if (o.value(QStringLiteral("title")).toString() == title) {
            o.insert(QStringLiteral("path"), url);
            o.insert(QStringLiteral("enabled"), true);
            streams.replace(i, o);
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        QJsonObject o;
        o.insert(QStringLiteral("path"), url);
        o.insert(QStringLiteral("title"), title);
        o.insert(QStringLiteral("enabled"), true);
        streams.append(o);
    }
    root.insert(QStringLiteral("streams"), streams);

    if (!streamsFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("Couldn't write predefined-streams file.");
        return false;
    }
    streamsFile.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    streamsFile.close();

    Q_EMIT predefinedStreamsChanged();
    return true;
}
