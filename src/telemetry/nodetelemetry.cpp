/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodetelemetry.h"
#include "application.h"
#include "layers/mpvlayer.h"
#include "mpvobject.h"
#include "utils/gpuload.h"

#include <mpv/client.h>
#include <nlohmann/json.hpp>
#include <sgct/sgct.h>

#include <QMetaObject>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>

using nlohmann::json;

namespace {

// A row is considered offline when no packet arrived for this long.
constexpr qint64 kStaleTimeoutMs = 3000;

// Default reporting interval when the master has not sent one.
constexpr int kDefaultIntervalMs = 1000;

qint64 nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Parse the leading number out of a perf-info text field, e.g.
// "59.94 fps (u50 v50 a50)" -> 59.94. Returns 0.0 when unparseable.
double leadingDouble(const QString& text) {
    const QByteArray utf8 = text.toUtf8();
    const char* begin = utf8.constData();
    char* end = nullptr;
    const double value = std::strtod(begin, &end);
    return (end == begin || value < 0.0) ? 0.0 : value;
}

// Read mpv's perf-info array once and return a map of entry name -> text. mpv resets
// its internal perf-measurement state on every query, so the array must be consumed
// in a single pass (mirrors MpvObject::updatePerformanceStats()).
QHash<QString, QString> readPerfInfo(mpv_handle* handle) {
    QHash<QString, QString> result;
    mpv_node node = {};
    if (mpv_get_property(handle, "perf-info", MPV_FORMAT_NODE, &node) >= 0 &&
        node.format == MPV_FORMAT_NODE_ARRAY && node.u.list) {
        for (int i = 0; i < node.u.list->num; ++i) {
            const mpv_node* entry = &node.u.list->values[i];
            if (!entry || entry->format != MPV_FORMAT_NODE_MAP || !entry->u.list)
                continue;
            QString name, text;
            for (int j = 0; j < entry->u.list->num; ++j) {
                const char* key = entry->u.list->keys[j];
                if (!key)
                    continue;
                const mpv_node& value = entry->u.list->values[j];
                if (strcmp(key, "name") == 0 && value.format == MPV_FORMAT_STRING)
                    name = QString::fromUtf8(value.u.string);
                else if (strcmp(key, "text") == 0 && value.format == MPV_FORMAT_STRING)
                    text = QString::fromUtf8(value.u.string);
            }
            if (!name.isEmpty())
                result.insert(name, text);
        }
    }
    mpv_free_node_contents(&node);
    return result;
}

QString readPropertyString(mpv_handle* handle, const char* name) {
    char* str = nullptr;
    QString result;
    if (mpv_get_property(handle, name, MPV_FORMAT_STRING, &str) >= 0 && str) {
        result = QString::fromUtf8(str);
        mpv_free(str);
    }
    return result;
}

int64_t readPropertyInt64(mpv_handle* handle, const char* name) {
    int64_t value = 0;
    if (mpv_get_property(handle, name, MPV_FORMAT_INT64, &value) < 0)
        return 0;
    return value;
}

// True when the demuxer cache reports itself idle (no active buffering).
bool readCacheIdle(mpv_handle* handle) {
    char* str = nullptr;
    if (mpv_get_property(handle, "demuxer-cache-state", MPV_FORMAT_STRING, &str) < 0 || !str)
        return true;
    const std::string jsonStr(str);
    mpv_free(str);
    try {
        const json root = json::parse(jsonStr);
        if (!root.is_object())
            return true;
        return root.contains("idle") && root["idle"].is_boolean() ? root["idle"].get<bool>()
                                                                  : true;
    } catch (const json::exception&) {
        return true;
    }
}

} // namespace

// Set to true inside instance() once the singleton has been constructed. The SGCT
// dataTransfer callbacks run on network threads and must not construct this QObject
// singleton there (a QObject created before QApplication exists - or from a non-main thread
// winning the construction race - corrupts Qt's main-thread bookkeeping and crashes at
// launch); until this flag is set they drop events instead (harmless: the master
// re-broadcasts its enable command every sweep tick, so nodes pick up late).
static std::atomic<bool> s_telemetryManagerReady{false};

