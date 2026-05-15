/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "wwsclientmodel.h"

#include <QAbstractSocket>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRandomGenerator>
#include <QCryptographicHash>
#include <QTimer>
#include <QUrl>
#include <QWebSocket>
#include <QWebSocketHandshakeOptions>

namespace {

constexpr int ObsOpHello = 0;
constexpr int ObsOpIdentify = 1;
constexpr int ObsOpIdentified = 2;
constexpr int ObsOpRequest = 6;
constexpr int ObsOpRequestResponse = 7;

enum ObsOptionType {
    ObsProfiles = 0,
    ObsScenes = 1,
    ObsSceneCollections = 2
};

struct WebSocketCommand {
    QString genericMessage;
    bool rawObsMessage = false;
    QJsonObject rawObsObject;
    bool obsRequest = false;
    QString requestType;
    QJsonObject requestData;
    QString password;
    int eventSubscriptions = 0;
};

QByteArray sha256Base64(const QString &value) {
    return QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha256).toBase64();
}

QString obsAuthentication(const QString &salt, const QString &challenge, const QString &password) {
    const QByteArray secret = sha256Base64(password + salt);
    return QString::fromUtf8(sha256Base64(QString::fromUtf8(secret) + challenge));
}

QJsonValue stringToJsonValue(const QString &value) {
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(value.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError) {
        if (doc.isObject()) {
            return doc.object();
        }
        if (doc.isArray()) {
            return doc.array();
        }
    }
    if (value.compare(QStringLiteral("true"), Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (value.compare(QStringLiteral("false"), Qt::CaseInsensitive) == 0) {
        return false;
    }
    bool intOk = false;
    int intValue = value.toInt(&intOk);
    if (intOk) {
        return intValue;
    }
    bool doubleOk = false;
    double doubleValue = value.toDouble(&doubleOk);
    if (doubleOk) {
        return doubleValue;
    }
    return value;
}

WebSocketCommand parseCommand(const QString &parameters) {
    WebSocketCommand command;
    if (parameters.isEmpty()) {
        return command;
    }

    QJsonParseError parseError;
    QJsonDocument paramDoc = QJsonDocument::fromJson(parameters.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError && paramDoc.isObject()) {
        QJsonObject obj = paramDoc.object();
        if (obj.contains(QStringLiteral("op")) && obj.contains(QStringLiteral("d"))) {
            command.rawObsMessage = true;
            command.rawObsObject = obj;
            command.genericMessage = QString::fromUtf8(paramDoc.toJson(QJsonDocument::Compact));
            return command;
        }

        if (obj.contains(QStringLiteral("password"))) {
            command.password = obj.value(QStringLiteral("password")).toString();
        }
        if (obj.contains(QStringLiteral("eventSubscriptions"))) {
            command.eventSubscriptions = obj.value(QStringLiteral("eventSubscriptions")).toInt();
        }
        if (obj.contains(QStringLiteral("requestType"))) {
            command.obsRequest = true;
            command.requestType = obj.value(QStringLiteral("requestType")).toString();
            command.requestData = obj.value(QStringLiteral("requestData")).toObject();
            command.genericMessage = QString::fromUtf8(paramDoc.toJson(QJsonDocument::Compact));
            return command;
        }
    }

    if (parseError.error == QJsonParseError::NoError && paramDoc.isArray()) {
        QJsonObject messageObject;
        QJsonArray paramArr = paramDoc.array();
        for (auto v : paramArr) {
            QJsonObject p = v.toObject();
            QString name = p.value(QStringLiteral("name")).toString();
            QString value = p.value(QStringLiteral("value")).toString();
            if (name.isEmpty()) {
                continue;
            }
            if (name == QStringLiteral("requestType")) {
                command.obsRequest = true;
                command.requestType = value;
            } else if (name == QStringLiteral("password")) {
                command.password = value;
            } else if (name == QStringLiteral("eventSubscriptions")) {
                command.eventSubscriptions = value.toInt();
            } else if (name.startsWith(QStringLiteral("requestData."))) {
                command.requestData.insert(name.mid(12), stringToJsonValue(value));
            } else {
                command.requestData.insert(name, stringToJsonValue(value));
                messageObject.insert(name, stringToJsonValue(value));
            }
        }
        command.genericMessage = QString::fromUtf8(QJsonDocument(messageObject).toJson(QJsonDocument::Compact));
        return command;
    }

    command.genericMessage = parameters;
    return command;
}

QString requestId() {
    return QString::number(QRandomGenerator::global()->generate64());
}

QJsonObject obsMessage(int op, const QJsonObject &data) {
    QJsonObject message;
    message.insert(QStringLiteral("op"), op);
    message.insert(QStringLiteral("d"), data);
    return message;
}

QString compactJson(const QJsonObject &object) {
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString normalizedWebSocketUrl(const QString &url) {
    QString normalizedUrl = url;
    if (!normalizedUrl.contains(QStringLiteral("://"))) {
        normalizedUrl.prepend(QStringLiteral("ws://"));
    }
    return normalizedUrl;
}

QString obsListRequestType(int optionType) {
    switch (optionType) {
    case ObsProfiles:
        return QStringLiteral("GetProfileList");
    case ObsScenes:
        return QStringLiteral("GetSceneList");
    case ObsSceneCollections:
        return QStringLiteral("GetSceneCollectionList");
    default:
        return QString();
    }
}

QStringList obsOptionsFromResponse(int optionType, const QJsonObject &responseData) {
    QStringList options;
    if (optionType == ObsProfiles) {
        QJsonArray profiles = responseData.value(QStringLiteral("profiles")).toArray();
        for (const QJsonValue &profileValue : profiles) {
            QString name = profileValue.toString();
            if (name.isEmpty()) {
                QJsonObject profile = profileValue.toObject();
                name = profile.value(QStringLiteral("profileName")).toString();
            }
            if (!name.isEmpty()) {
                options.append(name);
            }
        }
        return options;
    }
    if (optionType == ObsScenes) {
        QJsonArray scenes = responseData.value(QStringLiteral("scenes")).toArray();
        for (const QJsonValue &sceneValue : scenes) {
            QJsonObject scene = sceneValue.toObject();
            QString name = scene.value(QStringLiteral("sceneName")).toString();
            if (!name.isEmpty()) {
                options.append(name);
            }
        }
        return options;
    }
    if (optionType == ObsSceneCollections) {
        QJsonArray collections = responseData.value(QStringLiteral("sceneCollections")).toArray();
        for (const QJsonValue &collectionValue : collections) {
            QString name = collectionValue.toString();
            if (!name.isEmpty()) {
                options.append(name);
            }
        }
    }
    return options;
}

} // namespace

WwsRequestWorker::WwsRequestWorker(QObject *parent)
    : QObject(parent) {
}

void WwsRequestWorker::doRequest(const QString &url, const QString &parameters, bool ignoreStatus) {
    if (url.isEmpty()) {
        Q_EMIT requestFinished(0, QString(), QStringLiteral("Empty URL"));
        return;
    }

    QUrl webSocketUrl(normalizedWebSocketUrl(url));
    if (!webSocketUrl.isValid() || (webSocketUrl.scheme() != QStringLiteral("ws") && webSocketUrl.scheme() != QStringLiteral("wss"))) {
        Q_EMIT requestFinished(0, QString(), QStringLiteral("Invalid WebSocket URL. Use ws:// or wss://."));
        return;
    }

    WebSocketCommand command = parseCommand(parameters);
    const bool obsCandidate = command.rawObsMessage || command.obsRequest || !command.password.isEmpty() || webSocketUrl.port() == 4455;

    QWebSocket socket;
    QEventLoop eventLoop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    int statusCode = 0;
    QString responseBody;
    QString error;
    bool finished = false;
    bool opened = false;
    bool obsMode = false;
    bool identified = false;
    QString obsRequestId;

    auto finish = [&](int code, const QString &response, const QString &errorMessage) {
        if (finished) {
            return;
        }
        finished = true;
        statusCode = code;
        responseBody = response;
        error = errorMessage;
        socket.close();
        eventLoop.quit();
    };

    QObject::connect(&timeoutTimer, &QTimer::timeout, &eventLoop, [&]() {
        if (ignoreStatus) {
            finish(200, QString(), QString());
        } else {
            finish(0, QString(), QStringLiteral("WebSocket request timed out"));
        }
    });
    QObject::connect(&socket, &QWebSocket::connected, &eventLoop, [&]() {
        opened = true;
        obsMode = socket.subprotocol() == QStringLiteral("obswebsocket.json");
        if (!obsMode && !command.genericMessage.isEmpty()) {
            socket.sendTextMessage(command.genericMessage);
        }
        if (!obsMode && (ignoreStatus || command.genericMessage.isEmpty())) {
            finish(200, QString(), QString());
        }
    });
    QObject::connect(&socket, &QWebSocket::textMessageReceived, &eventLoop, [&](const QString &message) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            finish(200, message, QString());
            return;
        }

        QJsonObject msg = doc.object();
        if (!msg.contains(QStringLiteral("op")) || !msg.contains(QStringLiteral("d"))) {
            finish(200, message, QString());
            return;
        }

        obsMode = true;
        const int op = msg.value(QStringLiteral("op")).toInt(-1);
        const QJsonObject data = msg.value(QStringLiteral("d")).toObject();
        if (op == ObsOpHello) {
            if (command.rawObsMessage) {
                socket.sendTextMessage(compactJson(command.rawObsObject));
                return;
            }

            QJsonObject identify;
            identify.insert(QStringLiteral("rpcVersion"), data.value(QStringLiteral("rpcVersion")).toInt(1));
            identify.insert(QStringLiteral("eventSubscriptions"), command.eventSubscriptions);

            QJsonObject authentication = data.value(QStringLiteral("authentication")).toObject();
            if (!authentication.isEmpty()) {
                if (command.password.isEmpty()) {
                    finish(0, message, QStringLiteral("OBS WebSocket authentication required. Add a password parameter."));
                    return;
                }
                identify.insert(QStringLiteral("authentication"), obsAuthentication(
                    authentication.value(QStringLiteral("salt")).toString(),
                    authentication.value(QStringLiteral("challenge")).toString(),
                    command.password));
            }

            socket.sendTextMessage(compactJson(obsMessage(ObsOpIdentify, identify)));
            return;
        }

        if (op == ObsOpIdentified) {
            identified = true;
            if (!command.obsRequest) {
                finish(200, message, QString());
                return;
            }
            if (command.requestType.isEmpty()) {
                finish(0, message, QStringLiteral("OBS requestType parameter is empty."));
                return;
            }
            obsRequestId = requestId();
            QJsonObject request;
            request.insert(QStringLiteral("requestId"), obsRequestId);
            request.insert(QStringLiteral("requestType"), command.requestType);
            request.insert(QStringLiteral("requestData"), command.requestData);
            socket.sendTextMessage(compactJson(obsMessage(ObsOpRequest, request)));
            if (ignoreStatus) {
                finish(200, QString(), QString());
            }
            return;
        }

        if (op == ObsOpRequestResponse) {
            if (!obsRequestId.isEmpty() && data.value(QStringLiteral("requestId")).toString() != obsRequestId) {
                return;
            }
            QJsonObject requestStatus = data.value(QStringLiteral("requestStatus")).toObject();
            if (!requestStatus.value(QStringLiteral("result")).toBool()) {
                finish(0, message, requestStatus.value(QStringLiteral("comment")).toString(QStringLiteral("OBS request failed")));
                return;
            }
            finish(200, message, QString());
            return;
        }

        if (identified || command.rawObsMessage) {
            finish(200, message, QString());
        }
    });
    QObject::connect(&socket, &QWebSocket::binaryMessageReceived, &eventLoop, [&](const QByteArray &message) {
        finish(200, QString::fromUtf8(message), QString());
    });
    QObject::connect(&socket, &QWebSocket::errorOccurred, &eventLoop, [&](QAbstractSocket::SocketError) {
        finish(0, QString(), socket.errorString());
    });
    QObject::connect(&socket, &QWebSocket::disconnected, &eventLoop, [&]() {
        if (!finished) {
            const QString disconnectError = opened && obsCandidate && !obsMode
                ? QStringLiteral("WebSocket disconnected before OBS protocol negotiation. OBS requires the obswebsocket.json subprotocol.")
                : QStringLiteral("WebSocket disconnected before receiving a response");
            finish(ignoreStatus ? 200 : 0, QString(), ignoreStatus ? QString() : disconnectError);
        }
    });

    timeoutTimer.start(ignoreStatus ? 1000 : 5000);
    if (obsCandidate) {
        QWebSocketHandshakeOptions handshakeOptions;
        handshakeOptions.setSubprotocols({QStringLiteral("obswebsocket.json")});
        socket.open(webSocketUrl, handshakeOptions);
    } else {
        socket.open(webSocketUrl);
    }
    eventLoop.exec();

    Q_EMIT requestFinished(statusCode, responseBody, error);
}

