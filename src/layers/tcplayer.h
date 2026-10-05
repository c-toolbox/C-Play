/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TCPLAYER_H
#define TCPLAYER_H
#include <layers/baselayer.h>
#include <QPointer>
#include <atomic>
#include <memory>
#include "tcpcontrolmanager.h"

class TcpLayer : public BaseLayer {
public:
    TcpLayer();
    ~TcpLayer() override;
    void initialize() override { m_hasInitialized = true; }
    bool existOnMasterOnly() const override { return true; }
    bool ready() const override { return true; }
    bool hasTexture() const override { return false; }
    void start() override;
    void stop() override {} // A trigger does not own the persistent connection.
    std::string commandId() const { return m_commandId; }
    void setCommandId(const std::string &id) { m_commandId = id; setNeedSync(); }
    void setManager(TcpControlManager *manager) { m_manager = manager; }
    int triggerStatus() const { return m_trigger->status.load(); }
    void encodeTypeCore(std::vector<std::byte> &data) override;
    void decodeTypeCore(const std::vector<std::byte> &data, unsigned int &pos) override;
private:
    struct Trigger {
        std::atomic<bool> alive { true };
        std::atomic<int> status { -1 }; // idle, 0 rejected, 1 dispatching, 2 accepted by Qt.
    };
    std::shared_ptr<Trigger> m_trigger = std::make_shared<Trigger>();
    QPointer<TcpControlManager> m_manager;
    std::string m_commandId;
};
#endif