NodeTelemetryManager& NodeTelemetryManager::instance() {
    // C++11 guarantees this static is constructed exactly once, even under concurrent calls -
    // but it would be constructed *on whichever thread calls first*. Only the main thread may
    // trigger construction (Application's constructor does, after QApplication exists); every
    // non-main-thread entry point checks isReady() before calling instance().
    static NodeTelemetryManager s_instance;
    s_telemetryManagerReady.store(true, std::memory_order_release);
    return s_instance;
}

bool NodeTelemetryManager::isReady() noexcept {
    return s_telemetryManagerReady.load(std::memory_order_acquire);
}

NodeTelemetryManager::NodeTelemetryManager(QObject* parent)
    : QObject(parent)
    , m_isMaster(sgct::Engine::instance().isMaster()) {
}

// ---------------------------------------------------------------------------
// Master side
// ---------------------------------------------------------------------------

void NodeTelemetryManager::setEnabled(bool enabled) {
    if (m_enabled.load() == enabled)
        return;
    m_enabled.store(enabled);

    if (m_isMaster) {
        if (enabled && !m_sweepTimer) {
            m_sweepTimer = new QTimer(this);
            m_sweepTimer->setInterval(1000);
            connect(m_sweepTimer, &QTimer::timeout, this, &NodeTelemetryManager::sweepAndRefresh);
        }
        if (m_sweepTimer)
            m_sweepTimer->start();
        if (!enabled) {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_nodes.clear();
            m_clientToNode.clear();
        }
        broadcastEnableCommand();
        Q_EMIT telemetryChanged();
    }
}

void NodeTelemetryManager::setIntervalMs(int ms) {
    const int clamped = std::clamp(ms, 100, 5000);
    if (m_intervalMs.load() == clamped)
        return;
    m_intervalMs.store(clamped);
    if (m_isMaster && m_enabled.load())
        broadcastEnableCommand(); // propagate the new interval to the nodes
}

void NodeTelemetryManager::setMasterMpv(MpvObject* mpv) {
    m_masterMpv = mpv;
}

void NodeTelemetryManager::aggregateFromHandle(void* h, double& minFps, quint64& voDrops,
                                               quint64& decDrops, QString& hw, QString& codec,
                                               bool& cacheIdle, int& layerCount) {
    auto* handle = static_cast<mpv_handle*>(h);
    if (!handle)
        return;

    // perf-info must be queried exactly once per interval: mpv resets its
    // measurement window on each query.
    const QHash<QString, QString> perfInfo = readPerfInfo(handle);
    auto fpsIt = perfInfo.constFind(QStringLiteral("estimated-vf-fps"));
    if (fpsIt == perfInfo.constEnd())
        fpsIt = perfInfo.constFind(QStringLiteral("vsync-fps"));
    if (fpsIt != perfInfo.constEnd()) {
        const double fps = leadingDouble(fpsIt.value());
        if (fps > 0.0)
            minFps = std::min(minFps, fps);
    }

    const int64_t vo = readPropertyInt64(handle, "frame-drop-count");
    const int64_t dec = readPropertyInt64(handle, "decoder-frame-drop-count");
    DropBaseline& base = m_baselines[handle];
    if (vo < base.vo)
        base.vo = vo; // counter reset (seek/file load)
    if (dec < base.decoder)
        base.decoder = dec;
    voDrops += static_cast<quint64>(vo - base.vo);
    decDrops += static_cast<quint64>(dec - base.decoder);
    base.vo = vo;
    base.decoder = dec;

    if (hw.isEmpty())
        hw = readPropertyString(handle, "hw-current");
    if (codec.isEmpty())
        codec = readPropertyString(handle, "video-codec");

    cacheIdle = cacheIdle && readCacheIdle(handle);

    layerCount++;
}

