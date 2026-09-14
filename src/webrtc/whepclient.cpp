/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "webrtc/whepclient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QStringList>

#include <sgct/log.h>

using namespace Qt::Literals::StringLiterals;

namespace {

constexpr auto kSdpContentType = "application/sdp";

/// Splits on commas that are outside <> brackets and outside quoted strings.
QStringList splitLinkEntries(const QString &value)
{
    QStringList entries;
    QString current;
    bool inQuotes = false;
    bool inBrackets = false;

    for (const QChar ch : value) {
        if (ch == u'"') {
            inQuotes = !inQuotes;
        } else if (!inQuotes && ch == u'<') {
            inBrackets = true;
        } else if (!inQuotes && ch == u'>') {
            inBrackets = false;
        }

        if (ch == u',' && !inQuotes && !inBrackets) {
            entries.append(current);
            current.clear();
            continue;
        }
        current.append(ch);
    }

    if (!current.trimmed().isEmpty()) {
        entries.append(current);
    }
    return entries;
}

QStringList splitLinkParameters(const QString &value)
{
    QStringList parameters;
    QString current;
    bool inQuotes = false;

    for (const QChar ch : value) {
        if (ch == u'"') {
            inQuotes = !inQuotes;
        }
        if (ch == u';' && !inQuotes) {
            parameters.append(current);
            current.clear();
            continue;
        }
        current.append(ch);
    }

    if (!current.trimmed().isEmpty()) {
        parameters.append(current);
    }
    return parameters;
}

QString unquote(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2 && value.startsWith(u'"') && value.endsWith(u'"')) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

/// Parses RFC 5988 Link header values and returns the entries with
/// rel="ice-server". MediaMTX emits these from its webrtcICEServers2 config.
QList<IceServerSpec> parseIceServerLinkHeader(const QString &headerValue)
{
    QList<IceServerSpec> servers;

    for (const QString &entry : splitLinkEntries(headerValue)) {
        const int open = entry.indexOf(u'<');
        const int close = entry.indexOf(u'>', open + 1);
        if (open < 0 || close < 0) {
            continue;
        }

        IceServerSpec server;
        server.url = entry.mid(open + 1, close - open - 1).trimmed();
        if (server.url.isEmpty()) {
            continue;
        }

        bool isIceServer = false;
        for (const QString &parameter : splitLinkParameters(entry.mid(close + 1))) {
            const int equals = parameter.indexOf(u'=');
            if (equals < 0) {
                continue;
            }
            const QString key = parameter.left(equals).trimmed().toLower();
            const QString value = unquote(parameter.mid(equals + 1));

            if (key == u"rel"_s && value.compare(u"ice-server"_s, Qt::CaseInsensitive) == 0) {
                isIceServer = true;
            } else if (key == u"username"_s) {
                server.username = value;
            } else if (key == u"credential"_s) {
                server.credential = value;
            }
        }

        if (isIceServer) {
            servers.append(server);
        }
    }

    return servers;
}

} // namespace

WhepClient::WhepClient(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    m_network->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

WhepClient::~WhepClient()
{
    if (m_pending) {
        m_pending->abort();
    }
}

void WhepClient::setEndpoint(const QUrl &url)
{
    m_endpoint = url;

    // Credentials embedded in the URL are common for MediaMTX; lift them out so
    // they travel in the Authorization header instead of the request line.
    if (!m_endpoint.userName().isEmpty()) {
        m_username = m_endpoint.userName();
        m_password = m_endpoint.password();
        m_endpoint.setUserName(QString());
        m_endpoint.setPassword(QString());
    }
}

void WhepClient::setCredentials(const QString &username, const QString &password)
{
    if (!username.isEmpty()) {
        m_username = username;
        m_password = password;
    }
}

bool WhepClient::hasSession() const
{
    return !m_resourceUrl.isEmpty();
}

void WhepClient::applyAuth(QNetworkRequest &request) const
{
    if (m_username.isEmpty()) {
        return;
    }
    const QByteArray token = (m_username + u":"_s + m_password).toUtf8().toBase64();
    request.setRawHeader("Authorization", "Basic " + token);
}

QList<IceServerSpec> WhepClient::iceServersFrom(QNetworkReply *reply) const
{
    QList<IceServerSpec> servers;
    const auto headers = reply->rawHeaderPairs();
    for (const auto &[name, value] : headers) {
        if (name.compare("link", Qt::CaseInsensitive) == 0) {
            servers.append(parseIceServerLinkHeader(QString::fromUtf8(value)));
        }
    }
    return servers;
}

void WhepClient::requestIceServers()
{
    QNetworkRequest request(m_endpoint);
    applyAuth(request);

    QNetworkReply *reply = m_network->sendCustomRequest(request, "OPTIONS");
    QPointer<WhepClient> self(this);
    connect(reply, &QNetworkReply::finished, this, [this, reply, self] {
        reply->deleteLater();
        if (!self) {
            return;
        }

        // A transport-level failure here almost always means the POST will fail too, so
        // say so now instead of letting it surface as a confusing SDP error later.
        if (reply->error() != QNetworkReply::NoError) {
            sgct::Log::Warning("WHEP: OPTIONS failed ("
                               + reply->errorString().toStdString()
                               + "); continuing without advertised ICE servers\n");
        }

        const QList<IceServerSpec> servers = iceServersFrom(reply);
        sgct::Log::Info("WHEP: OPTIONS returned " + std::to_string(servers.size())
                        + " ICE server(s)\n");
        Q_EMIT iceServersReady(servers);
    });
}

void WhepClient::sendOffer(const QString &sdpOffer)
{
    if (m_pending) {
        m_pending->abort();
        m_pending = nullptr;
    }

    QNetworkRequest request(m_endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1(kSdpContentType));
    request.setRawHeader("Accept", kSdpContentType);
    applyAuth(request);

    QNetworkReply *reply = m_network->post(request, sdpOffer.toUtf8());
    m_pending = reply;

    QPointer<WhepClient> self(this);
    connect(reply, &QNetworkReply::finished, this, [this, reply, self] {
        reply->deleteLater();
        if (!self) {
            return;
        }
        if (m_pending == reply) {
            m_pending = nullptr;
        }

        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError && status != 201 && status != 200) {
            Q_EMIT failed(u"WHEP POST failed (HTTP %1): %2"_s.arg(status).arg(reply->errorString()));
            return;
        }
        if (status != 201 && status != 200) {
            Q_EMIT failed(u"WHEP POST returned unexpected status %1"_s.arg(status));
            return;
        }

        const QByteArray location = reply->rawHeader("Location");
        if (!location.isEmpty()) {
            m_resourceUrl = m_endpoint.resolved(QUrl(QString::fromUtf8(location)));
        }

        const QString answer = QString::fromUtf8(reply->readAll());
        if (answer.trimmed().isEmpty()) {
            Q_EMIT failed(u"WHEP POST returned an empty SDP answer"_s);
            return;
        }

        sgct::Log::Info("WHEP: POST accepted (HTTP " + std::to_string(status)
                        + "), session " + m_resourceUrl.path().toStdString() + "\n");
        Q_EMIT answerReceived(answer);
    });
}

void WhepClient::deleteSession()
{
    if (m_resourceUrl.isEmpty()) {
        return;
    }

    QNetworkRequest request(m_resourceUrl);
    applyAuth(request);
    m_resourceUrl.clear();

    QNetworkReply *reply = m_network->deleteResource(request);
    connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
}
