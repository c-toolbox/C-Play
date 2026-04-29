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
    if (m_url.empty() || !m_httpClientModel) {
        return;
    }
    m_httpClientModel->sendRequest(
        QString::fromStdString(m_url),
        m_method,
        QString::fromStdString(m_requestBody),
        QString::fromStdString(m_contentType));
}

void RestLayer::stop() {
    // Nothing to stop for a synchronous request
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

std::string RestLayer::requestBody() const {
    return m_requestBody;
}

void RestLayer::setRequestBody(const std::string& body) {
    m_requestBody = body;
    setNeedSync();
}

std::string RestLayer::contentType() const {
    return m_contentType;
}

void RestLayer::setContentType(const std::string& ct) {
    m_contentType = ct;
    setNeedSync();
}

void RestLayer::setHttpClientModel(HttpClientModel* model) {
    m_httpClientModel = model;
}

void RestLayer::encodeTypeCore(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_url);
    sgct::serializeObject(data, m_method);
    sgct::serializeObject(data, m_requestBody);
    sgct::serializeObject(data, m_contentType);
}

void RestLayer::decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_url);
    sgct::deserializeObject(data, pos, m_method);
    sgct::deserializeObject(data, pos, m_requestBody);
    sgct::deserializeObject(data, pos, m_contentType);
}
