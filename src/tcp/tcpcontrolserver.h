/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef TCPCONTROLSERVER_H
#define TCPCONTROLSERVER_H
#include <QObject>
#include <QTcpServer>
#include <QJsonObject>
#include <functional>
#include <optional>

// Master-only, bounded UTF-8 line transport for the REST action handlers.
// Every accepted client is a session with a stable ID, so the manager can push
// subscribed event updates to it with that session's terminator.
class TcpControlServer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool listening READ listening NOTIFY stateChanged)
    Q_PROPERTY(int port READ port NOTIFY stateChanged)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
public:
    using Handler = std::function<QJsonObject(const QString &, const QJsonObject &)>;
    // Handles session-scoped actions (subscriptions); nullopt passes the action to Handler.
    using SessionHandler = std::function<std::optional<QJsonObject>(quint64 session, const QString &path, const QJsonObject &parameters)>;
    explicit TcpControlServer(QObject *parent = nullptr);
    // Owners may already be partly destroyed; do not report the final disconnects.
    ~TcpControlServer() override { blockSignals(true); stop(); }
    void setHandler(Handler handler) { m_handler = std::move(handler); }
    void setSessionHandler(SessionHandler handler) { m_sessionHandler = std::move(handler); }
    bool start(const QString &address, int port);
    void stop();
    bool listening() const { return m_server.isListening(); }
    int port() const { return m_server.serverPort(); }
    int clientCount() const { return m_sessions.size(); }
    QString lastError() const { return m_lastError; }
    // Writes a payload followed by the session's terminator (none when raw). A client that
    // does not consume its data beyond MaximumPendingWriteBytes is disconnected.
    bool send(quint64 session, const QByteArray &payload, bool raw = false);
    // Terminator of replies and pushed updates; LF by default.
    void setTerminator(quint64 session, const QByteArray &terminator);
    // Whether action replies are written to the session; pushed updates are always sent.
    void setRepliesEnabled(quint64 session, bool enabled);
    static constexpr qsizetype MaximumMessageBytes = 65536;
    static constexpr qsizetype MaximumPendingWriteBytes = 1048576;
Q_SIGNALS:
    void stateChanged();
    // `address` is the canonical peer address; IPv4-mapped IPv6 peers use their IPv4 form.
    void clientConnected(quint64 session, const QString &address, int port);
    void clientDisconnected(quint64 session);
private:
    struct Session {
        quint64 id = 0;
        QByteArray buffer;
        QByteArray terminator = QByteArray("\n");
        bool replies = true;
    };
    QJsonObject execute(quint64 session, const QByteArray &line);
    void receive(QTcpSocket *socket);
    void drop(QTcpSocket *socket);
    QTcpServer m_server;
    QHash<QTcpSocket *, Session> m_sessions;
    QHash<quint64, QTcpSocket *> m_sockets;
    quint64 m_nextSession = 0;
    QHash<QString, QJsonObject> m_actions;
    QString m_lastError;
    Handler m_handler;
    SessionHandler m_sessionHandler;
    static constexpr int MaximumClients = 16;
};
#endif
