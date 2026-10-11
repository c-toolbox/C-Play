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

// Master-only, bounded UTF-8 line transport for the REST action handlers.
class TcpControlServer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool listening READ listening NOTIFY stateChanged)
    Q_PROPERTY(int port READ port NOTIFY stateChanged)
    Q_PROPERTY(int clientCount READ clientCount NOTIFY stateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
public:
    using Handler = std::function<QJsonObject(const QString &, const QJsonObject &)>;
    explicit TcpControlServer(QObject *parent = nullptr);
    ~TcpControlServer() override { stop(); }
    void setHandler(Handler handler) { m_handler = std::move(handler); }
    bool start(const QString &address, int port);
    void stop();
    bool listening() const { return m_server.isListening(); }
    int port() const { return m_server.serverPort(); }
    int clientCount() const { return m_buffers.size(); }
    QString lastError() const { return m_lastError; }
Q_SIGNALS:
    void stateChanged();
private:
    QJsonObject execute(const QByteArray &line);
    void receive(QTcpSocket *socket);
    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;
    QHash<QString, QJsonObject> m_actions;
    QString m_lastError;
    Handler m_handler;
    static constexpr qsizetype MaximumMessageBytes = 65536;
    static constexpr int MaximumClients = 16;
};
#endif
