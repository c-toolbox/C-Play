/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODESTREAMSOCKET_H
#define NODESTREAMSOCKET_H

#include <cstddef>
#include <cstdint>
#include <string>

// Minimal IPv4 UDP multicast socket over raw Winsock/BSD sockets, used from
// dedicated sender/receiver threads (no event loop needed).
class NodeStreamSocket {
public:
    NodeStreamSocket();
    ~NodeStreamSocket();

    NodeStreamSocket(const NodeStreamSocket &) = delete;
    NodeStreamSocket &operator=(const NodeStreamSocket &) = delete;

    // An empty interfaceAddress lets the OS pick the outgoing interface.
    bool openSender(const std::string &group, uint16_t port, const std::string &interfaceAddress,
                    int ttl, bool loopback, int sendBufferBytes);
    // An empty interfaceAddress joins the group on the default interface.
    bool openReceiver(const std::string &group, uint16_t port, const std::string &interfaceAddress,
                      int receiveBufferBytes, int receiveTimeoutMs);
    void close();
    bool isOpen() const;

    // Sends header + payload as one datagram without copying them together.
    bool send(const void *header, size_t headerLength, const void *payload, size_t payloadLength);
    // Returns the datagram size, 0 on timeout and -1 on error.
    int receive(void *buffer, size_t length);

    const std::string &lastError() const;
    // Effective kernel buffer sizes, or -1 when unknown.
    int sendBufferSize() const;
    int receiveBufferSize() const;

    // Resolves a host name or dotted address to a dotted IPv4 address, or empty.
    static std::string resolveIPv4(const std::string &host);
    static bool isLoopbackAddress(const std::string &dotted);
    // Transmit link speed of the adapter with this IPv4 address, or of the fastest
    // operational adapter when the address is empty. 0 when unknown.
    static uint64_t linkSpeedMbps(const std::string &interfaceAddress);
    // Sleeps until the steady_clock time in nanoseconds, with sub-millisecond precision.
    static void waitUntilNs(int64_t steadyNs);

private:
    void setError(const std::string &what);

    uintptr_t m_socket;
    bool m_wsaStarted = false;
    uint32_t m_groupAddr = 0;
    uint16_t m_port = 0;
    uint32_t m_joinedInterface = 0;
    bool m_joined = false;
    std::string m_lastError;
};

#endif // NODESTREAMSOCKET_H