void NodeTelemetryManager::refreshMasterRow() {
    // Master-side, GUI thread. Builds the master's own row from its main mpv instance
    // so the operator can compare master vs nodes in one glance.
    if (!m_masterMpv)
        return;
    mpv_handle* handle = m_masterMpv->telemetryHandle();
    if (!handle)
        return;

    NodeTelemetry row;
    row.nodeId = -1; // reserved key for the master row; real node ids are >= 0
    row.isMaster = true;
    row.online = true;
    row.lastSeenMs = nowMs();

    const auto& cm = sgct::ClusterManager::instance();
    row.address = QString::fromStdString(cm.thisNode().address());
    row.name = row.address.isEmpty() ? QStringLiteral("Master")
                                     : QStringLiteral("Master (%1)").arg(row.address);

    double minFps = std::numeric_limits<double>::max();
    bool cacheIdle = true;

    // Only report metrics while the master actually has media loaded.
    const QString path = readPropertyString(handle, "path");
    if (!path.isEmpty())
        aggregateFromHandle(handle, minFps, row.voDrops, row.decoderDrops, row.hwCurrent,
                            row.videoCodec, cacheIdle, row.layerCount);

    row.fps = (minFps == std::numeric_limits<double>::max()) ? 0.0 : minFps;
    row.cacheIdle = cacheIdle;
    row.syncOffsetMs = 0.0; // the master defines the reference clock
    row.latencyMs = 0.0;

    if (!m_gpuProbe)
        m_gpuProbe = std::make_unique<GpuLoadProbe>();
    row.gpuLoad = m_gpuProbe->sample();

    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_nodes.insert(row.nodeId, row);
    }
}

void NodeTelemetryManager::broadcastEnableCommand() {
    json cmd;
    cmd["v"] = 1;
    cmd["cmd"] = "enable";
    cmd["enable"] = m_enabled.load();
    cmd["interval"] = m_intervalMs.load();
    const std::string s = cmd.dump();

    auto& nm = sgct::NetworkManager::instance();
    nm.transferData(s.data(), static_cast<int>(s.size()), m_packageCounter.fetch_add(1));
}

bool NodeTelemetryManager::handleCommandPacket(const void* data, int length) {
    // Runs on SGCT's network receive thread on the node process.
    try {
        const json root = json::parse(static_cast<const char*>(data),
                                      static_cast<const char*>(data) + length);
        if (!root.is_object() || !root.contains("cmd") ||
            !root["cmd"].is_string() || root["cmd"].get<std::string>() != "enable") {
            return false;
        }
        const bool enabled = root.value("enable", false);
        const int interval = root.contains("interval") && root["interval"].is_number_integer()
                                 ? root["interval"].get<int>()
                                 : kDefaultIntervalMs;
        setNodeEnabledFromMaster(enabled, interval);
        return true;
    } catch (const json::exception&) {
        return false;
    }
}

void NodeTelemetryManager::handleNodeData(void* data, int length, int /*packageId*/, int clientIndex) {
    if (!data || length <= 0)
        return;

    if (!m_isMaster) {
        // Node process: the only packets we receive are the master's enable/disable commands.
        handleCommandPacket(data, length);
        return;
    }

    json root;
    try {
        root = json::parse(static_cast<const char*>(data), static_cast<const char*>(data) + length);
        if (!root.is_object() || !root.contains("v") || !root["v"].is_number_integer() ||
            root["v"].get<int>() != 1) {
            return;
        }

        // Node-side telemetry report.
        if (!root.contains("node") || !root["node"].is_number_integer())
            return;
        const int nodeId = root["node"].get<int>();
        if (nodeId < 0)
            return;

        NodeTelemetry row;
        row.nodeId = nodeId;
        row.online = true;
        row.lastSeenMs = nowMs();

        // Node name/address from the cluster config (best effort).
        const auto& cm = sgct::ClusterManager::instance();
        if (nodeId < cm.numberOfNodes())
            row.address = QString::fromStdString(cm.node(nodeId).address());
        row.name = row.address.isEmpty() ? QStringLiteral("node %1").arg(nodeId) : row.address;

        row.fps = root.value("fps", 0.0);
        row.voDrops = root.value("voDrops", Q_UINT64_C(0));
        row.decoderDrops = root.value("decDrops", Q_UINT64_C(0));
        row.syncOffsetMs = root.value("offsetMs", 0.0);
        row.gpuLoad = root.value("gpu", -1.0);
        row.layerCount = root.value("layers", 0);
        row.hwCurrent = QString::fromStdString(root.value("hw", std::string()));
        row.videoCodec = QString::fromStdString(root.value("codec", std::string()));
        row.cacheIdle = root.value("cacheIdle", true);

        // Network latency: read the sync connection loopTime for this node (master-side,
        // no node work).
        row.latencyMs = -1.0;
        auto& nm = sgct::NetworkManager::instance();
        const int syncIndex = syncConnectionIndexForNode(nodeId);
        if (syncIndex >= 0 && syncIndex < nm.syncConnectionsCount()) {
            row.latencyMs = nm.syncConnection(syncIndex).loopTime() * 1000.0;
        }

        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_nodes.insert(nodeId, row);
            m_clientToNode.insert(clientIndex, nodeId);
        }
    } catch (const json::exception&) {
        return; // not our JSON (or malformed) - ignore
    }

    // Notify the GUI thread (this runs on SGCT's network receive thread).
    QMetaObject::invokeMethod(this, "telemetryChanged", Qt::QueuedConnection);
}

