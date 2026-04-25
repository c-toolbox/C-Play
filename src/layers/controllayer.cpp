/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "controllayer.h"
#include <sgct/shareddata.h>

ControlLayer::DispatchCallback ControlLayer::s_dispatchCallback = nullptr;

ControlLayer::ControlLayer() {
    m_type = CONTROL;
    m_existOnMasterOnly = true;
}

void ControlLayer::initialize() {
    m_hasInitialized = true;
}

bool ControlLayer::existOnMasterOnly() const {
    return true;
}

bool ControlLayer::ready() const {
    return true;
}

bool ControlLayer::hasTexture() const {
    return false;
}

void ControlLayer::start() {
    if (s_dispatchCallback && !m_operation.empty()) {
        s_dispatchCallback(m_operation, m_parameter);
    }
}

void ControlLayer::stop() {
    // Nothing to stop
}

std::string ControlLayer::operation() const {
    return m_operation;
}

void ControlLayer::setOperation(const std::string& op) {
    m_operation = op;
    setNeedSync();
}

std::string ControlLayer::parameter() const {
    return m_parameter;
}

void ControlLayer::setParameter(const std::string& param) {
    m_parameter = param;
    setNeedSync();
}

void ControlLayer::encodeTypeCore(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_operation);
    sgct::serializeObject(data, m_parameter);
}

void ControlLayer::decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_operation);
    sgct::deserializeObject(data, pos, m_parameter);
}

void ControlLayer::setDispatchCallback(DispatchCallback cb) {
    s_dispatchCallback = cb;
}
