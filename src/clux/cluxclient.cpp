/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "cluxclient.h"
#include "cluxpreviewrenderer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSet>
#include <QUrl>

namespace {

// The single configured C-Lux server, persisted in data/clux-server.json (one entry only).
const QString serverConfigPath = QStringLiteral("./data/clux-server.json");
// Used when no value has been stored yet; matches the former KCFG default.
const QString defaultServerUrl = QStringLiteral("http://localhost:8787");
// The cached preview state (light count, scenes, patterns, applied scenes), persisted in
// data/clux-state.json so preview mode can be used without a connection.
const QString stateCachePath = QStringLiteral("./data/clux-state.json");

// Convert a JSON value to its QVariant equivalent, keeping nested objects and arrays so
// the full serialized pattern parameters can be stored as-is.
QVariant jsonValueToVariant(const QJsonValue& v) {
    switch (v.type()) {
    case QJsonValue::Object: {
        const QJsonObject o = v.toObject();
        QVariantMap m;
        for (auto it = o.constBegin(); it != o.constEnd(); ++it)
            m.insert(it.key(), jsonValueToVariant(it.value()));
        return m;
    }
    case QJsonValue::Array: {
        const QJsonArray a = v.toArray();
        QVariantList l;
        for (const QJsonValue& e : a)
            l.append(jsonValueToVariant(e));
        return l;
    }
    case QJsonValue::Double:
        return v.toDouble();
    case QJsonValue::Bool:
        return v.toBool();
    case QJsonValue::String:
        return v.toString();
    default: // Null
        return QVariant();
    }
}

// The inverse of jsonValueToVariant(): convert a QVariant back to JSON for the cached
// state file. Only the types that occur in practice (maps, lists, numbers, bools,
// strings) are handled; anything else is stored as null.
QJsonValue variantToJson(const QVariant& v) {
    const uint t = v.userType();
    // Containers first, then scalars; the numeric checks must come before any broader
    // conversion because an int converts to double as well.
    if (t == QMetaType::QVariantMap) {
        QJsonObject o;
        const QVariantMap m = v.toMap();
        for (auto it = m.constBegin(); it != m.constEnd(); ++it)
            o.insert(it.key(), variantToJson(it.value()));
        return o;
    }
    if (t == QMetaType::QVariantList) {
        QJsonArray a;
        const QVariantList l = v.toList();
        for (const QVariant& e : l)
            a.append(variantToJson(e));
        return a;
    }
    if (t == QMetaType::QString)
        return v.toString();
    if (t == QMetaType::Bool)
        return v.toBool();
    if (t == QMetaType::Int || t == QMetaType::UInt || t == QMetaType::LongLong ||
        t == QMetaType::ULongLong)
        return v.toLongLong();
    if (t == QMetaType::Double || t == QMetaType::Float)
        return v.toDouble();
    return QJsonValue(); // Null
}

QJsonArray variantListToJson(const QVariantList& list) {
    QJsonArray a;
    for (const QVariant& e : list)
        a.append(variantToJson(e));
    return a;
}

// Parse a /scenes response into the list display format: name, how many patterns each
// scene holds, and the full serialized pattern entries. The pattern details let preview
// mode simulate applying scenes locally without touching the server; they mirror what
// refreshState() stores in m_patterns for live patterns.
QVariantList parseSceneList(const QByteArray& data) {
    QVariantList list;
    const QJsonArray arr = QJsonDocument::fromJson(data).array();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        QVariantMap m;
        m.insert(QStringLiteral("name"), o.value(QStringLiteral("name")).toString());

        // The full serialized entries, so preview mode can render a scene's patterns
        // locally with exactly the parameters the server would use.
        QVariantList patterns;
        const QJsonArray parr = o.value(QStringLiteral("patterns")).toArray();
        for (const QJsonValue& pv : parr)
            patterns.append(jsonValueToVariant(pv).toMap());
        m.insert(QStringLiteral("patterns"), patterns);
        m.insert(QStringLiteral("patternCount"), parr.size());
        list.append(m);
    }
    return list;
}

// Parse a /scenes/applied response: the names of the scenes currently switched on.
QStringList parseAppliedScenes(const QByteArray& data) {
    QStringList names;
    const QJsonArray arr = QJsonDocument::fromJson(data).array();
    for (const QJsonValue& v : arr)
        names.append(v.toString());
    return names;
}

} // namespace

CLuxClient::CLuxClient(QObject* parent)
    : QObject(parent), m_nam(new QNetworkAccessManager(this)) {
    // In live mode the full server state is polled this often, so changes made from
    // another control surface show up in the UI within a second. The NDI status rides
    // along: its receiver can be re-aimed or lose its source at any time too.
    m_pollTimer.setInterval(1000);
    connect(&m_pollTimer, &QTimer::timeout, this, [this]() { refreshState(); refreshNdi(); });

    // The sampling geometry as C-Lux defines it by default (shared/video.ts): a centered
    // ring at full radius with zero width. Seeded so setNdiRingWidth() can send the whole
    // geometry before the first status has reported one.
    m_ndiGeometry = {
        {QStringLiteral("centerX"), 0.5},
        {QStringLiteral("centerY"), 0.5},
        {QStringLiteral("radius"), 1.0},
        {QStringLiteral("ringWidth"), 0.0},
        {QStringLiteral("rotation"), 0.0},
        {QStringLiteral("stripY"), 0.5},
        {QStringLiteral("stripHeight"), 0.025}
    };

    // Preview-mode frames are rendered locally from the state this client keeps, in place
    // of the live frame feed; see CLuxPreviewRenderer for how it mirrors the server.
    m_localPreview = new CLuxPreviewRenderer(this);
    connect(m_localPreview, &CLuxPreviewRenderer::frameReady, this, [this](const QVariantList& frame) {
        m_frame = frame;
        Q_EMIT frameChanged();
    });

    // Keep the renderer in step with every state change and start or stop it to match the
    // mode: it renders out of live mode, once a light count is known - connected or not.
    // The cached-state file follows along too (it skips the write when nothing changed).
    connect(this, &CLuxClient::stateChanged, this, [this]() {
        m_localPreview->setLightCount(m_nLights);
        m_localPreview->setPatterns(m_patterns);
        m_localPreview->setSolidColor(m_solidColor[0], m_solidColor[1], m_solidColor[2], m_solidEnabled);
        updateLocalPreview();
        saveStateCache();
    });
    connect(this, &CLuxClient::connectionStateChanged, this, [this]() { updateLocalPreview(); });
    connect(this, &CLuxClient::liveModeChanged, this, [this]() { updateLocalPreview(); });
}

