/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "httpclientmodel.h"
#include <cpp-httplib/httplib.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#pragma warning(disable : 4996)

HttpClientModel::HttpClientModel(QObject *parent)
    : QAbstractListModel(parent) {
}

HttpClientModel::~HttpClientModel() {
}

int HttpClientModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_titles.size();
}

QVariant HttpClientModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_titles.empty())
        return QVariant();
    if (!checkIndex(index))
        return QVariant();

    switch (role) {
    case titleRole:
        return m_titles.at(index.row());
    case urlRole:
        return m_urls.at(index.row());
    case methodRole:
        return m_methods.at(index.row());
    case bodyRole:
        return m_bodies.at(index.row());
    case contentTypeRole:
        return m_contentTypes.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> HttpClientModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[titleRole] = "title";
    roles[urlRole] = "url";
    roles[methodRole] = "method";
    roles[bodyRole] = "body";
    roles[contentTypeRole] = "contentType";
    return roles;
}

void HttpClientModel::updateCommandsList() {
    QFile file(QStringLiteral("./data/predefined-rest-commands.json"));

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning("Couldn't open predefined-rest-commands file.");
        return;
    }

    QByteArray fileData = file.readAll();
    QJsonDocument doc(QJsonDocument::fromJson(fileData));
    QJsonObject obj = doc.object();

    if (obj.contains(QStringLiteral("commands"))) {
        beginResetModel();
        m_titles.clear();
        m_urls.clear();
        m_methods.clear();
        m_bodies.clear();
        m_contentTypes.clear();

        QJsonArray arr = obj.value(QStringLiteral("commands")).toArray();
        for (auto v : arr) {
            QJsonObject o = v.toObject();
            bool enabled = true;
            if (o.contains(QStringLiteral("enabled"))) {
                enabled = o.value(QStringLiteral("enabled")).toBool();
            }
            if (enabled && o.contains(QStringLiteral("title")) && o.contains(QStringLiteral("url"))) {
                m_titles.append(o.value(QStringLiteral("title")).toString());
                m_urls.append(o.value(QStringLiteral("url")).toString());

                int method = 0;
                if (o.contains(QStringLiteral("method"))) {
                    QString methodStr = o.value(QStringLiteral("method")).toString().toUpper();
                    if (methodStr == QStringLiteral("POST")) method = 1;
                    else if (methodStr == QStringLiteral("PUT")) method = 2;
                    else if (methodStr == QStringLiteral("DELETE")) method = 3;
                }
                m_methods.append(method);
                m_bodies.append(o.contains(QStringLiteral("body")) ? o.value(QStringLiteral("body")).toString() : QStringLiteral(""));
                m_contentTypes.append(o.contains(QStringLiteral("contentType")) ? o.value(QStringLiteral("contentType")).toString() : QStringLiteral("application/json"));
            }
        }
        endResetModel();
        Q_EMIT commandsListChanged();
    }
}

int HttpClientModel::getNumberOfCommands() {
    return m_titles.size();
}

int HttpClientModel::sendRequest(const QString &url, int method,
                                 const QString &body, const QString &contentType) {
    std::string urlStr = url.toStdString();
    if (urlStr.empty()) {
        m_lastStatusCode = 0;
        m_lastResponseBody.clear();
        m_lastError = QStringLiteral("Empty URL");
        Q_EMIT responseChanged();
        return 0;
    }

    // Parse host:port and path
    std::string hostPort;
    std::string path = "/";

    size_t schemeEnd = urlStr.find("://");
    if (schemeEnd != std::string::npos) {
        size_t hostStart = schemeEnd + 3;
        size_t pathStart = urlStr.find('/', hostStart);
        if (pathStart != std::string::npos) {
            hostPort = urlStr.substr(0, pathStart);
            path = urlStr.substr(pathStart);
        } else {
            hostPort = urlStr;
        }
    } else {
        size_t pathStart = urlStr.find('/');
        if (pathStart != std::string::npos) {
            hostPort = "http://" + urlStr.substr(0, pathStart);
            path = urlStr.substr(pathStart);
        } else {
            hostPort = "http://" + urlStr;
        }
    }

    httplib::Client cli(hostPort);
    cli.set_connection_timeout(10, 0);
    cli.set_read_timeout(30, 0);

    std::string bodyStr = body.toStdString();
    std::string ctStr = contentType.toStdString();

    httplib::Result res;
    switch (method) {
    case 0: res = cli.Get(path); break;
    case 1: res = cli.Post(path, bodyStr, ctStr); break;
    case 2: res = cli.Put(path, bodyStr, ctStr); break;
    case 3: res = cli.Delete(path); break;
    default: res = cli.Get(path); break;
    }

    if (res) {
        m_lastStatusCode = res->status;
        m_lastResponseBody = QString::fromStdString(res->body);
        m_lastError.clear();
    } else {
        m_lastStatusCode = 0;
        m_lastResponseBody.clear();
        m_lastError = QString::fromStdString(httplib::to_string(res.error()));
    }

    Q_EMIT responseChanged();
    return m_lastStatusCode;
}

