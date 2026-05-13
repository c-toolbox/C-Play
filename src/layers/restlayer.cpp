/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "restlayer.h"
#include "httpclientmodel.h"
#include <sgct/shareddata.h>

RestLayer::RestLayer() {
    m_type = REST;
    m_existOnMasterOnly = true;
}

RestLayer::~RestLayer() {
    cleanup();
}

void RestLayer::cleanup() {
    m_statusCallback = nullptr;
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
        delete m_workerThread;
        m_workerThread = nullptr;
        m_worker = nullptr;
    }
}

void RestLayer::initialize() {
    m_hasInitialized = true;
}

bool RestLayer::existOnMasterOnly() const {
    return true;
}

bool RestLayer::ready() const {
    return true;
}

bool RestLayer::hasTexture() const {
    return false;
}

void RestLayer::start() {
    if (m_url.empty()) {
        return;
    }

    // Lazily create the worker thread on first use
    if (!m_workerThread) {
        m_workerThread = new QThread();
        m_worker = new HttpRequestWorker();
        m_worker->moveToThread(m_workerThread);
        QObject::connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
        QObject::connect(m_worker, &HttpRequestWorker::requestFinished,
            [this](int statusCode, const QString &responseBody, const QString &error) {
                onRequestFinished(statusCode, responseBody, error);
            });
        m_workerThread->start();
    }

    // Set status to "in progress" (1)
    if (m_statusCallback) {
        m_statusCallback(1);
    }

    // Dispatch request to the worker thread
    QMetaObject::invokeMethod(m_worker, "doRequest", Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(m_url)),
        Q_ARG(int, m_method),
        Q_ARG(QString, QString::fromStdString(m_parameters)),
        Q_ARG(bool, m_ignoreStatus));
}

void RestLayer::stop() {
    // Nothing to stop for an async request
}

void RestLayer::onRequestFinished(int statusCode, const QString &responseBody, const QString &error) {
    Q_UNUSED(responseBody)
    Q_UNUSED(error)

    // status: 2=success (HTTP 2xx), 0=failure
    int layerStatus = (statusCode >= 200 && statusCode < 300) ? 2 : 0;

    if (m_statusCallback) {
        m_statusCallback(layerStatus);
    }
}

std::string RestLayer::url() const {
    return m_url;
}

void RestLayer::setUrl(const std::string& u) {
    m_url = u;
    setNeedSync();
}

int RestLayer::method() const {
    return m_method;
}

void RestLayer::setMethod(int m) {
    m_method = m;
    setNeedSync();
}

std::string RestLayer::parameters() const {
    return m_parameters;
}

void RestLayer::setParameters(const std::string& params) {
    m_parameters = params;
    setNeedSync();
}

bool RestLayer::ignoreStatus() const {
    return m_ignoreStatus;
}

void RestLayer::setIgnoreStatus(bool ignore) {
    m_ignoreStatus = ignore;
    setNeedSync();
}

void RestLayer::setHttpClientModel(HttpClientModel* model) {
    m_httpClientModel = model;
}

void RestLayer::setStatusCallback(StatusCallback cb) {
    m_statusCallback = cb;
}

void RestLayer::encodeTypeCore(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_url);
    sgct::serializeObject(data, m_method);
    sgct::serializeObject(data, m_parameters);
    sgct::serializeObject(data, m_ignoreStatus);
}

void RestLayer::decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_url);
    sgct::deserializeObject(data, pos, m_method);
    sgct::deserializeObject(data, pos, m_parameters);
    sgct::deserializeObject(data, pos, m_ignoreStatus);
}
