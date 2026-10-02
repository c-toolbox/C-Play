/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <unistd.h>
#endif

#include "nodestreamsocket.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr uintptr_t kInvalid = static_cast<uintptr_t>(~static_cast<uintptr_t>(0));

#ifdef _WIN32
using NativeSocket = SOCKET;
int lastSocketError() { return WSAGetLastError(); }
bool isTimeoutError(int e) { return e == WSAETIMEDOUT || e == WSAEWOULDBLOCK; }
#else
using NativeSocket = int;
int lastSocketError() { return errno; }
bool isTimeoutError(int e) { return e == EAGAIN || e == EWOULDBLOCK || e == EINTR; }
#endif

NativeSocket native(uintptr_t s) {
    return static_cast<NativeSocket>(s);
}

bool parseIPv4(const std::string &dotted, uint32_t &out) {
    in_addr addr{};
    if (inet_pton(AF_INET, dotted.c_str(), &addr) != 1)
        return false;
    out = addr.s_addr;
    return true;
}

} // namespace

NodeStreamSocket::NodeStreamSocket()
    : m_socket(kInvalid) {
#ifdef _WIN32
    WSADATA wsaData;
    m_wsaStarted = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif
}

NodeStreamSocket::~NodeStreamSocket() {
    close();
#ifdef _WIN32
    if (m_wsaStarted)
        WSACleanup();
#endif
}

void NodeStreamSocket::setError(const std::string &what) {
    m_lastError = what + " (error " + std::to_string(lastSocketError()) + ")";
}

const std::string &NodeStreamSocket::lastError() const {
    return m_lastError;
}

bool NodeStreamSocket::isOpen() const {
    return m_socket != kInvalid;
}

void NodeStreamSocket::close() {
    if (m_socket == kInvalid)
        return;

    if (m_joined) {
        ip_mreq mreq{};
        mreq.imr_multiaddr.s_addr = m_groupAddr;
        mreq.imr_interface.s_addr = m_joinedInterface;
        setsockopt(native(m_socket), IPPROTO_IP, IP_DROP_MEMBERSHIP, reinterpret_cast<const char *>(&mreq), sizeof(mreq));
        m_joined = false;
    }

#ifdef _WIN32
    closesocket(native(m_socket));
#else
    ::close(native(m_socket));
#endif
    m_socket = kInvalid;
}

bool NodeStreamSocket::openSender(const std::string &group, uint16_t port, const std::string &interfaceAddress,
                                  int ttl, bool loopback, int sendBufferBytes) {
    close();

    if (!parseIPv4(group, m_groupAddr)) {
        m_lastError = "Invalid multicast group " + group;
        return false;
    }
    m_port = port;

    NativeSocket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (s == INVALID_SOCKET) {
#else
    if (s < 0) {
#endif
        setError("socket() failed");
        return false;
    }
    m_socket = static_cast<uintptr_t>(s);

    if (sendBufferBytes > 0) {
        setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char *>(&sendBufferBytes), sizeof(sendBufferBytes));
    }

#ifdef _WIN32
    DWORD ttlValue = static_cast<DWORD>(ttl);
    DWORD loopValue = loopback ? 1 : 0;
#else
    unsigned char ttlValue = static_cast<unsigned char>(ttl);
    unsigned char loopValue = loopback ? 1 : 0;
#endif
    if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL, reinterpret_cast<const char *>(&ttlValue), sizeof(ttlValue)) != 0) {
        setError("IP_MULTICAST_TTL failed");
        close();
        return false;
    }
    setsockopt(s, IPPROTO_IP, IP_MULTICAST_LOOP, reinterpret_cast<const char *>(&loopValue), sizeof(loopValue));

    if (!interfaceAddress.empty()) {
        in_addr iface{};
        if (inet_pton(AF_INET, interfaceAddress.c_str(), &iface) != 1) {
            m_lastError = "Invalid interface address " + interfaceAddress;
            close();
            return false;
        }
        if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_IF, reinterpret_cast<const char *>(&iface), sizeof(iface)) != 0) {
            setError("IP_MULTICAST_IF " + interfaceAddress + " failed");
            close();
            return false;
        }
    }

    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(port);
    dest.sin_addr.s_addr = m_groupAddr;
    if (connect(s, reinterpret_cast<const sockaddr *>(&dest), sizeof(dest)) != 0) {
        setError("connect() to " + group + " failed");
        close();
        return false;
    }

    return true;
}

