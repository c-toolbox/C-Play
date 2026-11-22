/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodeshutdowncoordinator.h"

#include <nlohmann/json.hpp>
#include <sgct/sgct.h>

#include <QMetaObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <chrono>
#include <format>

using nlohmann::json;

namespace {

constexpr const char *kKindQuit = "quit";
constexpr const char *kKindNodeState = "nodestate";

// Master: how often the shutdown progress is checked, and the quit command repeated.
constexpr int kTickMs = 100;
constexpr qint64 kQuitResendMs = 1000;
// Master: the longest wait for the nodes, so quitting is never blocked for good.
constexpr qint64 kShutdownTimeoutMs = 15000;
// Master: after every node reported "closed", the longest wait for their connections to drop.
constexpr qint64 kConnectionsDropTimeoutMs = 2000;
// Master: closes this long after the last node closed.
constexpr int kFinishGraceMs = 500;

qint64 nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

QString nodeNameForId(int nodeId) {
    const auto &cm = sgct::ClusterManager::instance();
    if (nodeId >= 0 && nodeId < cm.numberOfNodes()) {
        const QString address = QString::fromStdString(cm.node(nodeId).address());
        if (!address.isEmpty())
            return address;
    }
    return QStringLiteral("node %1").arg(nodeId);
}

// Master: the cluster node behind a DataTransfer connection, matched by its port (every node
// has its own DataTransfer port on the master). -1 if unknown.
int nodeIdForClient(int clientIndex) {
    const auto &nm = sgct::NetworkManager::instance();
    int port = -1;
    for (int i = 0; i < nm.connectionsCount(); ++i) {
        const sgct::Network &connection = nm.connection(i);
        if (connection.type() == sgct::Network::ConnectionType::DataTransfer && connection.id() == clientIndex) {
            port = connection.port();
            break;
        }
    }
    if (port <= 0)
        return -1;
    const auto &cm = sgct::ClusterManager::instance();
    for (int n = 0; n < cm.numberOfNodes(); ++n) {
        if (cm.node(n).dataTransferPort() == port)
            return n;
    }
    return -1;
}

QList<int> connectedDataTransferClients() {
    QList<int> ids;
    const auto &nm = sgct::NetworkManager::instance();
    for (int i = 0; i < nm.connectionsCount(); ++i) {
        const sgct::Network &connection = nm.connection(i);
        if (connection.type() == sgct::Network::ConnectionType::DataTransfer && connection.isConnected())
            ids.append(connection.id());
    }
    return ids;
}

} // namespace

// See NodeLoaderVerifier: SGCT's network callbacks may fire before this QObject exists and must
// never construct it off the main thread.
static std::atomic<bool> s_shutdownCoordinatorReady{false};

NodeShutdownCoordinator &NodeShutdownCoordinator::instance() {
    static NodeShutdownCoordinator s_instance;
    s_shutdownCoordinatorReady.store(true, std::memory_order_release);
    return s_instance;
}

bool NodeShutdownCoordinator::isReady() noexcept {
    return s_shutdownCoordinatorReady.load(std::memory_order_acquire);
}

NodeShutdownCoordinator::NodeShutdownCoordinator(QObject *parent)
    : QObject(parent)
    , m_isMaster(sgct::Engine::instance().isMaster()) {
    if (m_isMaster) {
        m_timer = new QTimer(this);
        m_timer->setInterval(kTickMs);
        connect(m_timer, &QTimer::timeout, this, &NodeShutdownCoordinator::tick);
    }
}

// ---------------------------------------------------------------------------
// Network callbacks (network threads)
// ---------------------------------------------------------------------------

void NodeShutdownCoordinator::handleData(void *data, int length, int clientIndex) {
    if (!data || length <= 0)
        return;

    std::string kind;
    int nodeId = -1;
    std::string state;
    try {
        const json root = json::parse(static_cast<const char *>(data), static_cast<const char *>(data) + length);
        if (!root.is_object() || root.value("v", 0) != 1 || !root.contains("kind") || !root["kind"].is_string())
            return;
        kind = root["kind"].get<std::string>();
        if (m_isMaster && kind == kKindNodeState) {
            if (!root.contains("node") || !root["node"].is_number_integer())
                return;
            nodeId = root["node"].get<int>();
            state = root.value("state", std::string());
        }
    } catch (const json::exception &) {
        return; // not JSON (or malformed) - another consumer's packet
    }

    if (m_isMaster) {
        if (kind != kKindNodeState || nodeId < 0)
            return;
        QMetaObject::invokeMethod(
            this, [this, clientIndex, nodeId, state]() { onNodeState(clientIndex, nodeId, state); },
            Qt::QueuedConnection);
        return;
    }

    if (kind == kKindQuit && !m_quitRequested.exchange(true, std::memory_order_acq_rel))
        sgct::Log::Info("Quit requested by the master over the data transfer connection");
}