void WwsRequestWorker::fetchObsOptions(const QString &url, int optionType) {
    if (url.isEmpty()) {
        Q_EMIT obsOptionsFinished(optionType, QStringList(), QStringLiteral("Empty URL"));
        return;
    }

    const QString requestType = obsListRequestType(optionType);
    if (requestType.isEmpty()) {
        Q_EMIT obsOptionsFinished(optionType, QStringList(), QStringLiteral("Invalid OBS option type"));
        return;
    }

    QUrl webSocketUrl(normalizedWebSocketUrl(url));
    if (!webSocketUrl.isValid() || (webSocketUrl.scheme() != QStringLiteral("ws") && webSocketUrl.scheme() != QStringLiteral("wss"))) {
        Q_EMIT obsOptionsFinished(optionType, QStringList(), QStringLiteral("Invalid WebSocket URL. Use ws:// or wss://."));
        return;
    }

    QWebSocket socket;
    QEventLoop eventLoop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QStringList options;
    QString error;
    bool finished = false;
    QString obsRequestId;

    auto finish = [&](const QStringList &resultOptions, const QString &errorMessage) {
        if (finished) {
            return;
        }
        finished = true;
        options = resultOptions;
        error = errorMessage;
        socket.close();
        eventLoop.quit();
    };

    QObject::connect(&timeoutTimer, &QTimer::timeout, &eventLoop, [&]() {
        finish(QStringList(), QStringLiteral("OBS option request timed out"));
    });
    QObject::connect(&socket, &QWebSocket::textMessageReceived, &eventLoop, [&](const QString &message) {
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return;
        }

        QJsonObject msg = doc.object();
        const int op = msg.value(QStringLiteral("op")).toInt(-1);
        const QJsonObject data = msg.value(QStringLiteral("d")).toObject();
        if (op == ObsOpHello) {
            if (!data.value(QStringLiteral("authentication")).toObject().isEmpty()) {
                finish(QStringList(), QStringLiteral("OBS WebSocket authentication is enabled. Disable authentication to fetch options."));
                return;
            }
            QJsonObject identify;
            identify.insert(QStringLiteral("rpcVersion"), data.value(QStringLiteral("rpcVersion")).toInt(1));
            identify.insert(QStringLiteral("eventSubscriptions"), 0);
            socket.sendTextMessage(compactJson(obsMessage(ObsOpIdentify, identify)));
            return;
        }

        if (op == ObsOpIdentified) {
            obsRequestId = requestId();
            QJsonObject request;
            request.insert(QStringLiteral("requestId"), obsRequestId);
            request.insert(QStringLiteral("requestType"), requestType);
            request.insert(QStringLiteral("requestData"), QJsonObject());
            socket.sendTextMessage(compactJson(obsMessage(ObsOpRequest, request)));
            return;
        }

        if (op == ObsOpRequestResponse) {
            if (!obsRequestId.isEmpty() && data.value(QStringLiteral("requestId")).toString() != obsRequestId) {
                return;
            }
            QJsonObject requestStatus = data.value(QStringLiteral("requestStatus")).toObject();
            if (!requestStatus.value(QStringLiteral("result")).toBool()) {
                finish(QStringList(), requestStatus.value(QStringLiteral("comment")).toString(QStringLiteral("OBS option request failed")));
                return;
            }
            finish(obsOptionsFromResponse(optionType, data.value(QStringLiteral("responseData")).toObject()), QString());
        }
    });
    QObject::connect(&socket, &QWebSocket::errorOccurred, &eventLoop, [&](QAbstractSocket::SocketError) {
        finish(QStringList(), socket.errorString());
    });
    QObject::connect(&socket, &QWebSocket::disconnected, &eventLoop, [&]() {
        if (!finished) {
            finish(QStringList(), QStringLiteral("WebSocket disconnected before receiving OBS options"));
        }
    });

    timeoutTimer.start(5000);
    QWebSocketHandshakeOptions handshakeOptions;
    handshakeOptions.setSubprotocols({QStringLiteral("obswebsocket.json")});
    socket.open(webSocketUrl, handshakeOptions);
    eventLoop.exec();

    Q_EMIT obsOptionsFinished(optionType, options, error);
}