bool NodeStreamSocket::openReceiver(const std::string &group, uint16_t port, const std::string &interfaceAddress,
                                    int receiveBufferBytes, int receiveTimeoutMs) {
    close();

    if (!parseIPv4(group, m_groupAddr)) {
        m_lastError = "Invalid multicast group " + group;
        return false;
    }
    m_port = port;

    NativeSocket s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef _WIN32
    if (s == INVALID_SOCKET) {
#else
    if (s < 0) {
#endif
        setError("socket() failed");
        return false;
    }
    m_socket = static_cast<uintptr_t>(s);

    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&reuse), sizeof(reuse));

    if (receiveBufferBytes > 0) {
        setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char *>(&receiveBufferBytes), sizeof(receiveBufferBytes));
    }

#ifdef _WIN32
    DWORD timeout = static_cast<DWORD>(receiveTimeoutMs);
#else
    timeval timeout{};
    timeout.tv_sec = receiveTimeoutMs / 1000;
    timeout.tv_usec = (receiveTimeoutMs % 1000) * 1000;
#endif
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char *>(&timeout), sizeof(timeout));

    sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
#ifdef _WIN32
    // Windows does not allow binding to a multicast address.
    local.sin_addr.s_addr = htonl(INADDR_ANY);
#else
    local.sin_addr.s_addr = m_groupAddr;
#endif
    if (bind(s, reinterpret_cast<const sockaddr *>(&local), sizeof(local)) != 0) {
        setError("bind() to port " + std::to_string(port) + " failed");
        close();
        return false;
    }

    uint32_t ifaceAddr = htonl(INADDR_ANY);
    if (!interfaceAddress.empty() && !parseIPv4(interfaceAddress, ifaceAddr)) {
        m_lastError = "Invalid interface address " + interfaceAddress;
        close();
        return false;
    }

    ip_mreq mreq{};
    mreq.imr_multiaddr.s_addr = m_groupAddr;
    mreq.imr_interface.s_addr = ifaceAddr;
    if (setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<const char *>(&mreq), sizeof(mreq)) != 0) {
        setError("IP_ADD_MEMBERSHIP " + group + " failed");
        close();
        return false;
    }
    m_joined = true;
    m_joinedInterface = ifaceAddr;

    return true;
}

bool NodeStreamSocket::send(const void *header, size_t headerLength, const void *payload, size_t payloadLength) {
    if (m_socket == kInvalid)
        return false;

#ifdef _WIN32
    WSABUF buffers[2];
    buffers[0].buf = static_cast<CHAR *>(const_cast<void *>(header));
    buffers[0].len = static_cast<ULONG>(headerLength);
    buffers[1].buf = static_cast<CHAR *>(const_cast<void *>(payload));
    buffers[1].len = static_cast<ULONG>(payloadLength);
    DWORD sent = 0;
    if (WSASend(native(m_socket), buffers, payloadLength > 0 ? 2 : 1, &sent, 0, nullptr, nullptr) != 0) {
        setError("WSASend failed");
        return false;
    }
    return sent == headerLength + payloadLength;
#else
    iovec iov[2];
    iov[0].iov_base = const_cast<void *>(header);
    iov[0].iov_len = headerLength;
    iov[1].iov_base = const_cast<void *>(payload);
    iov[1].iov_len = payloadLength;
    msghdr msg{};
    msg.msg_iov = iov;
    msg.msg_iovlen = payloadLength > 0 ? 2 : 1;
    const ssize_t sent = sendmsg(native(m_socket), &msg, 0);
    if (sent < 0) {
        setError("sendmsg failed");
        return false;
    }
    return static_cast<size_t>(sent) == headerLength + payloadLength;
#endif
}

int NodeStreamSocket::receive(void *buffer, size_t length) {
    if (m_socket == kInvalid)
        return -1;

    const int received = static_cast<int>(recv(native(m_socket), static_cast<char *>(buffer), static_cast<int>(length), 0));
    if (received >= 0)
        return received;

    const int err = lastSocketError();
    if (isTimeoutError(err))
        return 0;
#ifdef _WIN32
    // A datagram larger than the buffer is just dropped.
    if (err == WSAEMSGSIZE)
        return 0;
#endif
    setError("recv failed");
    return -1;
}

