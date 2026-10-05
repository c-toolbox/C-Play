/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TCPCONTROLCLIENT_H
#define TCPCONTROLCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

// Event-loop driven transport. All methods must run on this object's thread.
// Message framing is independent of the command language used by the peer.
class TcpControlClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ isConnected NOTIFY connectedChanged)
    Q_PROPERTY(ConnectionState connectionState READ connectionState NOTIFY connectionStateChanged)
    Q_PROPERTY(QString stateText READ stateText NOTIFY connectionStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
public:
    enum ConnectionState { Stopped, Connecting, Connected, WaitingToReconnect, Disconnecting };
    Q_ENUM(ConnectionState)
    enum FramingMode { Delimiter, LengthPrefix };
    Q_ENUM(FramingMode)

    struct Options {
        QString host;
        int port = 0;
        bool autoReconnect = true;
        int connectTimeoutMs = 5000;
        int retryInitialMs = 1000;
        int retryMaximumMs = 30000;
        int stableConnectionMs = 10000;
        FramingMode framing = Delimiter;
        QByteArray receiveTerminator = "\n";
        QByteArray sendTerminator = "\n";
        int maximumMessageBytes = 1024 * 1024;
        int maximumPendingWriteBytes = 1024 * 1024;
        bool operator==(const Options &) const = default;
    };

    explicit TcpControlClient(QObject *parent = nullptr);
    ~TcpControlClient() override;
    // Invalid configuration stops the client; valid changes restart an active client.
    bool configure(const Options &options);
    const Options &options() const { return m_options; }
    bool isConnected() const { return m_state == Connected; }
    ConnectionState connectionState() const { return m_state; }
    QString stateText() const;
    QString lastError() const { return m_lastError; }

    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE bool sendText(const QString &command);
    // Exact wire bytes, with no framing or text conversion added.
    Q_INVOKABLE bool sendBinary(const QByteArray &command);
    // Apply the configured framing to a binary payload (32-bit big-endian length
    // in LengthPrefix mode, sendTerminator in Delimiter mode).
    bool sendMessage(const QByteArray &payload);

Q_SIGNALS:
    void messageReceived(const QByteArray &payload);
    void textCommandReceived(const QString &command);
    void connectionStateChanged(ConnectionState state);
    void connectedChanged(bool connected);
    void lastErrorChanged();
    void errorOccurred(const QString &description);
    void reconnectScheduled(int delayMs);
    void bytesWritten(qint64 count);

private:
    bool validate(const Options &options);
    void connectSocket();
    void releaseSocket();
    void failed(const QString &reason);
    void receive();
    void setState(ConnectionState state);
    void reportError(const QString &reason);
    void clearError();

    Options m_options;
    QTcpSocket *m_socket = nullptr;
    QTimer m_connectTimer;
    QTimer m_retryTimer;
    QTimer m_stableTimer;
    QTimer m_stopTimer;
    QByteArray m_receiveBuffer;
    QString m_lastError;
    ConnectionState m_state = Stopped;
    bool m_running = false;
    int m_retryDelayMs = 1000;
    quint64 m_session = 0;
};
#endif
