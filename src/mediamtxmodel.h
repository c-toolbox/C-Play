/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MEDIAMTXMODEL_H
#define MEDIAMTXMODEL_H

#include <QAbstractListModel>
#include <QThread>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Holds the list of known MediaMTX servers, persisted in data/mediamtx-servers.json.
// Manually entered passwords are deliberately kept in memory only and never written to
// disk; alternatively a password can be picked up from the Windows Credential Manager.
class MediaMtxServersModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit MediaMtxServersModel(QObject *parent = nullptr);
    ~MediaMtxServersModel();

    enum {
        nameRole = Qt::UserRole,
        hostRole,
        apiPortRole,
        apiSchemeRole,
        usernameRole,
        rtspPortRole,
        rtspSchemeRole,
        rtspTransportRole,
        autoDetectRtspRole,
        enabledRole,
        hasPasswordRole,
        webrtcPortRole,
        webrtcSchemeRole,
        autoDetectWebRtcRole,
        srtPortRole,
        autoDetectSrtRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void updateServersList();

    Q_PROPERTY(int numberOfServers READ getNumberOfServers NOTIFY serversListChanged)
    int getNumberOfServers() const;

    Q_INVOKABLE void addServer(const QString &name, const QString &host, int apiPort,
                               const QString &apiScheme, const QString &username,
                               int rtspPort, const QString &rtspScheme,
                               const QString &rtspTransport, int webrtcPort,
                               const QString &webrtcScheme, int srtPort,
                               bool autoDetectRtsp, bool autoDetectWebRtc,
                               bool autoDetectSrt, bool enabled);
    Q_INVOKABLE void updateServer(int index, const QString &name, const QString &host, int apiPort,
                                  const QString &apiScheme, const QString &username,
                                  int rtspPort, const QString &rtspScheme,
                                  const QString &rtspTransport, int webrtcPort,
                                  const QString &webrtcScheme, int srtPort,
                                  bool autoDetectRtsp, bool autoDetectWebRtc,
                                  bool autoDetectSrt, bool enabled);
    Q_INVOKABLE void removeServer(int index);
    Q_INVOKABLE void moveServer(int from, int to);
    Q_INVOKABLE QVariantMap serverAt(int index) const;

    // Session-only credential, cleared when C-Play exits.
    Q_INVOKABLE void setPassword(int index, const QString &password);
    Q_INVOKABLE bool hasPassword(int index) const;
    QString password(int index) const;

    // Checks the Windows Credential Manager for a generic credential stored under this
    // server's name and API user ("MediaMTX/<name>/<username>").
    Q_INVOKABLE bool hasStoredCredential(const QString &serverName, const QString &username) const;
    // The password actually used to connect: the session (manually entered) password when
    // set, otherwise the one found in the Windows Credential Manager.
    QString effectivePassword(int index) const;

    // Effective HTTP Basic auth credentials for the server at index (username plus the resolved
    // session/stored password). Used to attach an Authorization header when adding a WebRTC layer.
    // An empty username means the server has no authentication configured.
    Q_INVOKABLE QVariantMap authCredentials(int index) const;

    QString apiBaseUrl(int index) const;
    QString username(int index) const;
    QString name(int index) const;
    // Applies RTSP port/scheme detected from the server config, without touching the stored file.
    void applyDetectedRtsp(int index, int port, const QString &scheme);
    // Applies WebRTC (WHEP) port/scheme detected from the server config, without touching the stored file.
    void applyDetectedWebRtc(int index, int port, const QString &scheme);
    // Applies the SRT port detected from the server config, without touching the stored file.
    // SRT has no TLS scheme variant; only the port can differ from the default.
    void applyDetectedSrt(int index, int port);

Q_SIGNALS:
    void serversListChanged();

private:
    struct Server {
        QString name;
        QString host;
        int apiPort = 9997;
        QString apiScheme = QStringLiteral("http");
        QString username;
        QString password; // session only
        int rtspPort = 8554;
        QString rtspScheme = QStringLiteral("rtsp");
        QString rtspTransport = QStringLiteral("tcp");
        int webrtcPort = 8889;
        QString webrtcScheme = QStringLiteral("http");
        int srtPort = 8890;
        bool autoDetectRtsp = true;
        bool autoDetectWebRtc = true;
        bool autoDetectSrt = true;
        bool enabled = true;
    };

    void saveServersToFile();
    bool isValidIndex(int index) const;

    QList<Server> m_servers;
};

class MediaMtxWorker : public QObject {
    Q_OBJECT

public:
    explicit MediaMtxWorker(QObject *parent = nullptr);

public Q_SLOTS:
    void doFetch(const QString &baseUrl, const QString &username, const QString &password,
                 bool includeConfiguredPaths, bool fetchGlobalConfig);

Q_SIGNALS:
    void fetchFinished(int statusCode, const QString &pathsJson, const QString &configPathsJson,
                       const QString &globalConfigJson, const QString &configWarning, const QString &error);
};

