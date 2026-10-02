/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODESTREAMLAYER_H
#define NODESTREAMLAYER_H

#include <layers/baselayer.h>
#include "nodestreamprotocol.h"
#include "nodestreamreceiver.h"

#include <memory>
#include <string>

// Node-only layer that shows the texture a master layer streams with "Send to nodes".
class NodeStreamLayer : public BaseLayer {
public:
    NodeStreamLayer();
    ~NodeStreamLayer();

    void cleanup() override;
    void initialize() override;
    void update(bool updateRendering = true) override;
    bool ready() const override;
    bool hasTexture() const override;
    void collectLoadStatus() override;

    void decodeTypeCore(const std::vector<std::byte> &data, unsigned int &pos) override;
    void decodeTypeAlways(const std::vector<std::byte> &data, unsigned int &pos) override;
    void decodeTypeProperties(const std::vector<std::byte> &data, unsigned int &pos) override;

private:
    void ensureReceiver();
    bool upload(const NodeStreamReceiver::Frame &frame);

    std::unique_ptr<NodeStreamReceiver> m_receiver;

    // Synced from the master.
    std::string m_group;
    int m_port = 0;
    uint8_t m_syncMode = static_cast<uint8_t>(nodestream::SyncMode::FrameLocked);
    int m_maxWaitMs = 3;
    uint32_t m_streamId = 0;
    uint32_t m_sessionId = 0;
    uint32_t m_targetFrameId = 0;
    bool m_masterSending = false;

    // Receiver state.
    std::string m_activeGroup;
    int m_activePort = 0;
    uint32_t m_activeStreamId = 0;
    int64_t m_lastStartAttemptNs = 0;

    // Texture state.
    unsigned int m_texture = 0;
    nodestream::Format m_texFormat = nodestream::Format::BC1;
    int m_texWidth = 0;
    int m_texHeight = 0;
    bool m_hasFrame = false;
    bool m_hasLastUploaded = false;
    uint32_t m_lastUploadedSession = 0;
    uint32_t m_lastUploadedFrameId = 0;
};

#endif // NODESTREAMLAYER_H