WwsClientModel::WwsClientModel(QObject *parent)
    : QObject(parent) {
    m_worker = new WwsRequestWorker();
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &WwsClientModel::startRequest, m_worker, &WwsRequestWorker::doRequest);
    connect(this, &WwsClientModel::startObsOptionsRequest, m_worker, &WwsRequestWorker::fetchObsOptions);
    connect(m_worker, &WwsRequestWorker::requestFinished, this, &WwsClientModel::onRequestFinished);
    connect(m_worker, &WwsRequestWorker::obsOptionsFinished, this, &WwsClientModel::onObsOptionsFinished);

    m_workerThread.start();
}

WwsClientModel::~WwsClientModel() {
    m_workerThread.quit();
    m_workerThread.wait();
}

void WwsClientModel::sendRequest(const QString &url, const QString &parameters, bool ignoreStatus) {
    m_requestInProgress = true;
    Q_EMIT requestInProgressChanged();
    Q_EMIT startRequest(url, parameters, ignoreStatus);
}

void WwsClientModel::updateObsOptions(const QString &url, int optionType) {
    m_obsOptionsInProgress = true;
    m_obsOptionsType = optionType;
    m_obsOptionsError.clear();
    Q_EMIT obsOptionsInProgressChanged();
    Q_EMIT startObsOptionsRequest(url, optionType);
}