CLuxClient::~CLuxClient() {
    stopStream();
}

QString CLuxClient::serverUrl() const {
    return m_serverUrl;
}

void CLuxClient::setServerUrl(const QString& url) {
    // Drop a trailing slash so path concatenation below stays simple.
    QString normalized = url.trimmed();
    while (normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);
    if (m_serverUrl != normalized) {
        m_serverUrl = normalized;
        // A session belongs to the server that issued it: a new URL makes any held token
        // meaningless, and whether auth is required will be re-checked on the next connect.
        const bool hadAuthState = m_authRequired || !m_token.isEmpty();
        m_authRequired = false;
        m_token.clear();
        if (hadAuthState)
            Q_EMIT authStateChanged();
        Q_EMIT serverUrlChanged();
        saveServerConfig(); // persist, as the former KCFG setting did automatically
    }
}

void CLuxClient::loadServerConfig() {
    QString url;
    QFile file(serverConfigPath);
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        if (doc.isObject())
            url = doc.object().value(QStringLiteral("serverUrl")).toString();
    }
    if (url.isEmpty())
        url = defaultServerUrl;

    // Normalize like setServerUrl() but assign directly so the startup load does not rewrite
    // the config file with the value it just read.
    QString normalized = url.trimmed();
    while (normalized.endsWith(QLatin1Char('/')))
        normalized.chop(1);
    if (m_serverUrl != normalized) {
        m_serverUrl = normalized;
        Q_EMIT serverUrlChanged();
    }
}

void CLuxClient::saveServerConfig() const {
    QJsonObject root;
    root.insert(QStringLiteral("serverUrl"), m_serverUrl);

    QFile file(serverConfigPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "CLuxClient: couldn't write" << serverConfigPath;
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
}

QByteArray CLuxClient::serializeState() const {
    QJsonObject root;
    root.insert(QStringLiteral("nLights"), m_nLights);
    root.insert(QStringLiteral("scenes"), variantListToJson(m_scenes));
    root.insert(QStringLiteral("patterns"), variantListToJson(m_patterns));
    QJsonArray applied;
    for (const QString& name : m_appliedScenes)
        applied.append(name);
    root.insert(QStringLiteral("appliedScenes"), applied);
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

void CLuxClient::saveStateCache() {
    const QByteArray json = serializeState();
    if (json == m_stateCache)
        return; // nothing changed since the last save or load
    m_stateCache = json;

    QFile file(stateCachePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "CLuxClient: couldn't write" << stateCachePath;
        return;
    }
    file.write(json);
}

void CLuxClient::loadStateCache() {
    QFile file(stateCachePath);
    if (!file.open(QIODevice::ReadOnly))
        return; // no cache yet (first run): preview mode starts empty
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    m_nLights = qMax(0, root.value(QStringLiteral("nLights")).toInt(0));

    // Same list shapes as refreshState() stores: full serialized entries for both scenes
    // and patterns, so preview mode can render them locally with the server's parameters.
    QVariantList scenes;
    for (const QJsonValue& v : root.value(QStringLiteral("scenes")).toArray())
        scenes.append(jsonValueToVariant(v).toMap());
    m_scenes = scenes;

    QVariantList patterns;
    for (const QJsonValue& v : root.value(QStringLiteral("patterns")).toArray())
        patterns.append(jsonValueToVariant(v).toMap());
    m_patterns = patterns;

    QStringList applied;
    for (const QJsonValue& v : root.value(QStringLiteral("appliedScenes")).toArray())
        applied.append(v.toString());
    m_appliedScenes = applied;

    // Remember the state as loaded so the stateChanged below does not rewrite the file.
    m_stateCache = serializeState();
    Q_EMIT stateChanged(); // syncs and starts the local preview renderer
}

QUrl CLuxClient::apiUrl(const QString& path) const {
    return QUrl(m_serverUrl + QStringLiteral("/api") + path);
}

// Send a request to the C-Lux API, attaching the session token when one is held. The
// caller owns the returned reply (it deletes itself on finish). Bodies are sent as JSON,
// which is what the server's body parser expects.
QNetworkReply* CLuxClient::request(QNetworkAccessManager::Operation op, const QString& path,
                                   const QByteArray& body) {
    QNetworkRequest req(apiUrl(path));
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_token);

    switch (op) {
    case QNetworkAccessManager::GetOperation:
        return m_nam->get(req);
    case QNetworkAccessManager::PostOperation:
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        return m_nam->post(req, body);
    case QNetworkAccessManager::PutOperation:
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        return m_nam->put(req, body);
    default:
        qWarning() << "CLuxClient: unsupported operation";
        return nullptr;
    }
}

void CLuxClient::connectToServer() {
    disconnectFromServer();

    if (m_serverUrl.isEmpty()) {
        Q_EMIT errorOccurred(QStringLiteral("No C-Lux server URL set."));
        return;
    }

    // Check whether an edit password is required and, if a token was kept from before,
    // whether it still works. The open endpoints work either way, so the stream starts
    // regardless of the auth outcome - but only when the server answered at all.
    QNetworkReply* reply = request(QNetworkAccessManager::GetOperation, QStringLiteral("/auth"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (handleAuthReply(reply)) {
            refreshState();
            startStream();
            // The NDI endpoints are open like the stream, so they work regardless of the
            // auth outcome; m_connected only flips once the stream's first bytes arrive.
            refreshNdi();
        }
    });
}

bool CLuxClient::handleAuthReply(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        m_authRequired = false;
        m_token.clear();
        Q_EMIT authStateChanged();
        Q_EMIT errorOccurred(QStringLiteral("Cannot reach the C-Lux server at %1: %2")
                                 .arg(m_serverUrl, reply->errorString()));
        return false;
    }

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
    m_authRequired = obj.value(QStringLiteral("required")).toBool(false);
    // Keep the token only when it is both needed and accepted by this server. When no
    // password is configured there is nothing to hold on to, and when auth is required
    // but our kept token is not accepted (it expired, or a restart dropped the in-memory
    // sessions) dropping it brings the login row back instead of leaving us looking
    // "authenticated" while every protected call answers 401.
    const bool tokenWorks = m_authRequired && obj.value(QStringLiteral("authenticated")).toBool(false);
    if (!tokenWorks)
        m_token.clear();
    Q_EMIT authStateChanged();
    return true;
}

