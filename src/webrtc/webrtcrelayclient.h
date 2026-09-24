/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "webrtc/webrtcmediasource.h"

#include <rtc/common.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>

namespace rtc {
class WebSocket;
class PeerConnection;
class DataChannel;
} // namespace rtc

/// Node-side media source: instead of pulling the WHEP endpoint itself (which would make
/// every node hit the same upstream address), it subscribes to the master's WebRtcHub and
/// receives the depacketized frames over a unique, reliable + ordered data channel.
/// Signaling (offer/answer + trickle ICE) runs over a WebSocket; media arrives framed on the
/// data channel (see WebRtcRelayFrameType in webrtctypes.h) and feeds exactly the same
/// decode pipeline as a direct pull would.
///
/// The node is always the offerer: it connects to the hub, announces its layer id, waits for
/// "ready", then creates the SDP offer. Once the PeerConnection is connected it opens the
/// data channel and starts decoding when the metadata frame (the negotiated codec) arrives.
class WebRtcRelayClient : public WebRtcMediaSource {
    Q_OBJECT

public:
    /// \p masterAddress host/IP of the master from the cluster config, \p hubPort the hub's
    /// WebSocket port and \p layerId identifies which layer's stream to subscribe to.
    WebRtcRelayClient(std::string masterAddress, std::uint16_t hubPort, std::string layerId,
                      QObject *parent = nullptr);
    ~WebRtcRelayClient() override;

    /// Opens the WebSocket and begins signaling. Safe to call repeatedly (restarts).
    void start() override;

    /// Closes everything and resets to Idle. Safe to call from any thread and repeatedly.
    void stop() override;

    /// Asks the hub (and thus the master's WHEP session) for an IDR, so a starved decoder on
    /// this node can start decoding again. A no-op while not connected.
    void requestKeyframe() override;

private:
    void setState(WebRtcStreamState state);
    /// Emits errorOccurred + Failed exactly once per session (idempotent). The layer's
    /// reconnect machinery then calls stop()/start() again on the main thread.
    void fail(const QString &reason);
    /// Creates the PeerConnection and sends the offer; runs when the hub answers "ready".
    void beginIce();
    void onSignalingMessage(const std::string &text);
    void onDataChannelMessage(const rtc::binary &message);

    std::string m_masterAddress;
    std::uint16_t m_hubPort;
    std::string m_layerId;

    // True while a session is running. Every libdatachannel callback captures its own copy,
    // so stop() can invalidate the session from the main thread without racing them. Atomic
    // because beginIce() (WebSocket thread) also loads it.
    std::atomic<std::shared_ptr<std::atomic<bool>>> m_session{nullptr};

    // Written by start()/beginIce() (main or WebSocket thread) and reset by stop() (main
    // thread), so both are atomic shared pointers; callbacks capture plain copies instead.
    std::atomic<std::shared_ptr<rtc::WebSocket>> m_ws{nullptr};
    std::atomic<std::shared_ptr<rtc::PeerConnection>> m_pc{nullptr};

    // libdatachannel keeps only weak references to user-created data channels, so this member
    // must keep the channel alive for as long as the session exists. If it were a local in
    // beginIce(), the channel would be destroyed when that function returns and SCTP would
    // silently skip stream assignment/opening (expired weak_ptr), leaving both sides "connected"
    // with no data channel ever opening.
    std::atomic<std::shared_ptr<rtc::DataChannel>> m_dc{nullptr};

    std::atomic<WebRtcStreamState> m_state{WebRtcStreamState::Idle};
    std::atomic<bool> m_iceStarted{false};
};
