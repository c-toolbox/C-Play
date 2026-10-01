/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODELOADERVERIFIER_H
#define NODELOADERVERIFIER_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariantList>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

#include <layers/baselayer.h>

class QTimer;

// One load failure reported by a cluster node (master-side storage + QML row).
struct NodeLoadFailure {
    int nodeId = -1;
    QString name;        // resolved from the cluster config on the master
    QString path;        // source path (or DirectShow capture key) that failed to load
    QString layerTitle;
    QString layerType;
    QString error;       // short human-readable reason
    qint64 ts = 0;       // master-side: first-report time (system_clock ms)
};

// Verifies that files the master synced to the cluster nodes actually loaded on each node.
// Mirrors NodeTelemetryManager and reuses the same SGCT DataTransfer channel, but is always-on
// and event-driven: a node sends a packet only when its set of failed loads changes (new,
// changed or recovered), so steady-state traffic is zero. Packets carry "kind":"loadfail" to
// discriminate them from telemetry packets on the shared channel.
//
// Constructed on the main thread in both the master and the node processes; SGCT's network
// callbacks run on network threads, so all shared state is mutex/atomic protected and QML
// notifications are delivered through queued invocations.
class NodeLoaderVerifier : public QObject {
    Q_OBJECT

public:
    // Must only be called from the main thread once QApplication/QCoreApplication exists
    // (Application's constructor on the master, main()'s node branch on nodes). Non-main-thread
    // entry points must check isReady() first so they can never trigger construction off the
    // main thread.
    static NodeLoaderVerifier &instance();

    // True once the singleton has been constructed on the main thread. Safe to call from any
    // thread; never triggers construction. SGCT's network callbacks and the render loop must
    // check this before calling instance().
    static bool isReady() noexcept;

    NodeLoaderVerifier(const NodeLoaderVerifier &) = delete;
    NodeLoaderVerifier(NodeLoaderVerifier &&) = delete;
    NodeLoaderVerifier &operator=(const NodeLoaderVerifier &) = delete;
    NodeLoaderVerifier &operator=(NodeLoaderVerifier &&) = delete;

    // Master-side decode of node load-failure packets (runs on SGCT's network thread).
    void handleNodeData(void *data, int length, int packageId, int clientIndex);

    // DataTransfer connection status changes (runs on SGCT's network thread). On the master a
    // disconnect drops that node's failures immediately; on a node a reconnect resets the dedupe
    // snapshot so still-failing paths are re-reported to the fresh session.
    void handleNodeStatus(bool connected, int clientIndex);

    // Rows for QML: one entry per (node, path) failure, nodes ordered by id.
    QVariantList loadFailuresList() const;

    // Node-side collection, called from postDraw(); self-throttled and event-driven - walks the
    // layers recursively (descending into sub-layers), refreshes each layer's load status via
    // collectLoadStatus(), and sends a packet only for failures that are new/changed or
    // recovered since the last send.
    void collectAndSend(const std::vector<std::shared_ptr<BaseLayer>> &layers);

Q_SIGNALS:
    void loadFailuresChanged();

private:
    explicit NodeLoaderVerifier(QObject *parent = nullptr);
    ~NodeLoaderVerifier() override = default;

    void sweepAndExpire(); // master-side timer tick: TTL for silent nodes

    const bool m_isMaster;

    mutable std::mutex m_mutex;

    // Master side.
    QHash<int, QVector<NodeLoadFailure>> m_failures; // keyed by cluster node id
    QHash<int, int> m_clientToNode;                  // DataTransfer client index -> node id
    QHash<int, qint64> m_nodeLastSeenMs;             // nodeId -> last packet time (TTL)
    QTimer *m_sweepTimer = nullptr;                  // master-side only

    // Node side: what has already been reported to the master, keyed by "path\0layerTitle".
    struct SentState {
        QString path;
        QString title;
        QString type;
        QString error;
    };
    QHash<QString, SentState> m_lastSent;
    bool m_nodeConnected = false;
    std::mutex m_nodeMutex; // guards m_lastSent + m_nodeConnected (network vs render thread)

    std::chrono::steady_clock::time_point m_lastCollect{};

    std::atomic_int m_packageCounter = 0;
};

#endif // NODELOADERVERIFIER_H