void CLuxClient::login(const QString& password) {
    if (m_serverUrl.isEmpty())
        return;

    const QByteArray body = QJsonDocument(
        QJsonObject{{QStringLiteral("password"), password}}).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = request(QNetworkAccessManager::PostOperation,
                                   QStringLiteral("/auth/login"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QString message = QStringLiteral("Login failed");
            if (status == 401)
                message = QStringLiteral("Incorrect password");
            else if (status == 429)
                message = QStringLiteral("Too many failed attempts, try again in a few minutes");
            else if (status == 409)
                // No edit password is configured; the login row should not be visible, but
                // handle it gracefully in case of a race with a config change.
                message = QStringLiteral("No edit password is configured on the C-Lux server");
            Q_EMIT errorOccurred(message);
            return;
        }

        const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        m_token = obj.value(QStringLiteral("token")).toString().toUtf8();
        Q_EMIT authStateChanged();
    });
}

void CLuxClient::disconnectFromServer() {
    stopStream();

    // The local state is kept on purpose: preview mode stays usable without a connection,
    // editing the cached state (see loadStateCache()), and queued preview changes are
    // replayed when live mode is entered again after reconnecting. A successful connect
    // re-syncs everything from the server via refreshState().

    if (m_connected) {
        m_connected = false;
        Q_EMIT connectionStateChanged();
    }
    updatePolling();

    // Live mode requires a connection: fall back to preview locally, with no server
    // traffic left to send (see setLiveMode()).
    if (m_liveMode)
        setLiveMode(false);
}

void CLuxClient::refreshState() {
    if (m_serverUrl.isEmpty())
        return;

    // Patterns: the full serialized entries, so preview mode can render them locally with
    // exactly the parameters the server holds (the editor shows name/type/enabled/opacity).
    QNetworkReply* patterns = request(QNetworkAccessManager::GetOperation, QStringLiteral("/patterns"));
    connect(patterns, &QNetworkReply::finished, this, [this, patterns]() {
        if (patterns->error() != QNetworkReply::NoError) {
            reportStateError(patterns);
            return;
        }
        QVariantList list;
        const QJsonArray arr = QJsonDocument::fromJson(patterns->readAll()).array();
        for (const QJsonValue& v : arr)
            list.append(jsonValueToVariant(v).toMap());
        m_patterns = list;
        Q_EMIT stateChanged();
    });

    // Scenes: name plus how many patterns they hold, for the list display.
    QNetworkReply* scenes = request(QNetworkAccessManager::GetOperation, QStringLiteral("/scenes"));
    connect(scenes, &QNetworkReply::finished, this, [this, scenes]() {
        if (scenes->error() != QNetworkReply::NoError) {
            reportStateError(scenes);
            return;
        }
        m_scenes = parseSceneList(scenes->readAll());
        Q_EMIT stateChanged();
    });

    // Which scenes are switched on.
    QNetworkReply* applied = request(QNetworkAccessManager::GetOperation, QStringLiteral("/scenes/applied"));
    connect(applied, &QNetworkReply::finished, this, [this, applied]() {
        if (applied->error() != QNetworkReply::NoError) {
            reportStateError(applied);
            return;
        }
        m_appliedScenes = parseAppliedScenes(applied->readAll());
        Q_EMIT stateChanged();
    });


    // Operator state.
    QNetworkReply* blackout = request(QNetworkAccessManager::GetOperation, QStringLiteral("/blackout"));
    connect(blackout, &QNetworkReply::finished, this, [this, blackout]() {
        if (blackout->error() != QNetworkReply::NoError) {
            reportStateError(blackout);
            return;
        }
        m_blackout = QJsonDocument::fromJson(blackout->readAll())
                         .object()
                         .value(QStringLiteral("blackout"))
                         .toBool(false);
        Q_EMIT stateChanged();
    });

    QNetworkReply* halfLight = request(QNetworkAccessManager::GetOperation, QStringLiteral("/half-light"));
    connect(halfLight, &QNetworkReply::finished, this, [this, halfLight]() {
        if (halfLight->error() != QNetworkReply::NoError) {
            reportStateError(halfLight);
            return;
        }
        m_halfLight = QJsonDocument::fromJson(halfLight->readAll())
                          .object()
                          .value(QStringLiteral("halfLight"))
                          .toBool(false);
        Q_EMIT stateChanged();
    });

    QNetworkReply* solid = request(QNetworkAccessManager::GetOperation, QStringLiteral("/solid-color"));
    connect(solid, &QNetworkReply::finished, this, [this, solid]() {
        if (solid->error() != QNetworkReply::NoError) {
            reportStateError(solid);
            return;
        }
        const QJsonObject o = QJsonDocument::fromJson(solid->readAll()).object();
        m_solidEnabled = o.value(QStringLiteral("enabled")).toBool(false);
        const QJsonObject target = o.value(QStringLiteral("target")).toObject();
        m_solidColor[0] = qBound(0, target.value(QStringLiteral("r")).toInt(0), 255);
        m_solidColor[1] = qBound(0, target.value(QStringLiteral("g")).toInt(0), 255);
        m_solidColor[2] = qBound(0, target.value(QStringLiteral("b")).toInt(0), 255);
        Q_EMIT stateChanged();
    });
}