void NodeShutdownCoordinator::handleStatus(bool connected, int clientIndex) {
    if (!m_isMaster)
        return;
    QMetaObject::invokeMethod(
        this,
        [this, connected, clientIndex]() {
            if (connected)
                onConnected(clientIndex);
            else
                onDisconnected(clientIndex);
        },
        Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------
// Master side (GUI thread)
// ---------------------------------------------------------------------------

bool NodeShutdownCoordinator::beginShutdown() {
    if (!m_isMaster || m_networkGone)
        return false;
    if (m_active)
        return true;

    const QList<int> clients = connectedDataTransferClients();
    if (clients.isEmpty())
        return false;

    m_rows.clear();
    for (int client : clients) {
        Row row;
        row.nodeId = nodeIdForClient(client);
        m_rows.insert(client, row);
    }
    m_active = true;
    m_timedOut = false;
    m_finishing = false;
    m_startMs = nowMs();
    m_allClosedMs = 0;
    m_closedNodes.clear(); // the shutdown notice replaces the closed-node notice
    Q_EMIT closedNodesChanged();

    sgct::Log::Info(std::format("Closing {} node(s) over the data transfer connection", clients.size()));
    sendQuit();
    m_timer->start();
    Q_EMIT stateChanged();
    return true;
}

void NodeShutdownCoordinator::shutdown() {
    m_networkGone = true;
    if (m_timer)
        m_timer->stop();
}

void NodeShutdownCoordinator::sendQuit() {
    if (m_networkGone)
        return;
    json cmd;
    cmd["v"] = 1;
    cmd["kind"] = kKindQuit;
    const std::string s = cmd.dump();
    try {
        sgct::NetworkManager::instance().transferData(s.data(), static_cast<int>(s.size()),
                                                      m_packageCounter.fetch_add(1));
    } catch (...) {
        // A connection that drops while sending: the node is closing anyway.
    }
    m_lastQuitSentMs = nowMs();
}

void NodeShutdownCoordinator::onNodeState(int clientIndex, int nodeId, const std::string &state) {
    const bool closed = state == "closed";
    if (!closed && state != "closing")
        return;

    if (!m_active) {
        // Not asked to close: a node closed on its own (window closed on the node, ...).
        if (closed)
            noteUnexpectedClose(nodeId, QStringLiteral("closed"));
        return;
    }

    auto it = m_rows.find(clientIndex);
    if (it == m_rows.end())
        return; // connected after the shutdown began - not tracked
    it->nodeId = nodeId;
    const State next = closed ? State::Closed : State::Closing;
    if (it->state == State::Closed || it->state == next)
        return;
    it->state = next;
    if (closed)
        sgct::Log::Info(std::format("Node {} closed", nodeNameForId(nodeId).toStdString()));
    Q_EMIT stateChanged();
}

void NodeShutdownCoordinator::onDisconnected(int clientIndex) {
    if (m_networkGone)
        return;
    if (m_active) {
        auto it = m_rows.find(clientIndex);
        if (it == m_rows.end() || it->state == State::Closed)
            return;
        it->state = State::Closed;
        sgct::Log::Info(std::format("Node {} disconnected", nodeNameForId(it->nodeId).toStdString()));
        Q_EMIT stateChanged();
        return;
    }
    const int nodeId = nodeIdForClient(clientIndex);
    if (nodeId >= 0)
        noteUnexpectedClose(nodeId, QStringLiteral("lost"));
}

void NodeShutdownCoordinator::onConnected(int clientIndex) {
    if (m_active || m_networkGone)
        return;
    // A node that came back is no longer closed.
    const int nodeId = nodeIdForClient(clientIndex);
    const auto before = m_closedNodes.size();
    m_closedNodes.erase(std::remove_if(m_closedNodes.begin(), m_closedNodes.end(),
                                       [nodeId](const QVariant &v) {
                                           return v.toMap().value(QStringLiteral("nodeId")).toInt() == nodeId;
                                       }),
                        m_closedNodes.end());
    if (m_closedNodes.size() != before)
        Q_EMIT closedNodesChanged();
}

void NodeShutdownCoordinator::noteUnexpectedClose(int nodeId, const QString &reason) {
    for (const QVariant &v : std::as_const(m_closedNodes)) {
        if (v.toMap().value(QStringLiteral("nodeId")).toInt() == nodeId)
            return; // "closed" report, then its connection dropped - already listed
    }
    const QString name = nodeNameForId(nodeId);
    sgct::Log::Warning(reason == QStringLiteral("closed")
                           ? std::format("Node {} has shut down", name.toStdString())
                           : std::format("Lost the data transfer connection to node {}", name.toStdString()));
    QVariantMap row;
    row.insert(QStringLiteral("nodeId"), nodeId);
    row.insert(QStringLiteral("name"), name);
    row.insert(QStringLiteral("reason"), reason);
    m_closedNodes.append(row);
    Q_EMIT closedNodesChanged();
}

void NodeShutdownCoordinator::clearClosedNodes() {
    if (m_closedNodes.isEmpty())
        return;
    m_closedNodes.clear();
    Q_EMIT closedNodesChanged();
}

bool NodeShutdownCoordinator::allClosed() const {
    return std::all_of(m_rows.cbegin(), m_rows.cend(), [](const Row &r) { return r.state == State::Closed; });
}

int NodeShutdownCoordinator::closedCount() const {
    return static_cast<int>(
        std::count_if(m_rows.cbegin(), m_rows.cend(), [](const Row &r) { return r.state == State::Closed; }));
}

void NodeShutdownCoordinator::tick() {
    if (!m_active || m_finishing || m_networkGone)
        return;

    const qint64 now = nowMs();
    bool done = false;
    if (allClosed()) {
        if (m_allClosedMs == 0)
            m_allClosedMs = now;
        // Nodes without a DataTransfer connection only follow the sync terminate flag; their
        // sync connections dropping is all the master can see of them.
        done = sgct::NetworkManager::instance().activeConnectionsCount() == 0 ||
               now - m_allClosedMs > kConnectionsDropTimeoutMs;
    } else if (now - m_startMs > kShutdownTimeoutMs) {
        QStringList waiting;
        for (const Row &r : std::as_const(m_rows))
            if (r.state != State::Closed)
                waiting.append(nodeNameForId(r.nodeId));
        sgct::Log::Warning(std::format("No closed report from {} after {} s, closing anyway",
                                       waiting.join(QStringLiteral(", ")).toStdString(), kShutdownTimeoutMs / 1000));
        m_timedOut = true;
        done = true;
    } else if (now - m_lastQuitSentMs >= kQuitResendMs) {
        sendQuit(); // in case a node missed it
    }

    if (!done)
        return;
    m_finishing = true;
    m_timer->stop();
    Q_EMIT stateChanged();
    QTimer::singleShot(kFinishGraceMs, this, [this]() { Q_EMIT finished(); });
}

QVariantList NodeShutdownCoordinator::nodes() const {
    QList<Row> rows = m_rows.values();
    std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.nodeId < b.nodeId; });
    QVariantList list;
    for (const Row &r : std::as_const(rows)) {
        QVariantMap m;
        m.insert(QStringLiteral("nodeId"), r.nodeId);
        m.insert(QStringLiteral("name"), nodeNameForId(r.nodeId));
        m.insert(QStringLiteral("state"), r.state == State::Closed    ? QStringLiteral("closed")
                                          : r.state == State::Closing ? QStringLiteral("closing")
                                                                      : QStringLiteral("running"));
        list.append(m);
    }
    return list;
}

// ---------------------------------------------------------------------------
// Node side (render thread)
// ---------------------------------------------------------------------------

void NodeShutdownCoordinator::sendNodeState(const char *state) {
    json root;
    root["v"] = 1;
    root["kind"] = kKindNodeState;
    root["node"] = sgct::ClusterManager::instance().thisNodeId();
    root["state"] = state;
    const std::string s = root.dump();
    try {
        // Only sent while connected; the master counts a dropped connection as closed too.
        sgct::NetworkManager::instance().transferData(s.data(), static_cast<int>(s.size()),
                                                      m_packageCounter.fetch_add(1));
    } catch (...) {
    }
}

void NodeShutdownCoordinator::reportClosing() {
    if (m_isMaster || m_closingSent)
        return;
    m_closingSent = true;
    sendNodeState("closing");
}

void NodeShutdownCoordinator::reportClosed() {
    if (m_isMaster || m_closedSent)
        return;
    m_closedSent = true;
    sendNodeState("closed");
}
