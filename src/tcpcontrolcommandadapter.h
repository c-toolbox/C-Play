/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TCPCONTROLCOMMANDADAPTER_H
#define TCPCONTROLCOMMANDADAPTER_H
#include <QObject>

// C-Play protocol: one UTF-8 JSON object per transport frame.
class TcpControlCommandAdapter : public QObject {
    Q_OBJECT
public:
    explicit TcpControlCommandAdapter(QObject *parent = nullptr) : QObject(parent) {}
public Q_SLOTS:
    void parseMessage(const QByteArray &payload);
Q_SIGNALS:
    void commandReceived(const QString &operation, const QString &parameter);
    void protocolError(const QString &description);
};
#endif