void CLuxClient::refreshScenes() {
    if (!m_connected)
        return;

    // Scene list: name plus how many patterns they hold, for the list display.
    QNetworkReply* scenes = request(QNetworkAccessManager::GetOperation, QStringLiteral("/scenes"));
    connect(scenes, &QNetworkReply::finished, this, [this, scenes]() {
        if (scenes->error() != QNetworkReply::NoError) {
            reportStateError(scenes);
            return;
        }
        m_scenes = parseSceneList(scenes->readAll());
        Q_EMIT stateChanged();
    });

    // Which scenes are switched on.
    QNetworkReply* applied = request(QNetworkAccessManager::GetOperation, QStringLiteral("/scenes/applied"));
    connect(applied, &QNetworkReply::finished, this, [this, applied]() {
        if (applied->error() != QNetworkReply::NoError) {
            reportStateError(applied);
            return;
        }
        m_appliedScenes = parseAppliedScenes(applied->readAll());
        Q_EMIT stateChanged();
    });
}

// Applies a NdiStatus object from the server to the NDI properties and emits the change.
// Shared by refreshNdi() and setNdiSource(), since both responses carry it.
void CLuxClient::applyNdiStatus(const QJsonObject& obj) {
    m_ndiSupported = obj.value(QStringLiteral("supported")).toBool(true);
    // reason/error/source are null (not absent) when they do not apply; toString() maps
    // that to an empty string, which is what the properties use for "none".
    m_ndiReason = obj.value(QStringLiteral("reason")).toString();
    m_ndiSource = obj.value(QStringLiteral("source")).toString();
    m_ndiRunning = obj.value(QStringLiteral("running")).toBool(false);
    m_ndiConnections = qMax(0, obj.value(QStringLiteral("connections")).toInt(0));
    m_ndiError = obj.value(QStringLiteral("error")).toString();

    // The sampling geometry, kept whole so an update can send it back with one field
    // changed; the API requires every part of it at once.
    const QJsonObject geo = obj.value(QStringLiteral("geometry")).toObject();
    if (!geo.isEmpty()) {
        QVariantMap g;
        for (auto it = geo.constBegin(); it != geo.constEnd(); ++it)
            g.insert(it.key(), jsonValueToVariant(it.value()));
        m_ndiGeometry = g;
    }

    // Without NDI on the server no source list can exist; drop whatever was cached so a
    // later reconnect does not show senders from another machine's server.
    if (!m_ndiSupported)
        m_ndiSources.clear();

    Q_EMIT ndiStateChanged();
}

void CLuxClient::refreshNdi() {
    if (m_serverUrl.isEmpty())
        return;

    // Status first: it says whether discovery is available at all, and the source list
    // request would only answer 503 when it is not. Like refreshState(), this runs right
    // after a connect as well, before m_connected has flipped yet. The status is applied
    // (and emitted) only once both requests have answered, so the UI sees one consistent
    // update per refresh - the QML side picks its default source from that single signal.
    QNetworkReply* status = request(QNetworkAccessManager::GetOperation, QStringLiteral("/ndi"));
    connect(status, &QNetworkReply::finished, this, [this, status]() {
        if (status->error() != QNetworkReply::NoError) {
            reportStateError(status);
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(status->readAll()).object();

        // Without NDI on the server there is no source list to fetch.
        if (!obj.value(QStringLiteral("supported")).toBool(true)) {
            applyNdiStatus(obj);
            return;
        }

        // What discovery has seen on the network: name plus, when known, the address.
        QNetworkReply* sources = request(QNetworkAccessManager::GetOperation, QStringLiteral("/ndi/sources"));
        connect(sources, &QNetworkReply::finished, this, [this, sources, obj]() {
            if (sources->error() != QNetworkReply::NoError) {
                reportStateError(sources);
                return;
            }
            QVariantList list;
            const QJsonArray arr = QJsonDocument::fromJson(sources->readAll()).array();
            for (const QJsonValue& v : arr) {
                const QVariantMap m = jsonValueToVariant(v).toMap();
                if (m.value(QStringLiteral("name")).toString().isEmpty())
                    continue;
                list.append(m);
            }
            m_ndiSources = list;
            applyNdiStatus(obj);   // emits ndiStateChanged() with both updates in place
        });
    });
}

void CLuxClient::setNdiSource(const QString& name) {
    // A command like the others: it needs a connection, and the UI keeps its controls
    // disabled without one.
    if (!m_connected)
        return;

    // The API takes null to stop the receiver; an empty string would be rejected.
    const QJsonObject body = name.isEmpty()
                                 ? QJsonObject{{QStringLiteral("source"), QJsonValue::Null}}
                                 : QJsonObject{{QStringLiteral("source"), name}};
    QNetworkReply* reply = request(QNetworkAccessManager::PutOperation, QStringLiteral("/ndi"),
                                   QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        applyNdiStatus(QJsonDocument::fromJson(reply->readAll()).object());
    });
}

