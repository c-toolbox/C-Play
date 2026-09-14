/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

/// One ICE server advertised by a WHEP server in a Link header.
struct IceServerSpec {
    QString url;
    QString username;
    QString credential;
};

/// WHEP (WebRTC-HTTP Egress Protocol) signaling against MediaMTX and compatible servers.
///
/// Flow: OPTIONS (optional, for ICE servers) -> POST offer -> 201 + answer +
/// Location -> DELETE on teardown. The offer is posted with ICE gathering already
/// complete, so no PATCH/trickle round trip is needed; on a LAN gathering finishes
/// in milliseconds.
class WhepClient : public QObject
{
    Q_OBJECT

public:
    explicit WhepClient(QObject *parent = nullptr);
    ~WhepClient() override;

    void setEndpoint(const QUrl &url);
    void setCredentials(const QString &username, const QString &password);

    /// Queries the endpoint for advertised ICE servers. Emits iceServersReady even
    /// when the request fails, with an empty list, so callers can always proceed.
    void requestIceServers();

    void sendOffer(const QString &sdpOffer);

    /// Best-effort session teardown. Safe to call when no session is active.
    void deleteSession();

    bool hasSession() const;

Q_SIGNALS:
    void iceServersReady(const QList<IceServerSpec> &servers);
    void answerReceived(const QString &sdpAnswer);
    void failed(const QString &reason);

private:
    void applyAuth(QNetworkRequest &request) const;
    QList<IceServerSpec> iceServersFrom(QNetworkReply *reply) const;

    QNetworkAccessManager *m_network = nullptr;
    QUrl m_endpoint;
    QString m_username;
    QString m_password;

    QUrl m_resourceUrl;
    QNetworkReply *m_pending = nullptr;
};
