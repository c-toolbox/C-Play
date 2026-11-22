/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODESHUTDOWNCOORDINATOR_H
#define NODESHUTDOWNCOORDINATOR_H

#include <QObject>
#include <QMap>
#include <QVariantList>

#include <atomic>
#include <string>

class QTimer;

// Ordered cluster shutdown over SGCT's DataTransfer channel (shared with NodeTelemetryManager,
// NodeLoaderVerifier and NodeCaptureInventory; packets are discriminated by "kind").
//
// Master -> nodes: {"kind":"quit"} asks every node to close (on top of the terminateNodes
// flag in the sync frame, which still covers nodes without a DataTransfer connection).
// Nodes -> master: {"kind":"nodestate","state":"closing"|"closed"} - "closing" when the node
// starts to shut down, "closed" from cleanup() once its layers are released, right before
// SGCT drops its connections.
//
// On the master, beginShutdown() tracks every node connected over DataTransfer; a node counts
// as closed on its "closed" report or when its DataTransfer connection drops (older node
// builds only do the latter). finished() is emitted slightly after the last node closed (or
// after a timeout), so the master closes after its nodes. A node that closes while no
// shutdown is in progress is listed in closedNodes for the main view.
//
// Master-side state lives on the GUI thread only: the SGCT network callbacks parse packets
// and queue the result. Constructed on the main thread in both master and node processes.
class NodeShutdownCoordinator : public QObject {
    Q_OBJECT

    // Master: a cluster shutdown is in progress.
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    // Master: the nodes of the current shutdown, ordered by node id:
    // [{nodeId, name, state: "running"|"closing"|"closed"}].
    Q_PROPERTY(QVariantList nodes READ nodes NOTIFY stateChanged)
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY stateChanged)
    Q_PROPERTY(int closedCount READ closedCount NOTIFY stateChanged)
    Q_PROPERTY(bool timedOut READ timedOut NOTIFY stateChanged)
    // Master: nodes that closed while no shutdown was in progress:
    // [{nodeId, name, reason: "closed"|"lost"}].
    Q_PROPERTY(QVariantList closedNodes READ closedNodes NOTIFY closedNodesChanged)

public:
    // Main thread only, once QApplication/QCoreApplication exists. Other threads must check
    // isReady() first so they never trigger construction.
    static NodeShutdownCoordinator &instance();
    static bool isReady() noexcept;

    NodeShutdownCoordinator(const NodeShutdownCoordinator &) = delete;
    NodeShutdownCoordinator(NodeShutdownCoordinator &&) = delete;
    NodeShutdownCoordinator &operator=(const NodeShutdownCoordinator &) = delete;
    NodeShutdownCoordinator &operator=(NodeShutdownCoordinator &&) = delete;

    // SGCT DataTransfer callbacks (network threads).
    void handleData(void *data, int length, int clientIndex);
    void handleStatus(bool connected, int clientIndex);

    // Master, GUI thread: sends the quit command and starts tracking the nodes. Returns false
    // when no node is connected over DataTransfer (nothing to track - the caller falls back
    // to waiting for the sync connections); true when a shutdown runs (also if already running).
    Q_INVOKABLE bool beginShutdown();
    Q_INVOKABLE void clearClosedNodes();

    // Master, GUI thread: stops all SGCT traffic; called before SGCT is torn down.
    void shutdown();

    bool active() const { return m_active; }
    QVariantList nodes() const;
    int nodeCount() const { return static_cast<int>(m_rows.size()); }
    int closedCount() const;
    bool timedOut() const { return m_timedOut; }
    QVariantList closedNodes() const { return m_closedNodes; }

    // Node: the master sent the quit command (any thread).
    bool quitRequested() const noexcept { return m_quitRequested.load(std::memory_order_acquire); }
    // Node, render thread: report to the master. Each is sent at most once.
    void reportClosing();
    void reportClosed();

Q_SIGNALS:
    void stateChanged();
    void closedNodesChanged();
    // Master: all nodes have closed (or the wait timed out); the master can quit now.
    void finished();

private:
    explicit NodeShutdownCoordinator(QObject *parent = nullptr);
    ~NodeShutdownCoordinator() override = default;

    enum class State { Running, Closing, Closed };
    struct Row {
        int nodeId = -1;
        State state = State::Running;
    };

    // Master, GUI thread.
    void onNodeState(int clientIndex, int nodeId, const std::string &state);
    void onDisconnected(int clientIndex);
    void onConnected(int clientIndex);
    void tick();
    void sendQuit();
    void noteUnexpectedClose(int nodeId, const QString &reason);
    bool allClosed() const;

    // Node side.
    void sendNodeState(const char *state);

    const bool m_isMaster;

    // Master side (GUI thread).
    bool m_active = false;
    bool m_timedOut = false;
    bool m_finishing = false;
    bool m_networkGone = false;
    qint64 m_startMs = 0;
    qint64 m_allClosedMs = 0;
    qint64 m_lastQuitSentMs = 0;
    QMap<int, Row> m_rows;            // DataTransfer client index -> node
    QVariantList m_closedNodes;
    QTimer *m_timer = nullptr;

    // Node side.
    std::atomic_bool m_quitRequested = false;
    bool m_closingSent = false;
    bool m_closedSent = false;

    std::atomic_int m_packageCounter = 0;
};

#endif // NODESHUTDOWNCOORDINATOR_H