void CLuxClient::setNdiRingWidth(double width) {
    // A command like the others: it needs a connection, and the UI keeps its controls
    // disabled without one.
    if (!m_connected)
        return;

    // The API validates the whole geometry at once, so send it back with only this field
    // changed; clamp to what the server accepts (a fraction of the rim radius).
    const double clamped = qBound(0.0, width, 1.0);
    QJsonObject geo;
    for (auto it = m_ndiGeometry.constBegin(); it != m_ndiGeometry.constEnd(); ++it) {
        const double value = it.key() == QStringLiteral("ringWidth") ? clamped : it.value().toDouble();
        geo.insert(it.key(), value);
    }

    const QJsonObject body{{QStringLiteral("geometry"), geo}};
    QNetworkReply* reply = request(QNetworkAccessManager::PutOperation, QStringLiteral("/ndi"),
                                   QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        applyNdiStatus(QJsonDocument::fromJson(reply->readAll()).object());
    });
}

// A failed request only matters while the stream says we are connected; during a teardown
// the in-flight replies just die quietly. A 401 can only come from the password-protected
// endpoints (pattern toggles): the server requires a login and our credentials, if any,
// are not accepted - the session expired, or it was dropped by a restart or a password
// change. Mark auth as required in case we did not know yet (the password can be set while
// connected), drop the token so the login row comes back, and say what happened instead of
// dumping the raw error string.
void CLuxClient::reportStateError(QNetworkReply* reply) {
    if (!m_connected)
        return;

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 401) {
        const bool hadToken = !m_token.isEmpty();
        m_authRequired = true;
        m_token.clear();
        Q_EMIT authStateChanged();
        Q_EMIT errorOccurred(hadToken
                                 ? QStringLiteral("The C-Lux session is no longer valid (it may have expired or the password was changed), log in again.")
                                 : QStringLiteral("Log in to the C-Lux server first: this action needs the edit password."));
        return;
    }
    Q_EMIT errorOccurred(QStringLiteral("Failed to fetch state: %1").arg(reply->errorString()));
}


void CLuxClient::startStream() {
    stopStream();
    m_streamBuffer.clear();

    // In preview mode we watch the raw (pre-mask) feed only to hold the connection and
    // learn the light count; its frames are ignored because the local renderer produces
    // the preview instead.
    const QString path = m_liveMode ? QStringLiteral("/stream") : QStringLiteral("/stream?raw=1");
    QNetworkRequest req(apiUrl(path));
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", "Bearer " + m_token);

    m_streamReply = m_nam->get(req);

    // Server-Sent Events: frames arrive as "data: [r,g,b,...]\n\n" blocks, with occasional
    // ":heartbeat" comments. The buffer holds whatever has not yet formed a full block.
    connect(m_streamReply, &QNetworkReply::readyRead, this, [this]() {
        const int status = m_streamReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200) {
            Q_EMIT errorOccurred(QStringLiteral("The C-Lux server refused the frame stream."));
            stopStream();
            return;
        }
        if (!m_connected) {
            m_connected = true;
            Q_EMIT connectionStateChanged();
        }
        updatePolling();

        m_streamBuffer.append(m_streamReply->readAll());

        while (true) {
            const int end = m_streamBuffer.indexOf("\n\n");
            if (end < 0)
                break;

            const QByteArray block = m_streamBuffer.left(end);
            m_streamBuffer.remove(0, end + 2);

            // Skip comments (the heartbeat lines).
            if (!block.startsWith("data:"))
                continue;

            const QJsonArray arr = QJsonDocument::fromJson(block.mid(5)).array();
            if (arr.size() < 3 || arr.size() % 3 != 0)
                continue;

            const int lights = arr.size() / 3;
            if (lights != m_nLights) {
                m_nLights = lights;
                Q_EMIT stateChanged();
            }

            // In preview mode the local renderer owns the frame feed, so the stream only
            // keeps the connection alive and reports the light count.
            if (!m_liveMode)
                continue;

            QVariantList frame;
            frame.reserve(arr.size());
            for (const QJsonValue& v : arr)
                frame.append(v.toInt(0));

            m_frame = frame;
            Q_EMIT frameChanged();
        }
    });

    connect(m_streamReply, &QNetworkReply::finished, this, [this]() {
        // The stream is the connection's heartbeat: when it ends (server restart, network
        // drop) we are disconnected. The UI can offer a reconnect from there.
        if (m_connected) {
            m_connected = false;
            Q_EMIT connectionStateChanged();
        }

        // Live mode requires a connection: fall back to preview locally, keeping any
        // queued changes for the next time live mode is entered (see setLiveMode()).
        if (m_liveMode)
            setLiveMode(false);

        updatePolling();
        stopStream();
    });
}

void CLuxClient::stopStream() {
    if (!m_streamReply)
        return;

    // Detach the handlers before aborting so the finished signal from the abort does not
    // re-enter this function. The connection state is left alone: callers that mean to
    // disconnect update it themselves, and startStream() calls this as a plain reset.
    disconnect(m_streamReply, nullptr, this, nullptr);
    m_streamReply->abort();
    m_streamReply->deleteLater();
    m_streamReply = nullptr;
}

// The state poll runs only while connected and in live mode: preview mode is read-only
// (its scene data comes from the manual "Update scenes" button), and a disconnected
// client has nothing to talk to. Call this whenever m_connected or m_liveMode changes.
void CLuxClient::updatePolling() {
    const bool shouldPoll = m_connected && m_liveMode;
    if (shouldPoll && !m_pollTimer.isActive())
        m_pollTimer.start();
    else if (!shouldPoll && m_pollTimer.isActive())
        m_pollTimer.stop();
}