void WwsClientModel::onRequestFinished(int statusCode, const QString &responseBody, const QString &error) {
    m_lastStatusCode = statusCode;
    m_lastResponseBody = responseBody;
    m_lastError = error;
    m_requestInProgress = false;
    Q_EMIT requestInProgressChanged();
    Q_EMIT responseChanged();
}

void WwsClientModel::onObsOptionsFinished(int optionType, const QStringList &options, const QString &error) {
    if (optionType != m_obsOptionsType) {
        return;
    }
    m_obsOptions = options;
    m_obsOptionsError = error;
    m_obsOptionsInProgress = false;
    Q_EMIT obsOptionsInProgressChanged();
    Q_EMIT obsOptionsChanged();
}

bool WwsClientModel::requestInProgress() const {
    return m_requestInProgress;
}

QString WwsClientModel::lastResponseBody() const {
    return m_lastResponseBody;
}

int WwsClientModel::lastStatusCode() const {
    return m_lastStatusCode;
}

QString WwsClientModel::lastError() const {
    return m_lastError;
}

QStringList WwsClientModel::obsOptions() const {
    return m_obsOptions;
}

bool WwsClientModel::obsOptionsInProgress() const {
    return m_obsOptionsInProgress;
}

QString WwsClientModel::obsOptionsError() const {
    return m_obsOptionsError;
}
