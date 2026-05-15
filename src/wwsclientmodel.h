/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WWSCLIENTMODEL_H
#define WWSCLIENTMODEL_H

#include <QObject>
#include <QStringList>
#include <QThread>
#include <QtQml/qqmlregistration.h>

class WwsRequestWorker : public QObject {
    Q_OBJECT

public:
    explicit WwsRequestWorker(QObject *parent = nullptr);

public Q_SLOTS:
    void doRequest(const QString &url, const QString &parameters, bool ignoreStatus);
    void fetchObsOptions(const QString &url, int optionType);

Q_SIGNALS:
    void requestFinished(int statusCode, const QString &responseBody, const QString &error);
    void obsOptionsFinished(int optionType, const QStringList &options, const QString &error);
};

class WwsClientModel : public QObject {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit WwsClientModel(QObject *parent = nullptr);
    ~WwsClientModel();

    Q_INVOKABLE void sendRequest(const QString &url,
                                 const QString &parameters = QString(),
                                 bool ignoreStatus = false);

    Q_INVOKABLE void updateObsOptions(const QString &url, int optionType);

    Q_PROPERTY(QStringList obsOptions READ obsOptions NOTIFY obsOptionsChanged)
    QStringList obsOptions() const;

    Q_PROPERTY(bool obsOptionsInProgress READ obsOptionsInProgress NOTIFY obsOptionsInProgressChanged)
    bool obsOptionsInProgress() const;

    Q_PROPERTY(QString obsOptionsError READ obsOptionsError NOTIFY obsOptionsChanged)
    QString obsOptionsError() const;

    Q_PROPERTY(QString lastResponseBody READ lastResponseBody NOTIFY responseChanged)
    QString lastResponseBody() const;

    Q_PROPERTY(int lastStatusCode READ lastStatusCode NOTIFY responseChanged)
    int lastStatusCode() const;

    Q_PROPERTY(QString lastError READ lastError NOTIFY responseChanged)
    QString lastError() const;

    Q_PROPERTY(bool requestInProgress READ requestInProgress NOTIFY requestInProgressChanged)
    bool requestInProgress() const;

Q_SIGNALS:
    void responseChanged();
    void requestInProgressChanged();
    void obsOptionsChanged();
    void obsOptionsInProgressChanged();
    void startRequest(const QString &url, const QString &parameters, bool ignoreStatus);
    void startObsOptionsRequest(const QString &url, int optionType);

private Q_SLOTS:
    void onRequestFinished(int statusCode, const QString &responseBody, const QString &error);
    void onObsOptionsFinished(int optionType, const QStringList &options, const QString &error);

private:
    QString m_lastResponseBody;
    int m_lastStatusCode = 0;
    QString m_lastError;
    bool m_requestInProgress = false;
    QStringList m_obsOptions;
    QString m_obsOptionsError;
    bool m_obsOptionsInProgress = false;
    int m_obsOptionsType = -1;

    QThread m_workerThread;
    WwsRequestWorker *m_worker = nullptr;
};

#endif // WWSCLIENTMODEL_H