// The local preview renders out of live mode, once a light count is known - with or
// without a connection: offline it previews edits against the cached state (see
// loadStateCache()). While connected the SSE feed keeps running either way as the
// connection's heartbeat (see startStream()); in preview mode its frames are ignored.
void CLuxClient::updateLocalPreview() {
    const bool shouldRun = !m_liveMode && m_nLights > 0;
    if (shouldRun)
        m_localPreview->start();
    else
        m_localPreview->stop();
}


void CLuxClient::setLiveMode(bool on) {
    if (m_liveMode == on)
        return;

    // Live mode drives the physical lights, so it is only available while connected.
    // Preview mode works offline against the cached state (see loadStateCache()).
    if (on && !m_connected)
        return;

    const bool wasConnected = m_connected;
    m_liveMode = on;
    Q_EMIT liveModeChanged();

    // Reaching here while disconnected means turning preview off after a disconnect: it
    // takes effect locally, with no server to reconfigure. (Turning it on is impossible
    // there: see the guard above.)
    if (!wasConnected) {
        updatePolling();
        return;
    }

    if (on) {
        // Entering live: replay the changes queued in preview mode, then restore the
        // blackout so the lights come up with exactly that look. The per-second poll is
        // started by finishEnteringLiveMode(), once the replay is done, so it cannot
        // overwrite the state mid-flush.
        if (m_authRequired && !authenticated() && pendingOpsNeedAuth()) {
            // Pattern toggles in the queue sit behind the edit password; without a login
            // they would all fail one by one. Stay in preview mode until the user has
            // logged in, instead of half-applying the queued look.
            m_liveMode = false;
            Q_EMIT liveModeChanged();
            Q_EMIT errorOccurred(QStringLiteral("Log in to the C-Lux server first: some queued changes need the edit password."));
            return;
        }
        flushPendingOps();
        return;
    }

    // Entering preview blackouts the real output so edits can be watched safely. Changes
    // made in live mode were already sent to the server and the per-second poll kept the
    // local state in sync with it, so there is nothing to carry over into preview: drop
    // whatever may still be queued (e.g. from an interrupted replay).
    clearPendingOps();
    updatePolling();

    const QByteArray body = QJsonDocument(
        QJsonObject{{QStringLiteral("blackout"), !on}}).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = request(QNetworkAccessManager::PutOperation,
                                   QStringLiteral("/blackout"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        m_blackout = !m_liveMode;
        Q_EMIT stateChanged();

        // Switch the feed to match the new mode.
        startStream();
    });
}

// Percent-encode a name for use in a URL path segment (pattern/scene names may contain
// spaces and other characters).
static QString encodeName(const QString& name) {
    return QString::fromUtf8(QUrl::toPercentEncoding(name).constData());
}

void CLuxClient::queuePendingOp(const QVariantMap& op) {
    m_pendingOps.append(op);
    Q_EMIT pendingChangesChanged();
}

void CLuxClient::clearPendingOps() {
    if (m_pendingOps.isEmpty())
        return;
    m_pendingOps.clear();
    Q_EMIT pendingChangesChanged();
}

bool CLuxClient::pendingOpsNeedAuth() const {
    for (const QVariant& v : m_pendingOps) {
        if (v.toMap().value(QStringLiteral("op")).toString() == QStringLiteral("setPatternEnabled"))
            return true;
    }
    return false;
}

QVariantMap CLuxClient::sceneByName(const QString& name) const {
    for (const QVariant& v : m_scenes) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("name")).toString() == name)
            return m;
    }
    return {};
}

// Replays the changes queued in preview mode against the server, one request at a time
// and in the order they were made, so scene apply/unapply pairs cannot interleave. When
// the queue is exhausted (or was empty) it restores the blackout and restarts the stream;
// see setLiveMode(). A disconnect or a switch back to preview stops the replay: those
// paths clear the queue themselves, and the in-flight reply's handler bails out below.
void CLuxClient::flushPendingOps() {
    if (!m_pendingOps.isEmpty())
        sendPendingOp(m_pendingOps.first().toMap());
    else
        finishEnteringLiveMode();
}

void CLuxClient::sendPendingOp(const QVariantMap& op) {
    const QString kind = op.value(QStringLiteral("op")).toString();
    QNetworkReply* reply = nullptr;

    if (kind == QLatin1String("setPatternEnabled")) {
        const QByteArray body = QJsonDocument(
            QJsonObject{{QStringLiteral("enabled"), op.value(QStringLiteral("enabled")).toBool()}})
            .toJson(QJsonDocument::Compact);
        reply = request(QNetworkAccessManager::PutOperation,
                        QStringLiteral("/patterns/") +
                            encodeName(op.value(QStringLiteral("name")).toString()) +
                            QStringLiteral("/enabled"),
                        body);
    } else if (kind == QLatin1String("clearPatterns")) {
        reply = request(QNetworkAccessManager::PostOperation, QStringLiteral("/patterns/clear"));
    } else if (kind == QLatin1String("applyScene") || kind == QLatin1String("unapplyScene")
               || kind == QLatin1String("replaceWithScene")) {
        const QString action = kind.chopped(5); // "apply", "unapply" or "replace"
        reply = request(QNetworkAccessManager::PostOperation,
                        QStringLiteral("/scenes/") + encodeName(op.value(QStringLiteral("name")).toString())
                            + QStringLiteral("/") + action);
    }

    if (!reply) {
        // Unknown op (should not happen): drop it and continue with the rest of the queue.
        m_pendingOps.removeFirst();
        Q_EMIT pendingChangesChanged();
        if (!m_pendingOps.isEmpty())
            sendPendingOp(m_pendingOps.first().toMap());
        else
            finishEnteringLiveMode();
        return;
    }

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        // The op just sent is always at the front: the replay is strictly sequential.
        if (!m_pendingOps.isEmpty()) {
            m_pendingOps.removeFirst();
            Q_EMIT pendingChangesChanged();
        }

        if (reply->error() != QNetworkReply::NoError)
            reportStateError(reply);

        // A disconnect or a switch back to preview stops the replay; those paths clear
        // the queue themselves.
        if (!m_connected || !m_liveMode)
            return;

        if (!m_pendingOps.isEmpty())
            sendPendingOp(m_pendingOps.first().toMap());
        else
            finishEnteringLiveMode();
    });
}