void NodeTelemetryManager::handleNodeStatus(bool connected, int clientIndex) {
    if (!m_isMaster)
        return;

    int nodeId = -1;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_clientToNode.constFind(clientIndex);
        if (it != m_clientToNode.constEnd())
            nodeId = it.value();
    }
    if (nodeId < 0)
        return; // unknown mapping; the sweep timer will age the row out

    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_nodes.find(nodeId);
        if (it != m_nodes.end()) {
            it->online = connected;
            if (!connected)
                it->lastSeenMs = 0;
        }
    }
    QMetaObject::invokeMethod(this, "telemetryChanged", Qt::QueuedConnection);
}

int NodeTelemetryManager::syncConnectionIndexForNode(int nodeId) const {
    // SGCT adds one sync connection per cluster node (skipping the master itself), in
    // cluster-config order. Reproduce that ordering to map a node id to its sync connection.
    const auto& cm = sgct::ClusterManager::instance();
    const int masterNodeId = cm.thisNodeId(); // on the master this is its own index
    int syncIndex = -1;
    int counter = 0;
    for (int i = 0; i < cm.numberOfNodes(); ++i) {
        if (i == masterNodeId)
            continue;
        if (i == nodeId) {
            syncIndex = counter;
            break;
        }
        ++counter;
    }
    return syncIndex;
}

void NodeTelemetryManager::sweepAndRefresh() {
    // Keep re-broadcasting the enable command at the reporting cadence so nodes that
    // connect (or restart) later pick up the current state without extra handshaking.
    broadcastEnableCommand();

    refreshMasterRow();

    const qint64 cutoff = nowMs() - kStaleTimeoutMs;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
            if (it->online && it->lastSeenMs < cutoff)
                it->online = false;
        }
    }

    // Refresh at the reporting cadence regardless, so latency values stay live.
    Q_EMIT telemetryChanged();
}

QVariantList NodeTelemetryManager::telemetryList() const {
    QVariantList list;
    const std::lock_guard<std::mutex> lock(m_mutex);

    QList<int> ids;
    for (auto it = m_nodes.constBegin(); it != m_nodes.constEnd(); ++it)
        ids.append(it.key());
    std::sort(ids.begin(), ids.end());

    for (int id : ids) {
        const NodeTelemetry& r = m_nodes.value(id);
        QVariantMap m;
        m.insert(QStringLiteral("nodeId"), r.nodeId);
        m.insert(QStringLiteral("name"), r.name);
        m.insert(QStringLiteral("address"), r.address);
        m.insert(QStringLiteral("isMaster"), r.isMaster);
        m.insert(QStringLiteral("online"), r.online);
        m.insert(QStringLiteral("fps"), r.fps);
        m.insert(QStringLiteral("voDrops"), static_cast<qulonglong>(r.voDrops));
        m.insert(QStringLiteral("decoderDrops"), static_cast<qulonglong>(r.decoderDrops));
        m.insert(QStringLiteral("syncOffsetMs"), r.syncOffsetMs);
        m.insert(QStringLiteral("gpuLoad"), r.gpuLoad);
        m.insert(QStringLiteral("hwCurrent"), r.hwCurrent);
        m.insert(QStringLiteral("videoCodec"), r.videoCodec);
        m.insert(QStringLiteral("cacheIdle"), r.cacheIdle);
        m.insert(QStringLiteral("layerCount"), r.layerCount);
        m.insert(QStringLiteral("latencyMs"), r.latencyMs);
        list.append(m);
    }
    return list;
}

