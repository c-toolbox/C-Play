/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODETELEMETRY_H
#define NODETELEMETRY_H

#include <QObject>
#include <QHash>
#include <QString>
#include <QVariantList>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

#include <layers/baselayer.h>

class QTimer;
class GpuLoadProbe;
class MpvObject;

// One row in the master's per-node health table (and the master's own row).
struct NodeTelemetry {
    int nodeId = -1;
    QString name;
    QString address;
    bool isMaster = false;
    bool online = false;
    double fps = 0.0;                       // min across layers (worst case)
    quint64 voDrops = 0;                    // VO drops since previous report (delta)
    quint64 decoderDrops = 0;               // decoder drops since previous report (delta)
    double syncOffsetMs = 0.0;              // |node position - master timePosition|
    double gpuLoad = -1.0;                  // -1 = unavailable (non-Windows / no counters)
    QString hwCurrent;
    QString videoCodec;
    bool cacheIdle = true;
    int layerCount = 0;
    double latencyMs = -1.0;                // master-side only: sync connection loopTime
    qint64 lastSeenMs = 0;                  // master-side only
};

// Collects per-node playback/GPU metrics on the cluster nodes and ships them to the
// master over SGCT's app-owned DataTransfer channel (JSON packets), where they are
// stored and exposed to QML. Constructed on the main thread in both the master and
// the node processes; the node-side decode/status callbacks run on SGCT's network
// thread, so all shared state is mutex/atomic protected and QML notifications are
// delivered through queued invocations.
//
// SGCT itself is never modified: the transport is the already-existing
// NetworkManager::transferData() DataTransfer connections, registered through the
// plain-function-pointer Engine::Callbacks::dataTransferDecode/dataTransferStatus.
class NodeTelemetryManager : public QObject {
    Q_OBJECT

public:
    // Must only be called from the main thread once QApplication exists (Application's
    // constructor does this). Non-main-thread entry points must check isReady() first so
    // they can never trigger construction off the main thread.
    static NodeTelemetryManager &instance();

    // True once the singleton has been constructed on the main thread (after QApplication
    // exists). Safe to call from any thread; never triggers construction. SGCT's network
    // callbacks and the render loop must check this before calling instance().
    static bool isReady() noexcept;

    NodeTelemetryManager(const NodeTelemetryManager &) = delete;
    NodeTelemetryManager(NodeTelemetryManager &&) = delete;
    NodeTelemetryManager &operator=(const NodeTelemetryManager &) = delete;
    NodeTelemetryManager &operator=(NodeTelemetryManager &&) = delete;

    // Master-side toggle: starts/stops the sweep timer and broadcasts the enable state
    // (plus reporting interval) to all nodes as a tiny JSON command packet.
    void setEnabled(bool enabled);
    bool enabled() const { return m_enabled; }

    // Master-side reporting interval (clamped to 100..5000 ms), propagated to the
    // nodes with the next enable-command broadcast.
    void setIntervalMs(int ms);
    int intervalMs() const { return m_intervalMs; }

    // Rows for QML: master row first, then nodes ordered by node id.
    QVariantList telemetryList() const;

    // Master-side decode of node telemetry packets (runs on SGCT's network thread).
    void handleNodeData(void *data, int length, int packageId, int clientIndex);

    // Master-side DataTransfer connection status changes (runs on SGCT's network thread).
    void handleNodeStatus(bool connected, int clientIndex);

    // Node-side collection, called from postDraw() every frame; self-throttled to the
    // configured interval and a no-op unless telemetry is enabled on this node.
    void collectAndSend(const std::vector<std::shared_ptr<BaseLayer>> &layers);

    // Node side: updated when the master broadcasts the enable command.
    void setNodeEnabledFromMaster(bool enabled, int intervalMs);

    // Master side: the main mpv instance, used to build the master's own row so the
    // operator can compare master vs nodes in one glance. Called from the GUI thread.
    void setMasterMpv(MpvObject *mpv);

Q_SIGNALS:
    void telemetryChanged();

private:
    explicit NodeTelemetryManager(QObject *parent = nullptr);
    ~NodeTelemetryManager() override = default;

    void broadcastEnableCommand();
    void sweepAndRefresh(); // master-side timer tick
    bool handleCommandPacket(const void *data, int length); // node side; true if consumed
    int syncConnectionIndexForNode(int nodeId) const;     // master side
    void refreshMasterRow(); // master-side: build the master's own row from its mpv

    // Reads fps/drops/decoder state from one live mpv handle into the aggregates,
    // differencing the cumulative drop counters against per-handle baselines.
    void aggregateFromHandle(void *handle, double &minFps, quint64 &voDrops,
                             quint64 &decDrops, QString &hw, QString &codec,
                             bool &cacheIdle, int &layerCount);

    struct DropBaseline {
        int64_t vo = 0;
        int64_t decoder = 0;
    };

    const bool m_isMaster;

    std::atomic_bool m_enabled = false;      // master: operator toggle; node: last cmd received
    std::atomic_int m_intervalMs = 1000;     // node-side reporting interval

    QTimer *m_sweepTimer = nullptr;          // master-side only, created lazily

    mutable std::mutex m_mutex;
    QHash<int, NodeTelemetry> m_nodes;       // keyed by cluster node id
    QHash<int, int> m_clientToNode;          // DataTransfer client index -> node id

    QHash<void *, DropBaseline> m_baselines; // per mpv_handle cumulative-counter baselines
    std::unique_ptr<GpuLoadProbe> m_gpuProbe; // node-side only

    MpvObject *m_masterMpv = nullptr;        // master-side only (master's own row)

    std::chrono::steady_clock::time_point m_lastCollect{};

    std::atomic_int m_packageCounter = 0;
};

#endif // NODETELEMETRY_H