// Lists the streams (paths) available on a MediaMTX server and turns them into RTSP URLs
// that can be used by a Stream layer, WHEP URLs for a WebRTC layer when the server has
// WebRTC enabled, or SRT URLs for a Stream layer when the server has SRT enabled.
class MediaMtxModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit MediaMtxModel(QObject *parent = nullptr);
    ~MediaMtxModel();

    enum {
        nameRole = Qt::UserRole,
        serverNameRole,
        rtspUrlRole,
        whepUrlRole,
        onlineRole,
        sourceTypeRole,
        tracksRole,
        readersRole,
        configuredOnlyRole,
        srtUrlRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setServersModel(MediaMtxServersModel *servers);

    Q_INVOKABLE void refresh(int serverIndex);
    Q_INVOKABLE void testConnection(int serverIndex);
    Q_INVOKABLE void clear();

    Q_INVOKABLE QString rtspUrlAt(int index, bool includeCredentials = false) const;
    // WHEP endpoint URL for a WebRTC layer (only meaningful when webRtcAvailable is true).
    Q_INVOKABLE QString whepUrlAt(int index, bool includeCredentials = false) const;
    // SRT pull URL for a Stream layer (only meaningful when srtAvailable is true).
    // SRT URLs never carry user:password credentials - MediaMTX authenticates SRT readers
    // by IP allowlist or passphrase instead.
    Q_INVOKABLE QString srtUrlAt(int index) const;
    Q_INVOKABLE QString nameAt(int index) const;
    // Appends the given URL as a plain entry in data/predefined-streams.json, replacing an
    // existing entry with the same title. The predefined list is consumed by mpv-based Stream
    // layers, so any URL they can open (RTSP, SRT, ...) may be stored here.
    Q_INVOKABLE bool addToPredefinedStreams(const QString &title, const QString &url);

    Q_PROPERTY(int numberOfStreams READ getNumberOfStreams NOTIFY streamsListChanged)
    int getNumberOfStreams() const;

    Q_PROPERTY(int currentServerIndex READ currentServerIndex NOTIFY streamsListChanged)
    int currentServerIndex() const;

    Q_PROPERTY(bool refreshInProgress READ refreshInProgress NOTIFY refreshInProgressChanged)
    bool refreshInProgress() const;

    Q_PROPERTY(QString lastError READ lastError NOTIFY responseChanged)
    QString lastError() const;

    Q_PROPERTY(int lastStatusCode READ lastStatusCode NOTIFY responseChanged)
    int lastStatusCode() const;

    Q_PROPERTY(QString lastSummary READ lastSummary NOTIFY responseChanged)
    QString lastSummary() const;

    // True when the server's global configuration has WebRTC (WHEP) enabled. Updated on every fetch/test.
    Q_PROPERTY(bool webRtcAvailable READ webRtcAvailable NOTIFY responseChanged)
    bool webRtcAvailable() const;

    // True when the server's global configuration has SRT enabled ("srt: yes"). Updated on every fetch/test.
    Q_PROPERTY(bool srtAvailable READ srtAvailable NOTIFY responseChanged)
    bool srtAvailable() const;

    // Non-empty when the server's global configuration could not be fetched, in which case
    // webRtcAvailable is unknown and auto-detection was skipped. Shown by the UI so a failed
    // config read is never mistaken for "the server has no WebRTC enabled".
    Q_PROPERTY(QString configWarning READ configWarning NOTIFY responseChanged)
    QString configWarning() const;

Q_SIGNALS:
    void streamsListChanged();
    void refreshInProgressChanged();
    void responseChanged();
    void predefinedStreamsChanged();
    void startFetch(const QString &baseUrl, const QString &username, const QString &password,
                    bool includeConfiguredPaths, bool fetchGlobalConfig);

private Q_SLOTS:
    void onFetchFinished(int statusCode, const QString &pathsJson, const QString &configPathsJson,
                         const QString &globalConfigJson, const QString &configWarning, const QString &error);

private:
    struct Stream {
        QString name;
        QString serverName;
        QString rtspUrl;
        QString whepUrl;
        QString srtUrl;
        bool online = false;
        QString sourceType;
        QString tracks;
        int readers = 0;
        bool configuredOnly = false;
    };

    void applyGlobalConfig(const QString &globalConfigJson);
    QString buildRtspUrl(const QString &pathName, bool includeCredentials) const;
    QString buildWhepUrl(const QString &pathName, bool includeCredentials) const;
    // "srt://host:port/<path>" - libSRT/ffmpeg take the stream ID from the URL path and
    // MediaMTX matches SRT readers against the path name. No credentials are embedded.
    QString buildSrtUrl(const QString &pathName) const;
    // "user[:password]@" prefix when credentials should be embedded in the URL.
    QString credentialsPrefix(bool includeCredentials) const;

    MediaMtxServersModel *m_servers = nullptr;
    QList<Stream> m_streams;

    int m_currentServerIndex = -1;
    int m_pendingServerIndex = -1;
    bool m_testOnly = false;
    bool m_refreshInProgress = false;
    QString m_lastError;
    QString m_lastSummary;
    int m_lastStatusCode = 0;
    bool m_webrtcAvailable = false;
    bool m_srtAvailable = false;
    QString m_configWarning;

    QThread m_workerThread;
    MediaMtxWorker *m_worker = nullptr;
};

#endif // MEDIAMTXMODEL_H