// The last step of entering live mode: restore the blackout so the lights fade back in
// with whatever look the replay produced, switch to the normal (masked) feed and resume
// the per-second state poll. A final refreshState() re-syncs the UI with the server in
// case any replayed op failed along the way.
void CLuxClient::finishEnteringLiveMode() {
    const QByteArray body = QJsonDocument(
        QJsonObject{{QStringLiteral("blackout"), false}}).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = request(QNetworkAccessManager::PutOperation, QStringLiteral("/blackout"), body);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        m_blackout = false;
        Q_EMIT stateChanged();

        startStream();   // normal feed: frames after the blackout and half-light masks
        updatePolling(); // resume the per-second poll now that the replay is done
        refreshState();  // re-sync with whatever the server actually ended up as
    });
}

void CLuxClient::setPatternEnabled(const QString& name, bool enabled) {
    if (name.isEmpty())
        return;

    // Preview mode never sends commands that change the server: they update the local
    // state and are queued for replay when live mode is entered. The same applies to
    // every scene/pattern command below. It works while disconnected too, against the
    // cached state (see loadStateCache()).
    if (!m_liveMode) {
        for (QVariant& v : m_patterns) {
            QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("name")).toString() != name)
                continue;
            if (m.value(QStringLiteral("enabled")).toBool() == enabled)
                return; // already in this state locally: nothing to queue
            m.insert(QStringLiteral("enabled"), enabled);
            v = m;
            Q_EMIT stateChanged();
            queuePendingOp({{QStringLiteral("op"), QStringLiteral("setPatternEnabled")},
                            {QStringLiteral("name"), name},
                            {QStringLiteral("enabled"), enabled}});
            return;
        }
        // Not in the local list (the server would answer 404 as well): nothing to do.
        return;
    }

    // Live mode sends straight away and therefore requires a connection.
    if (!m_connected)
        return;

    // The toggle endpoint sits behind the edit password; refuse early with a clear message
    // instead of letting the server answer 401 (reportStateError() covers that case too,
    // e.g. when the session expires between this check and the reply).
    if (m_authRequired && !authenticated()) {
        Q_EMIT errorOccurred(QStringLiteral("Log in to the C-Lux server first: this action needs the edit password."));
        return;
    }

    const QByteArray body = QJsonDocument(
        QJsonObject{{QStringLiteral("enabled"), enabled}}).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = request(QNetworkAccessManager::PutOperation,
                                   QStringLiteral("/patterns/") + encodeName(name) + QStringLiteral("/enabled"),
                                   body);
    connect(reply, &QNetworkReply::finished, this, [this, name, enabled, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        for (QVariant& v : m_patterns) {
            QVariantMap m = v.toMap();
            if (m.value(QStringLiteral("name")).toString() == name) {
                m.insert(QStringLiteral("enabled"), enabled);
                v = m;
                break;
            }
        }
        Q_EMIT stateChanged();
    });
}

void CLuxClient::clearPatterns() {
    if (!m_liveMode) {
        // Mirror the server: clearing also drops the applied scenes. Skip the queue when
        // there is nothing to clear, as the server-side call would be a no-op too. Works
        // while disconnected as well (see setPatternEnabled()).
        if (m_patterns.isEmpty() && m_appliedScenes.isEmpty())
            return;
        m_patterns.clear();
        m_appliedScenes.clear();
        Q_EMIT stateChanged();
        queuePendingOp({{QStringLiteral("op"), QStringLiteral("clearPatterns")}});
        return;
    }

    // Live mode sends straight away and therefore requires a connection.
    if (!m_connected)
        return;

    QNetworkReply* reply = request(QNetworkAccessManager::PostOperation,
                                   QStringLiteral("/patterns/clear"));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        m_patterns.clear();
        refreshState();
    });
}

void CLuxClient::applyScene(const QString& name) {
    if (name.isEmpty())
        return;

    if (!m_liveMode) {
        const QVariantMap scene = sceneByName(name);
        if (scene.isEmpty())
            return; // unknown scene: the server would answer 404 as well

        // Mirror the server: add the scene's patterns that are not already present.
        QSet<QString> existing;
        for (const QVariant& v : m_patterns)
            existing.insert(v.toMap().value(QStringLiteral("name")).toString());
        bool added = false;
        const QVariantList pats = scene.value(QStringLiteral("patterns")).toList();
        for (const QVariant& pv : pats) {
            const QString pn = pv.toMap().value(QStringLiteral("name")).toString();
            if (!existing.contains(pn)) {
                m_patterns.append(pv);
                existing.insert(pn);
                added = true;
            }
        }

        // Skip the queue when applying would change nothing, as on the server.
        if (added || !m_appliedScenes.contains(name)) {
            if (!m_appliedScenes.contains(name))
                m_appliedScenes.append(name);
            Q_EMIT stateChanged();
            queuePendingOp({{QStringLiteral("op"), QStringLiteral("applyScene")},
                            {QStringLiteral("name"), name}});
        }
        return;
    }

    // Live mode sends straight away and therefore requires a connection.
    if (!m_connected)
        return;

    QNetworkReply* reply = request(QNetworkAccessManager::PostOperation,
                                   QStringLiteral("/scenes/") + encodeName(name) + QStringLiteral("/apply"));
    connect(reply, &QNetworkReply::finished, this, [this, name, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        if (!m_appliedScenes.contains(name))
            m_appliedScenes.append(name);
        Q_EMIT stateChanged();
        refreshState(); // The pattern list changed with the scene.
    });
}

