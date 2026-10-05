/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "tcplayer.h"
#include <sgct/shareddata.h>

TcpLayer::TcpLayer() {
    m_type = TCP;
    m_existOnMasterOnly = true;
}

TcpLayer::~TcpLayer() {
    m_trigger->alive = false;
}

void TcpLayer::start() {
    if (!m_manager || m_commandId.empty()) {
        m_trigger->status = 0;
        return;
    }
    const auto trigger = m_trigger;
    const auto manager = m_manager;
    const QString id = QString::fromStdString(m_commandId);
    trigger->status = 1;
    // Layer starts may originate outside the GUI thread. The manager and its
    // sockets always run on the application's thread. Do not capture this layer.
    QMetaObject::invokeMethod(manager, [manager, trigger, id] {
        if (trigger->alive && manager)
            trigger->status = manager->triggerCommand(id) ? 2 : 0;
    }, Qt::AutoConnection);
}

void TcpLayer::encodeTypeCore(std::vector<std::byte> &data) {
    sgct::serializeObject(data, m_commandId);
}

void TcpLayer::decodeTypeCore(const std::vector<std::byte> &data, unsigned int &pos) {
    sgct::deserializeObject(data, pos, m_commandId);
}