int HttpClientModel::triggerCommand(int index) {
    if (index < 0 || index >= m_titles.size())
        return 0;
    return sendRequest(m_urls.at(index), m_methods.at(index),
                       m_bodies.at(index), m_contentTypes.at(index));
}

void HttpClientModel::addCommand(const QString &title, const QString &url, int method,
                                 const QString &body, const QString &contentType) {
    beginInsertRows(QModelIndex(), m_titles.size(), m_titles.size());
    m_titles.append(title);
    m_urls.append(url);
    m_methods.append(method);
    m_bodies.append(body);
    m_contentTypes.append(contentType);
    endInsertRows();
    saveCommandsToFile();
    Q_EMIT commandsListChanged();
}

void HttpClientModel::updateCommand(int index, const QString &title, const QString &url, int method,
                                    const QString &body, const QString &contentType) {
    if (index < 0 || index >= m_titles.size())
        return;
    m_titles[index] = title;
    m_urls[index] = url;
    m_methods[index] = method;
    m_bodies[index] = body;
    m_contentTypes[index] = contentType;
    Q_EMIT dataChanged(this->index(index, 0), this->index(index, 0));
    saveCommandsToFile();
    Q_EMIT commandsListChanged();
}

void HttpClientModel::removeCommand(int index) {
    if (index < 0 || index >= m_titles.size())
        return;
    beginRemoveRows(QModelIndex(), index, index);
    m_titles.removeAt(index);
    m_urls.removeAt(index);
    m_methods.removeAt(index);
    m_bodies.removeAt(index);
    m_contentTypes.removeAt(index);
    endRemoveRows();
    saveCommandsToFile();
    Q_EMIT commandsListChanged();
}

QString HttpClientModel::lastResponseBody() const {
    return m_lastResponseBody;
}

int HttpClientModel::lastStatusCode() const {
    return m_lastStatusCode;
}

QString HttpClientModel::lastError() const {
    return m_lastError;
}

void HttpClientModel::saveCommandsToFile() {
    QJsonArray arr;
    for (int i = 0; i < m_titles.size(); ++i) {
        QJsonObject o;
        o.insert(QStringLiteral("title"), m_titles.at(i));
        o.insert(QStringLiteral("url"), m_urls.at(i));
        QString methodStr;
        switch (m_methods.at(i)) {
        case 1: methodStr = QStringLiteral("POST"); break;
        case 2: methodStr = QStringLiteral("PUT"); break;
        case 3: methodStr = QStringLiteral("DELETE"); break;
        default: methodStr = QStringLiteral("GET"); break;
        }
        o.insert(QStringLiteral("method"), methodStr);
        o.insert(QStringLiteral("body"), m_bodies.at(i));
        o.insert(QStringLiteral("contentType"), m_contentTypes.at(i));
        o.insert(QStringLiteral("enabled"), true);
        arr.append(o);
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("commands"), arr);
    QJsonDocument doc(obj);

    QFile file(QStringLiteral("./data/predefined-rest-commands.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}
