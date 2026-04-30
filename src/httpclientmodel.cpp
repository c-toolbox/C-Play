/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "httpclientmodel.h"
#ifdef OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif
#include <cpp-httplib/httplib.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#pragma warning(disable : 4996)

// --- HttpRequestWorker implementation ---

HttpRequestWorker::HttpRequestWorker(QObject *parent)
    : QObject(parent) {
}

void HttpRequestWorker::doRequest(const QString &url, int method,
                                  const QString &parameters) {
    int statusCode = 0;
    QString responseBody;
    QString error;

    try {
        std::string urlStr = url.toStdString();
        if (urlStr.empty()) {
            error = QStringLiteral("Empty URL");
            Q_EMIT requestFinished(0, QString(), error);
            return;
        }

#ifndef OPENSSL_SUPPORT
        if (urlStr.find("https://") == 0) {
            error = QStringLiteral("HTTPS is not supported (OpenSSL not available). Use HTTP instead.");
            Q_EMIT requestFinished(0, QString(), error);
            return;
        }
#endif

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

        // Build httplib::Params from the parameters string.
        // Parameters are stored as a JSON array: [{"name":"key","value":"val"}, ...]
        // For backward compatibility, also support the old "name=value&name=value" format.
        httplib::Params params;
        if (!parameters.isEmpty()) {
            QJsonDocument paramDoc = QJsonDocument::fromJson(parameters.toUtf8());
            if (paramDoc.isArray()) {
                QJsonArray paramArr = paramDoc.array();
                for (auto v : paramArr) {
                    QJsonObject p = v.toObject();
                    QString name = p.value(QStringLiteral("name")).toString();
                    QString value = p.value(QStringLiteral("value")).toString();
                    if (!name.isEmpty()) {
                        params.emplace(name.toStdString(), value.toStdString());
                    }
                }
            } else {
                // Backward compatibility: parse "name=value&name=value" format
                QStringList pairs = parameters.split(QStringLiteral("&"), Qt::SkipEmptyParts);
                for (const QString &pair : pairs) {
                    int eqIdx = pair.indexOf(QLatin1Char('='));
                    if (eqIdx >= 0) {
                        params.emplace(pair.left(eqIdx).toStdString(), pair.mid(eqIdx + 1).toStdString());
                    } else {
                        params.emplace(pair.toStdString(), std::string());
                    }
                }
            }
        }

        httplib::Result res;
        switch (method) {
        case 0: // GET
            if (!params.empty()) {
                std::string queryPath = path + "?" + httplib::detail::params_to_query_str(params);
                res = cli.Get(queryPath);
            } else {
                res = cli.Get(path);
            }
            break;
        case 1: // POST
            res = cli.Post(path, params);
            break;
        case 2: // PUT
            res = cli.Put(path, httplib::detail::params_to_query_str(params), "application/x-www-form-urlencoded");
            break;
        case 3: // DELETE
            if (!params.empty()) {
                std::string queryPath = path + "?" + httplib::detail::params_to_query_str(params);
                res = cli.Delete(queryPath);
            } else {
                res = cli.Delete(path);
            }
            break;
        default:
            res = cli.Get(path);
            break;
        }

        if (res) {
            statusCode = res->status;
            responseBody = QString::fromStdString(res->body);
        } else {
            error = QString::fromStdString(httplib::to_string(res.error()));
        }
    } catch (const std::exception &e) {
        error = QString::fromStdString(std::string("Exception: ") + e.what());
    } catch (...) {
        error = QStringLiteral("Unknown exception occurred during request");
    }

    Q_EMIT requestFinished(statusCode, responseBody, error);
}

// --- HttpClientModel implementation ---

HttpClientModel::HttpClientModel(QObject *parent)
    : QAbstractListModel(parent) {
    m_worker = new HttpRequestWorker();
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(this, &HttpClientModel::startRequest, m_worker, &HttpRequestWorker::doRequest);
    connect(m_worker, &HttpRequestWorker::requestFinished, this, &HttpClientModel::onRequestFinished);

    m_workerThread.start();
}

HttpClientModel::~HttpClientModel() {
    m_workerThread.quit();
    m_workerThread.wait();
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
    case parametersRole:
        return m_parameters.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> HttpClientModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[titleRole] = "title";
    roles[urlRole] = "url";
    roles[methodRole] = "method";
    roles[parametersRole] = "parameters";
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
        m_parameters.clear();

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

                // Parameters are stored as a JSON array of {name, value} objects
                if (o.contains(QStringLiteral("parameters"))) {
                    QJsonValue paramVal = o.value(QStringLiteral("parameters"));
                    if (paramVal.isArray()) {
                        QJsonDocument paramDoc(paramVal.toArray());
                        m_parameters.append(QString::fromUtf8(paramDoc.toJson(QJsonDocument::Compact)));
                    } else {
                        // Backward compatibility: string format
                        m_parameters.append(paramVal.toString());
                    }
                } else {
                    m_parameters.append(QStringLiteral(""));
                }
            }
        }
        endResetModel();
        Q_EMIT commandsListChanged();
    }
}

int HttpClientModel::getNumberOfCommands() {
    return m_titles.size();
}

void HttpClientModel::sendRequest(const QString &url, int method,
                                  const QString &parameters) {
    m_requestInProgress = true;
    Q_EMIT requestInProgressChanged();
    Q_EMIT startRequest(url, method, parameters);
}

void HttpClientModel::onRequestFinished(int statusCode, const QString &responseBody, const QString &error) {
    m_lastStatusCode = statusCode;
    m_lastResponseBody = responseBody;
    m_lastError = error;
    m_requestInProgress = false;
    Q_EMIT requestInProgressChanged();
    Q_EMIT responseChanged();
}

void HttpClientModel::triggerCommand(int index) {
    if (index < 0 || index >= m_titles.size())
        return;
    sendRequest(m_urls.at(index), m_methods.at(index),
                m_parameters.at(index));
}

void HttpClientModel::addCommand(const QString &title, const QString &url, int method,
                                 const QString &parameters) {
    beginInsertRows(QModelIndex(), m_titles.size(), m_titles.size());
    m_titles.append(title);
    m_urls.append(url);
    m_methods.append(method);
    m_parameters.append(parameters);
    endInsertRows();
    saveCommandsToFile();
    Q_EMIT commandsListChanged();
}

void HttpClientModel::updateCommand(int index, const QString &title, const QString &url, int method,
                                    const QString &parameters) {
    if (index < 0 || index >= m_titles.size())
        return;
    m_titles[index] = title;
    m_urls[index] = url;
    m_methods[index] = method;
    m_parameters[index] = parameters;
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
    m_parameters.removeAt(index);
    endRemoveRows();
    saveCommandsToFile();
    Q_EMIT commandsListChanged();
}

bool HttpClientModel::requestInProgress() const {
    return m_requestInProgress;
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

        // Save parameters as a JSON array
        QJsonDocument paramDoc = QJsonDocument::fromJson(m_parameters.at(i).toUtf8());
        if (paramDoc.isArray()) {
            o.insert(QStringLiteral("parameters"), paramDoc.array());
        } else {
            o.insert(QStringLiteral("parameters"), QJsonArray());
        }

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