void CLuxClient::unapplyScene(const QString& name) {
    if (name.isEmpty())
        return;

    if (!m_liveMode) {
        const QVariantMap scene = sceneByName(name);
        if (scene.isEmpty())
            return; // unknown scene: the server would answer 404 as well

        const bool wasApplied = m_appliedScenes.contains(name);
        m_appliedScenes.removeAll(name);

        // Mirror the server: drop this scene's patterns unless another applied scene
        // still holds them.
        QSet<QString> keep;
        for (const QString& other : m_appliedScenes) {
            const QVariantList opats =
                sceneByName(other).value(QStringLiteral("patterns")).toList();
            for (const QVariant& pv : opats)
                keep.insert(pv.toMap().value(QStringLiteral("name")).toString());
        }

        QSet<QString> drop;
        const QVariantList pats = scene.value(QStringLiteral("patterns")).toList();
        for (const QVariant& pv : pats) {
            const QString pn = pv.toMap().value(QStringLiteral("name")).toString();
            if (!keep.contains(pn))
                drop.insert(pn);
        }

        bool dropped = false;
        for (int i = m_patterns.size() - 1; i >= 0; --i) {
            if (drop.contains(m_patterns[i].toMap().value(QStringLiteral("name")).toString())) {
                m_patterns.removeAt(i);
                dropped = true;
            }
        }

        // Skip the queue when unapplying would change nothing, as on the server.
        if (wasApplied || dropped) {
            Q_EMIT stateChanged();
            queuePendingOp({{QStringLiteral("op"), QStringLiteral("unapplyScene")},
                            {QStringLiteral("name"), name}});
        }
        return;
    }

    // Live mode sends straight away and therefore requires a connection.
    if (!m_connected)
        return;

    QNetworkReply* reply = request(QNetworkAccessManager::PostOperation,
                                   QStringLiteral("/scenes/") + encodeName(name) + QStringLiteral("/unapply"));
    connect(reply, &QNetworkReply::finished, this, [this, name, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        m_appliedScenes.removeAll(name);
        Q_EMIT stateChanged();
        refreshState();
    });
}

void CLuxClient::replaceWithScene(const QString& name) {
    if (name.isEmpty())
        return;

    if (!m_liveMode) {
        const QVariantMap scene = sceneByName(name);
        if (scene.isEmpty())
            return; // unknown scene: the server would answer 404 as well

        // Mirror the server: swap in exactly this scene's patterns.
        m_patterns.clear();
        for (const QVariant& pv : scene.value(QStringLiteral("patterns")).toList())
            m_patterns.append(pv);
        m_appliedScenes = QStringList{name};
        Q_EMIT stateChanged();
        queuePendingOp({{QStringLiteral("op"), QStringLiteral("replaceWithScene")},
                        {QStringLiteral("name"), name}});
        return;
    }

    // Live mode sends straight away and therefore requires a connection.
    if (!m_connected)
        return;

    QNetworkReply* reply = request(QNetworkAccessManager::PostOperation,
                                   QStringLiteral("/scenes/") + encodeName(name) + QStringLiteral("/replace"));
    connect(reply, &QNetworkReply::finished, this, [this, name, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        m_appliedScenes = QStringList{name};
        Q_EMIT stateChanged();
        refreshState();
    });
}


void CLuxClient::setBlackout(bool on) {
    if (!m_connected || !m_liveMode)
        return;

    const QByteArray body = QJsonDocument(
        QJsonObject{{QStringLiteral("blackout"), on}}).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = request(QNetworkAccessManager::PutOperation,
                                   QStringLiteral("/blackout"), body);
    connect(reply, &QNetworkReply::finished, this, [this, on, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        m_blackout = on;
        Q_EMIT stateChanged();
    });
}

void CLuxClient::setHalfLight(bool on) {
    if (!m_connected || !m_liveMode)
        return;

    const QByteArray body = QJsonDocument(
        QJsonObject{{QStringLiteral("halfLight"), on}}).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = request(QNetworkAccessManager::PutOperation,
                                   QStringLiteral("/half-light"), body);
    connect(reply, &QNetworkReply::finished, this, [this, on, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        m_halfLight = on;
        Q_EMIT stateChanged();
    });
}

void CLuxClient::setSolidColor(int r, int g, int b, bool enabled) {
    if (!m_connected || !m_liveMode)
        return;

    const QJsonObject body{
        {QStringLiteral("color"), QJsonObject{{QStringLiteral("r"), qBound(0, r, 255)},
                                              {QStringLiteral("g"), qBound(0, g, 255)},
                                              {QStringLiteral("b"), qBound(0, b, 255)}}},
        {QStringLiteral("enabled"), enabled}
    };
    QNetworkReply* reply = request(QNetworkAccessManager::PutOperation,
                                   QStringLiteral("/solid-color"),
                                   QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, r, g, b, enabled, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            reportStateError(reply);
            return;
        }
        m_solidColor[0] = qBound(0, r, 255);
        m_solidColor[1] = qBound(0, g, 255);
        m_solidColor[2] = qBound(0, b, 255);
        m_solidEnabled = enabled;
        Q_EMIT stateChanged();
    });
}

