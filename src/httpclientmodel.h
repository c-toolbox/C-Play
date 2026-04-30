/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HTTPCLIENTMODEL_H
#define HTTPCLIENTMODEL_H

#include <QAbstractListModel>
#include <QThread>
#include <QtQml/qqmlregistration.h>

class HttpRequestWorker : public QObject {
    Q_OBJECT

public:
    explicit HttpRequestWorker(QObject *parent = nullptr);

public Q_SLOTS:
    void doRequest(const QString &url, int method,
                   const QString &parameters, bool ignoreStatus);

Q_SIGNALS:
    void requestFinished(int statusCode, const QString &responseBody, const QString &error);
};

class HttpClientModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit HttpClientModel(QObject *parent = nullptr);
    ~HttpClientModel();

    enum {
        titleRole = Qt::UserRole,
        urlRole,
        methodRole,
        parametersRole,
        ignoreStatusRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void updateCommandsList();

    Q_PROPERTY(int numberOfCommands READ getNumberOfCommands NOTIFY commandsListChanged)
    int getNumberOfCommands();

    // Send a request asynchronously on a worker thread.
    // Returns immediately. Results are delivered via responseChanged signal.
    Q_INVOKABLE void sendRequest(const QString &url, int method = 0,
                                 const QString &parameters = QString(),
                                 bool ignoreStatus = false);

    // Trigger a predefined command by index
    Q_INVOKABLE void triggerCommand(int index);

    // Add a new command to the list and save to file
    Q_INVOKABLE void addCommand(const QString &title, const QString &url, int method,
                                const QString &parameters, bool ignoreStatus = false);

    // Update an existing command and save to file
    Q_INVOKABLE void updateCommand(int index, const QString &title, const QString &url, int method,
                                   const QString &parameters, bool ignoreStatus = false);

    // Remove a command and save to file
    Q_INVOKABLE void removeCommand(int index);

    Q_PROPERTY(QString lastResponseBody READ lastResponseBody NOTIFY responseChanged)
    QString lastResponseBody() const;

    Q_PROPERTY(int lastStatusCode READ lastStatusCode NOTIFY responseChanged)
    int lastStatusCode() const;

    Q_PROPERTY(QString lastError READ lastError NOTIFY responseChanged)
    QString lastError() const;

    Q_PROPERTY(bool requestInProgress READ requestInProgress NOTIFY requestInProgressChanged)
    bool requestInProgress() const;

Q_SIGNALS:
    void commandsListChanged();
    void responseChanged();
    void requestInProgressChanged();
    void startRequest(const QString &url, int method,
                      const QString &parameters, bool ignoreStatus);

private Q_SLOTS:
    void onRequestFinished(int statusCode, const QString &responseBody, const QString &error);

private:
    void saveCommandsToFile();

    QStringList m_titles;
    QStringList m_urls;
    QList<int> m_methods;
    QStringList m_parameters;
    QList<bool> m_ignoreStatus;

    QString m_lastResponseBody;
    int m_lastStatusCode = 0;
    QString m_lastError;
    bool m_requestInProgress = false;

    QThread m_workerThread;
    HttpRequestWorker *m_worker = nullptr;
};

#endif // HTTPCLIENTMODEL_H