std::string NodeStreamSocket::resolveIPv4(const std::string &host) {
    if (host.empty())
        return {};

    uint32_t direct = 0;
    if (parseIPv4(host, direct))
        return host;

#ifdef _WIN32
    WSADATA wsaData;
    const bool started = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
#endif

    std::string result;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    addrinfo *info = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &info) == 0 && info) {
        char text[INET_ADDRSTRLEN] = {};
        const auto *addr = reinterpret_cast<const sockaddr_in *>(info->ai_addr);
        if (inet_ntop(AF_INET, &addr->sin_addr, text, sizeof(text)))
            result = text;
        freeaddrinfo(info);
    }

#ifdef _WIN32
    if (started)
        WSACleanup();
#endif
    return result;
}

bool NodeStreamSocket::isLoopbackAddress(const std::string &dotted) {
    return dotted.rfind("127.", 0) == 0;
}

namespace {

int socketBufferSize(uintptr_t s, int option) {
    if (s == kInvalid)
        return -1;
    int value = 0;
#ifdef _WIN32
    int length = sizeof(value);
#else
    socklen_t length = sizeof(value);
#endif
    if (getsockopt(native(s), SOL_SOCKET, option, reinterpret_cast<char *>(&value), &length) != 0)
        return -1;
    return value;
}

int64_t steadyNowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

#ifdef _WIN32
// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, Windows 10 1803 and later.
constexpr DWORD kHighResolutionTimer = 0x00000002;
#endif

} // namespace

int NodeStreamSocket::sendBufferSize() const {
    return socketBufferSize(m_socket, SO_SNDBUF);
}

int NodeStreamSocket::receiveBufferSize() const {
    return socketBufferSize(m_socket, SO_RCVBUF);
}

uint64_t NodeStreamSocket::linkSpeedMbps(const std::string &interfaceAddress) {
#ifdef _WIN32
    uint32_t wanted = 0;
    const bool anyInterface = interfaceAddress.empty() || !parseIPv4(interfaceAddress, wanted);

    ULONG size = 16 * 1024;
    std::vector<uint8_t> buffer;
    ULONG result = ERROR_BUFFER_OVERFLOW;
    for (int attempt = 0; attempt < 3 && result == ERROR_BUFFER_OVERFLOW; ++attempt) {
        buffer.resize(size);
        result = GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                      nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()), &size);
    }
    if (result != NO_ERROR)
        return 0;

    uint64_t fastest = 0;
    for (auto *adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()); adapter; adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
            continue;
        // ULONG64_MAX means unknown.
        const uint64_t speed = adapter->TransmitLinkSpeed == ~0ULL ? 0 : adapter->TransmitLinkSpeed / 1'000'000;
        if (anyInterface) {
            fastest = std::max(fastest, speed);
            continue;
        }
        for (auto *unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
            const sockaddr *addr = unicast->Address.lpSockaddr;
            if (addr && addr->sa_family == AF_INET
                && reinterpret_cast<const sockaddr_in *>(addr)->sin_addr.s_addr == wanted)
                return speed;
        }
    }
    return anyInterface ? fastest : 0;
#else
    (void)interfaceAddress;
    return 0;
#endif
}

void NodeStreamSocket::waitUntilNs(int64_t steadyNs) {
    const int64_t remaining = steadyNs - steadyNowNs();
#ifdef _WIN32
    if (remaining > 600'000) {
        struct ThreadTimer {
            HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr, kHighResolutionTimer, TIMER_ALL_ACCESS);
            ~ThreadTimer() {
                if (handle)
                    CloseHandle(handle);
            }
        };
        thread_local ThreadTimer threadTimer;
        const HANDLE timer = threadTimer.handle;
        if (timer) {
            // Relative due time in 100 ns units, waking a little early to spin the rest.
            LARGE_INTEGER due;
            due.QuadPart = -((remaining - 400'000) / 100);
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
                WaitForSingleObject(timer, INFINITE);
        } else if (remaining > 2'000'000) {
            std::this_thread::sleep_for(std::chrono::nanoseconds(remaining - 1'000'000));
        }
    }
#else
    if (remaining > 200'000)
        std::this_thread::sleep_for(std::chrono::nanoseconds(remaining - 100'000));
#endif
    while (steadyNowNs() < steadyNs)
        std::this_thread::yield();
}
