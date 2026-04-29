/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef HTTPCLIENTMODEL_H
#define HTTPCLIENTMODEL_H

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

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
        bodyRole,
        contentTypeRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void updateCommandsList();

    Q_PROPERTY(int numberOfCommands READ getNumberOfCommands NOTIFY commandsListChanged)
    int getNumberOfCommands();

    // Send a request and return synchronously (blocking).
    // Returns status code (0 on error). Response body and error available via properties.
    Q_INVOKABLE int sendRequest(const QString &url, int method = 0,
                                const QString &body = QString(),
                                const QString &contentType = QStringLiteral("application/json"));

    // Trigger a predefined command by index
    Q_INVOKABLE int triggerCommand(int index);

    // Add a new command to the list and save to file
    Q_INVOKABLE void addCommand(const QString &title, const QString &url, int method,
                                const QString &body, const QString &contentType);

    // Update an existing command and save to file
    Q_INVOKABLE void updateCommand(int index, const QString &title, const QString &url, int method,
                                   const QString &body, const QString &contentType);

    // Remove a command and save to file
    Q_INVOKABLE void removeCommand(int index);

    Q_PROPERTY(QString lastResponseBody READ lastResponseBody NOTIFY responseChanged)
    QString lastResponseBody() const;

    Q_PROPERTY(int lastStatusCode READ lastStatusCode NOTIFY responseChanged)
    int lastStatusCode() const;

    Q_PROPERTY(QString lastError READ lastError NOTIFY responseChanged)
    QString lastError() const;

Q_SIGNALS:
    void commandsListChanged();
    void responseChanged();

private:
    void saveCommandsToFile();

    QStringList m_titles;
    QStringList m_urls;
    QList<int> m_methods;
    QStringList m_bodies;
    QStringList m_contentTypes;

    QString m_lastResponseBody;
    int m_lastStatusCode = 0;
    QString m_lastError;
};

#endif // HTTPCLIENTMODEL_H