// ---------------------------------------------------------------------------
// Node side
// ---------------------------------------------------------------------------

void NodeTelemetryManager::collectAndSend(const std::vector<std::shared_ptr<BaseLayer>>& layers) {
    if (m_isMaster || !m_enabled.load())
        return;

    const auto now = std::chrono::steady_clock::now();
    if (m_lastCollect.time_since_epoch().count() != 0 &&
        now - m_lastCollect < std::chrono::milliseconds(m_intervalMs.load())) {
        return;
    }
    m_lastCollect = now;

    if (!m_gpuProbe)
        m_gpuProbe = std::make_unique<GpuLoadProbe>();

    double minFps = std::numeric_limits<double>::max();
    quint64 voDrops = 0;
    quint64 decDrops = 0;
    int layerCount = 0;
    QString hw, codec;
    bool cacheIdle = true;

    // Recursively collect active mpv layers, descending into MultiVideoLayer sub-players.
    std::vector<MpvLayer*> activeLayers;
    std::function<void(const std::vector<std::shared_ptr<BaseLayer>>&)> walk =
        [&](const std::vector<std::shared_ptr<BaseLayer>>& ls) {
            for (const auto& layer : ls) {
                if (!layer || !layer->isEnabled())
                    continue;
                if (layer->hasSubLayers()) {
                    walk(layer->getSubLayers());
                    continue;
                }
                auto* mpv = dynamic_cast<MpvLayer*>(layer.get());
                if (mpv && mpv->telemetryActive())
                    activeLayers.push_back(mpv);
            }
        };
    walk(layers);

    for (MpvLayer* mpv : activeLayers)
        aggregateFromHandle(mpv->telemetryHandle(), minFps, voDrops, decDrops, hw, codec,
                            cacheIdle, layerCount);

    // Sync offset: node playback position vs the master's synced timePosition. Live
    // streams have no meaningful offset against the master's file position.
    double offsetMs = 0.0;
    const auto& v = SyncHelper::instance().variables;
    if (v.syncOn) {
        for (MpvLayer* mpv : activeLayers) {
            if (mpv->telemetryIsStream())
                continue;
            const double nodePos = mpv->position();
            if (nodePos >= 0.0) {
                offsetMs = std::abs(nodePos - v.timePosition) * 1000.0;
                break;
            }
        }
    }

    const double gpu = m_gpuProbe->sample();

    json root;
    root["v"] = 1;
    root["node"] = sgct::ClusterManager::instance().thisNodeId();
    root["fps"] = (minFps == std::numeric_limits<double>::max()) ? 0.0 : minFps;
    root["voDrops"] = voDrops;
    root["decDrops"] = decDrops;
    root["offsetMs"] = offsetMs;
    root["gpu"] = gpu;
    root["layers"] = layerCount;
    root["hw"] = hw.toStdString();
    root["codec"] = codec.toStdString();
    root["cacheIdle"] = cacheIdle;

    const std::string s = root.dump();
    try {
        sgct::NetworkManager::instance().transferData(
            s.data(), static_cast<int>(s.size()), m_packageCounter.fetch_add(1));
    } catch (...) {
        // transferData may throw if the connection is down; the sweep timer ages us out.
    }
}

void NodeTelemetryManager::setNodeEnabledFromMaster(bool enabled, int intervalMs) {
    m_enabled.store(enabled);
    if (intervalMs >= 100 && intervalMs <= 5000)
        m_intervalMs.store(intervalMs);
    else
        m_intervalMs.store(kDefaultIntervalMs);
    // Note: m_lastCollect is render-thread state and is deliberately not touched here;
    // collectAndSend() re-throttles naturally once m_enabled flips.
}
