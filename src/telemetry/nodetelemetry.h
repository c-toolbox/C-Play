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
#include <QVariantMap>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>

#include <layers/baselayer.h>
#include <layers/mpvlayer.h>

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
    // GPU utilization in %, -1 = unavailable (non-Windows / no counters). Per engine, never
    // summed: gpuLoad is the busiest engine of any type (Task Manager style).
    double gpuLoad = -1.0;
    double gpuRender = -1.0;                // busiest 3D engine
    double gpuDecode = -1.0;                // busiest VideoDecode engine (NVDEC etc.)
    QVariantMap gpuEngines;                 // engine type -> busiest engine of that type
    QString hwCurrent;
    QString videoCodec;
    bool cacheIdle = true;
    int layerCount = 0;
    double latencyMs = -1.0;                // master-side only: sync connection loopTime
    qint64 lastSeenMs = 0;                  // master-side only
    // Per video layer: engine (mpv/vulkan), codec, hw decoding, size, fps, drops, file.
    // Optional in the packet (older nodes send none).
    QVariantList layers;
};

// Running min/max/average of one metric over a measurement.
struct MeasurementStat {
    int count = 0;
    double sum = 0.0;
    double min = 0.0;
    double max = 0.0;
    void add(double v) {
        if (count == 0 || v < min)
            min = v;
        if (count == 0 || v > max)
            max = v;
        sum += v;
        ++count;
    }
    double avg() const { return count > 0 ? sum / count : -1.0; }
};

// Per-node aggregate over one measurement (master side).
struct NodeMeasurement {
    int nodeId = -1;
    QString name;
    bool isMaster = false;
    int samples = 0;
    qint64 firstMs = 0;
    qint64 lastMs = 0;
    MeasurementStat fps;                    // only samples with a video playing (fps > 0)
    MeasurementStat gpuLoad;                // busiest engine of any type
    MeasurementStat gpuRender;
    MeasurementStat gpuDecode;
    QHash<QString, MeasurementStat> gpuEngines;
    MeasurementStat syncOffsetMs;
    MeasurementStat latencyMs;
    quint64 voDrops = 0;                    // totals over the measurement
    quint64 decoderDrops = 0;
    // Last seen decoder state, so the summary can show what was measured.
    QString hwCurrent;
    QString videoCodec;
    int layerCount = 0;
    QVariantList layers;
};

// One raw report recorded during a measurement, for CSV export.
struct MeasurementSample {
    qint64 timeMs = 0;
    int nodeId = -1;
    double fps = 0.0;
    double gpuLoad = -1.0;
    double gpuRender = -1.0;
    double gpuDecode = -1.0;
    QVariantMap gpuEngines;
    quint64 voDrops = 0;
    quint64 decoderDrops = 0;
    double syncOffsetMs = 0.0;
    double latencyMs = -1.0;
    int layerCount = 0;
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

    // Stops the sweep timer and blocks all further SGCT network traffic. Must be called on
    // the main thread before the render thread tears down SGCT (NetworkManager::destroy()).
    void shutdown();

    // Master side, GUI thread: measurement over time. While a measurement runs, every
    // report (nodes and the master's own row) is accumulated per node and kept as a raw
    // sample for CSV export. Starting clears the previous measurement; stopping keeps it
    // for review. Recording needs telemetry enabled; disabling telemetry stops it.
    void startMeasurement();
    void stopMeasurement();
    bool measuring() const;

    // { running, startMs, durationMs, sampleCount, truncated, rows: [ per-node maps ] }
    QVariantMap measurementSummary() const;

    // Downsampled time series for the timeline graph: the measurement is cut into at most
    // `buckets` equal time slots and each node's GPU loads are averaged per slot. Empty
    // slots are left out, so every series lists only the slots it has data in:
    // { startMs, durationMs, bucketMs, series: [ { nodeId, name, isMaster,
    //   t: [ms since start, slot centre], render: [%], decode: [%], busiest: [%] } ] }
    // A load is -1 in a slot where that counter was unavailable.
    QVariantMap measurementTimeline(int buckets) const;

    // Writes the recorded samples as CSV. Returns an empty string on success, otherwise
    // an error message.
    QString exportMeasurementCsv(const QString &path) const;

Q_SIGNALS:
    void telemetryChanged();
    void measurementChanged(); // started, stopped or cleared (GUI thread)

private:
    explicit NodeTelemetryManager(QObject *parent = nullptr);
    ~NodeTelemetryManager() override = default;

    void broadcastEnableCommand();
    void sweepAndRefresh(); // master-side timer tick
    bool handleCommandPacket(const void *data, int length); // node side; true if consumed
    int syncConnectionIndexForNode(int nodeId) const;     // master side
    void refreshMasterRow(); // master-side: build the master's own row from its mpv
    void recordSampleLocked(const NodeTelemetry &row); // m_mutex held; no-op unless measuring

    // Reads fps/drops/decoder state from one live mpv handle into the aggregates,
    // differencing the cumulative drop counters against per-handle baselines.
    // Returns the layer's details for the master's tooltips.
    QVariantMap aggregateFromHandle(void *handle, double &minFps, quint64 &voDrops,
                                    quint64 &decDrops, QString &hw, QString &codec,
                                    bool &cacheIdle, int &layerCount);

    // Same aggregates from a non-mpv video engine (Vulkan engine): fps is measured as
    // presented frames over the time since the previous sample, drops as a counter delta.
    // Node side, render thread.
    struct EngineBaseline {
        uint64_t presented = 0;
        uint64_t dropped = 0;
        std::chrono::steady_clock::time_point time{};
    };
    using EngineBaselines = QHash<const void *, EngineBaseline>;

    // prev: the baselines of the previous collection; the new one is stored in
    // m_engineBaselines.
    QVariantMap aggregateFromEngine(const void *layerKey, const MpvLayer::TelemetryEngineSample &sample,
                                    const EngineBaselines &prev,
                                    std::chrono::steady_clock::time_point now, double &minFps,
                                    quint64 &voDrops, QString &hw, QString &codec, int &layerCount);

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

    // Measurement over time (master side, guarded by m_mutex).
    bool m_measuring = false;
    qint64 m_measureStartMs = 0;
    qint64 m_measureStopMs = 0;
    QHash<int, NodeMeasurement> m_measurements; // keyed by node id (master row: -1)
    std::vector<MeasurementSample> m_measureSamples;
    bool m_measureTruncated = false;         // sample cap reached; stats keep accumulating

    QHash<void *, DropBaseline> m_baselines; // per mpv_handle cumulative-counter baselines
    // Per engine layer baselines (render thread only). Rebuilt every collection so that
    // destroyed layers drop out and a reused address starts over.
    EngineBaselines m_engineBaselines;
    std::unique_ptr<GpuLoadProbe> m_gpuProbe; // node-side only

    MpvObject *m_masterMpv = nullptr;        // master-side only (master's own row)

    std::chrono::steady_clock::time_point m_lastCollect{};

    std::atomic_int m_packageCounter = 0;
};

#endif // NODETELEMETRY_H
