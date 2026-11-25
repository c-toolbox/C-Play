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

#include <client.h>
#include <nlohmann/json.hpp>
#include <sgct/sgct.h>

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QSaveFile>
#include <QTextStream>
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

// Raw samples kept per measurement for CSV export (summary stats keep accumulating past
// this). 1M samples is e.g. 10 nodes at 10 reports/s for almost 3 hours.
constexpr size_t kMaxMeasurementSamples = 1000000;

QVariantMap statToMap(const MeasurementStat& s) {
    QVariantMap m;
    m.insert(QStringLiteral("count"), s.count);
    m.insert(QStringLiteral("avg"), s.avg());
    m.insert(QStringLiteral("min"), s.count > 0 ? s.min : -1.0);
    m.insert(QStringLiteral("max"), s.count > 0 ? s.max : -1.0);
    return m;
}

// CSV number: empty when unavailable (negative = "n/a" throughout the telemetry).
QString csvNumber(double v, int digits) {
    return v < 0.0 ? QString() : QString::number(v, 'f', digits);
}

QString csvText(QString s) {
    if (s.contains(QLatin1Char(',')) || s.contains(QLatin1Char('"')) || s.contains(QLatin1Char('\n'))) {
        s.replace(QStringLiteral("\""), QStringLiteral("\"\""));
        s = QLatin1Char('"') + s + QLatin1Char('"');
    }
    return s;
}

qint64 nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void applyGpuSample(NodeTelemetry& row, const GpuLoadSample& gpu) {
    row.gpuLoad = gpu.overall;
    row.gpuRender = gpu.render;
    row.gpuDecode = gpu.videoDecode;
    row.gpuEngines.clear();
    for (const auto& [type, load] : gpu.engineTypes)
        row.gpuEngines.insert(QString::fromStdString(type), load);
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

// Set once Application starts shutting down SGCT; afterwards NetworkManager may be gone.
static std::atomic<bool> s_telemetryShutdown{false};

void NodeTelemetryManager::shutdown() {
    s_telemetryShutdown.store(true, std::memory_order_release);
    if (m_sweepTimer)
        m_sweepTimer->stop();
}

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
        bool stoppedMeasurement = false;
        if (!enabled) {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_nodes.clear();
            m_clientToNode.clear();
            // No more reports will arrive: end a running measurement (results are kept).
            if (m_measuring) {
                m_measuring = false;
                m_measureStopMs = nowMs();
                stoppedMeasurement = true;
            }
        }
        broadcastEnableCommand();
        Q_EMIT telemetryChanged();
        if (stoppedMeasurement)
            Q_EMIT measurementChanged();
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

QVariantMap NodeTelemetryManager::aggregateFromHandle(void* h, double& minFps, quint64& voDrops,
                                                      quint64& decDrops, QString& hw, QString& codec,
                                                      bool& cacheIdle, int& layerCount) {
    QVariantMap detail;
    auto* handle = static_cast<mpv_handle*>(h);
    if (!handle)
        return detail;

    // perf-info must be queried exactly once per interval: mpv resets its
    // measurement window on each query.
    double layerFps = -1.0;
    const QHash<QString, QString> perfInfo = readPerfInfo(handle);
    auto fpsIt = perfInfo.constFind(QStringLiteral("estimated-vf-fps"));
    if (fpsIt == perfInfo.constEnd())
        fpsIt = perfInfo.constFind(QStringLiteral("vsync-fps"));
    if (fpsIt != perfInfo.constEnd()) {
        const double fps = leadingDouble(fpsIt.value());
        if (fps > 0.0) {
            minFps = std::min(minFps, fps);
            layerFps = fps;
        }
    }

    const int64_t vo = readPropertyInt64(handle, "frame-drop-count");
    const int64_t dec = readPropertyInt64(handle, "decoder-frame-drop-count");
    DropBaseline& base = m_baselines[handle];
    if (vo < base.vo)
        base.vo = vo; // counter reset (seek/file load)
    if (dec < base.decoder)
        base.decoder = dec;
    const quint64 layerVo = static_cast<quint64>(vo - base.vo);
    const quint64 layerDec = static_cast<quint64>(dec - base.decoder);
    voDrops += layerVo;
    decDrops += layerDec;
    base.vo = vo;
    base.decoder = dec;

    const QString hwdec = readPropertyString(handle, "hwdec-current");
    const QString format = readPropertyString(handle, "video-format"); // short: hevc, h264, ...
    const QString codecLong = readPropertyString(handle, "video-codec");
    if (hw.isEmpty())
        hw = hwdec;
    if (codec.isEmpty())
        codec = format.isEmpty() ? codecLong : format;

    cacheIdle = cacheIdle && readCacheIdle(handle);

    layerCount++;

    detail.insert(QStringLiteral("engine"), QStringLiteral("mpv"));
    detail.insert(QStringLiteral("codec"), format);
    detail.insert(QStringLiteral("codecLong"), codecLong);
    detail.insert(QStringLiteral("hw"), !hwdec.isEmpty() && hwdec != QStringLiteral("no"));
    detail.insert(QStringLiteral("hwdec"), hwdec);
    detail.insert(QStringLiteral("width"), readPropertyInt64(handle, "video-params/w"));
    detail.insert(QStringLiteral("height"), readPropertyInt64(handle, "video-params/h"));
    detail.insert(QStringLiteral("pixfmt"), readPropertyString(handle, "video-params/pixelformat"));
    detail.insert(QStringLiteral("hwPixfmt"), readPropertyString(handle, "video-params/hw-pixelformat"));
    detail.insert(QStringLiteral("nominalFps"), leadingDouble(readPropertyString(handle, "container-fps")));
    detail.insert(QStringLiteral("fps"), layerFps);
    detail.insert(QStringLiteral("voDrops"), layerVo);
    detail.insert(QStringLiteral("decDrops"), layerDec);
    detail.insert(QStringLiteral("file"), readPropertyString(handle, "filename"));
    return detail;
}

QVariantMap NodeTelemetryManager::aggregateFromEngine(const void* layerKey,
                                                      const MpvLayer::TelemetryEngineSample& sample,
                                                      const EngineBaselines& prev,
                                                      std::chrono::steady_clock::time_point now,
                                                      double& minFps, quint64& voDrops, QString& hw,
                                                      QString& codec, int& layerCount) {
    // The previous collection's baseline, if this layer was seen then.
    const auto prevIt = prev.constFind(layerKey);
    const bool hasPrev = prevIt != prev.constEnd();
    // The engine zeroes its counters when it opens a file: a decrease starts over.
    const bool reset = !hasPrev || sample.framesPresented < prevIt->presented ||
                       sample.framesDropped < prevIt->dropped;

    double layerFps = -1.0;
    quint64 layerVo = 0;
    if (!reset) {
        const double seconds = std::chrono::duration<double>(now - prevIt->time).count();
        if (seconds > 0.0) {
            layerFps = static_cast<double>(sample.framesPresented - prevIt->presented) / seconds;
            minFps = std::min(minFps, layerFps);
        }
        layerVo = sample.framesDropped - prevIt->dropped;
        voDrops += layerVo;
    }
    // On a reset the first interval reports no fps (rather than a bogus one) and no drops.

    m_engineBaselines.insert(layerKey, {sample.framesPresented, sample.framesDropped, now});

    if (hw.isEmpty()) {
        hw = QStringLiteral("vulkan");
        if (!sample.hwDecoding)
            hw += QStringLiteral("-sw");
        hw += sample.gpuInterop ? QStringLiteral("+interop") : QStringLiteral("+pbo");
    }
    const QString engineCodec = QString::fromStdString(sample.codec);
    if (codec.isEmpty())
        codec = engineCodec;

    layerCount++;

    QVariantMap detail;
    detail.insert(QStringLiteral("engine"), QStringLiteral("vulkan"));
    detail.insert(QStringLiteral("codec"), engineCodec);
    detail.insert(QStringLiteral("hw"), sample.hwDecoding);
    detail.insert(QStringLiteral("upload"), sample.gpuInterop ? QStringLiteral("interop")
                                                              : QStringLiteral("host"));
    detail.insert(QStringLiteral("width"), sample.width);
    detail.insert(QStringLiteral("height"), sample.height);
    detail.insert(QStringLiteral("outputFormat"), QString::fromStdString(sample.outputFormat));
    detail.insert(QStringLiteral("nominalFps"), sample.nominalFps);
    detail.insert(QStringLiteral("fps"), layerFps);
    detail.insert(QStringLiteral("voDrops"), layerVo);
    detail.insert(QStringLiteral("file"),
                  QFileInfo(QString::fromStdString(sample.file)).fileName());
    return detail;
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
        row.layers.append(aggregateFromHandle(handle, minFps, row.voDrops, row.decoderDrops,
                                              row.hwCurrent, row.videoCodec, cacheIdle,
                                              row.layerCount));

    row.fps = (minFps == std::numeric_limits<double>::max()) ? 0.0 : minFps;
    row.cacheIdle = cacheIdle;
    row.syncOffsetMs = 0.0; // the master defines the reference clock
    row.latencyMs = 0.0;

    if (!m_gpuProbe)
        m_gpuProbe = std::make_unique<GpuLoadProbe>();
    applyGpuSample(row, m_gpuProbe->sample());

    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_nodes.insert(row.nodeId, row);
        recordSampleLocked(row);
    }
}

// ---------------------------------------------------------------------------
// Measurement over time (master side)
// ---------------------------------------------------------------------------

void NodeTelemetryManager::startMeasurement() {
    if (!m_isMaster)
        return;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_measurements.clear();
        m_measureSamples.clear();
        m_measureSamples.shrink_to_fit();
        m_measureTruncated = false;
        m_measureStartMs = nowMs();
        m_measureStopMs = 0;
        m_measuring = true;
    }
    Q_EMIT measurementChanged();
}

void NodeTelemetryManager::stopMeasurement() {
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_measuring)
            return;
        m_measuring = false;
        m_measureStopMs = nowMs();
    }
    Q_EMIT measurementChanged();
}

bool NodeTelemetryManager::measuring() const {
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_measuring;
}

void NodeTelemetryManager::recordSampleLocked(const NodeTelemetry& row) {
    if (!m_measuring)
        return;

    NodeMeasurement& m = m_measurements[row.nodeId];
    if (m.samples == 0) {
        m.nodeId = row.nodeId;
        m.isMaster = row.isMaster;
        m.firstMs = row.lastSeenMs;
    }
    m.name = row.name;
    m.samples++;
    m.lastMs = row.lastSeenMs;

    // fps is 0 while nothing plays (paused, no media): leave those out of the average.
    if (row.fps > 0.0)
        m.fps.add(row.fps);
    if (row.gpuLoad >= 0.0)
        m.gpuLoad.add(row.gpuLoad);
    if (row.gpuRender >= 0.0)
        m.gpuRender.add(row.gpuRender);
    if (row.gpuDecode >= 0.0)
        m.gpuDecode.add(row.gpuDecode);
    for (auto it = row.gpuEngines.constBegin(); it != row.gpuEngines.constEnd(); ++it)
        m.gpuEngines[it.key()].add(it.value().toDouble());
    if (row.layerCount > 0)
        m.syncOffsetMs.add(row.syncOffsetMs);
    if (row.latencyMs >= 0.0)
        m.latencyMs.add(row.latencyMs);
    // Reported drops are deltas since the node's previous report: the sum is the total.
    m.voDrops += row.voDrops;
    m.decoderDrops += row.decoderDrops;
    if (row.layerCount > 0) {
        m.hwCurrent = row.hwCurrent;
        m.videoCodec = row.videoCodec;
        m.layerCount = row.layerCount;
        m.layers = row.layers;
    }

    if (m_measureSamples.size() >= kMaxMeasurementSamples) {
        m_measureTruncated = true;
        return;
    }
    MeasurementSample s;
    s.timeMs = row.lastSeenMs;
    s.nodeId = row.nodeId;
    s.fps = row.fps;
    s.gpuLoad = row.gpuLoad;
    s.gpuRender = row.gpuRender;
    s.gpuDecode = row.gpuDecode;
    s.gpuEngines = row.gpuEngines; // implicitly shared, cheap
    s.voDrops = row.voDrops;
    s.decoderDrops = row.decoderDrops;
    s.syncOffsetMs = row.syncOffsetMs;
    s.latencyMs = row.latencyMs;
    s.layerCount = row.layerCount;
    m_measureSamples.push_back(std::move(s));
}

QVariantMap NodeTelemetryManager::measurementSummary() const {
    const std::lock_guard<std::mutex> lock(m_mutex);

    QVariantMap result;
    const qint64 end = m_measuring ? nowMs() : m_measureStopMs;
    result.insert(QStringLiteral("running"), m_measuring);
    result.insert(QStringLiteral("startMs"), m_measureStartMs);
    result.insert(QStringLiteral("durationMs"), m_measureStartMs > 0 ? std::max<qint64>(0, end - m_measureStartMs) : 0);
    result.insert(QStringLiteral("sampleCount"), static_cast<qulonglong>(m_measureSamples.size()));
    result.insert(QStringLiteral("truncated"), m_measureTruncated);

    QList<int> ids = m_measurements.keys();
    std::sort(ids.begin(), ids.end()); // master row (-1) first

    QVariantList rows;
    for (int id : ids) {
        const NodeMeasurement& m = m_measurements.value(id);
        QVariantMap r;
        r.insert(QStringLiteral("nodeId"), m.nodeId);
        r.insert(QStringLiteral("name"), m.name);
        r.insert(QStringLiteral("isMaster"), m.isMaster);
        r.insert(QStringLiteral("samples"), m.samples);
        r.insert(QStringLiteral("durationMs"), std::max<qint64>(0, m.lastMs - m.firstMs));
        r.insert(QStringLiteral("fps"), statToMap(m.fps));
        r.insert(QStringLiteral("gpuLoad"), statToMap(m.gpuLoad));
        r.insert(QStringLiteral("gpuRender"), statToMap(m.gpuRender));
        r.insert(QStringLiteral("gpuDecode"), statToMap(m.gpuDecode));
        QVariantMap engines;
        for (auto it = m.gpuEngines.constBegin(); it != m.gpuEngines.constEnd(); ++it)
            engines.insert(it.key(), statToMap(it.value()));
        r.insert(QStringLiteral("gpuEngines"), engines);
        r.insert(QStringLiteral("syncOffsetMs"), statToMap(m.syncOffsetMs));
        r.insert(QStringLiteral("latencyMs"), statToMap(m.latencyMs));
        r.insert(QStringLiteral("voDrops"), static_cast<qulonglong>(m.voDrops));
        r.insert(QStringLiteral("decoderDrops"), static_cast<qulonglong>(m.decoderDrops));
        r.insert(QStringLiteral("hwCurrent"), m.hwCurrent);
        r.insert(QStringLiteral("videoCodec"), m.videoCodec);
        r.insert(QStringLiteral("layerCount"), m.layerCount);
        r.insert(QStringLiteral("layers"), m.layers);
        rows.append(r);
    }
    result.insert(QStringLiteral("rows"), rows);
    return result;
}

QVariantMap NodeTelemetryManager::measurementTimeline(int buckets) const {
    const std::lock_guard<std::mutex> lock(m_mutex);

    QVariantMap result;
    const qint64 end = m_measuring ? nowMs() : m_measureStopMs;
    const qint64 durationMs = m_measureStartMs > 0 ? std::max<qint64>(0, end - m_measureStartMs) : 0;
    const int slots = std::clamp(buckets, 10, 4000);
    const qint64 bucketMs = std::max<qint64>(1, (durationMs + slots - 1) / slots);
    result.insert(QStringLiteral("startMs"), m_measureStartMs);
    result.insert(QStringLiteral("durationMs"), durationMs);
    result.insert(QStringLiteral("bucketMs"), bucketMs);

    // Per node: running sums of the current slot, flushed when a sample lands in a later
    // one. Samples are appended in arrival order, which is time order per node.
    struct Acc {
        double sum[3] = {0, 0, 0};
        int n[3] = {0, 0, 0};
        int count = 0;
    };
    struct Series {
        qint64 slot = -1;
        Acc acc;
        QVariantList t, load[3];
    };
    QHash<int, Series> series;

    auto flush = [bucketMs](Series& s) {
        if (s.acc.count == 0)
            return;
        s.t.append(static_cast<double>(s.slot * bucketMs + bucketMs / 2));
        for (int k = 0; k < 3; ++k)
            s.load[k].append(s.acc.n[k] > 0 ? s.acc.sum[k] / s.acc.n[k] : -1.0);
        s.acc = Acc();
    };

    for (const MeasurementSample& sample : m_measureSamples) {
        Series& s = series[sample.nodeId];
        const qint64 slot = std::max<qint64>(0, sample.timeMs - m_measureStartMs) / bucketMs;
        if (slot != s.slot) {
            flush(s);
            s.slot = slot;
        }
        const double v[3] = {sample.gpuRender, sample.gpuDecode, sample.gpuLoad};
        for (int k = 0; k < 3; ++k) {
            if (v[k] >= 0.0) {
                s.acc.sum[k] += v[k];
                s.acc.n[k]++;
            }
        }
        s.acc.count++;
    }

    QList<int> ids = series.keys();
    std::sort(ids.begin(), ids.end()); // master row (-1) first, like the summary table

    QVariantList list;
    for (int id : ids) {
        Series& s = series[id];
        flush(s);
        const NodeMeasurement m = m_measurements.value(id);
        QVariantMap entry;
        entry.insert(QStringLiteral("nodeId"), id);
        entry.insert(QStringLiteral("name"), m.name);
        entry.insert(QStringLiteral("isMaster"), m.isMaster);
        entry.insert(QStringLiteral("t"), s.t);
        entry.insert(QStringLiteral("render"), s.load[0]);
        entry.insert(QStringLiteral("decode"), s.load[1]);
        entry.insert(QStringLiteral("busiest"), s.load[2]);
        list.append(entry);
    }
    result.insert(QStringLiteral("series"), list);
    return result;
}

QString NodeTelemetryManager::exportMeasurementCsv(const QString& path) const {
    // Copy under the lock, write without it (reports keep arriving while measuring).
    std::vector<MeasurementSample> samples;
    QHash<int, QString> names;
    qint64 startMs = 0;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        samples = m_measureSamples;
        for (auto it = m_measurements.constBegin(); it != m_measurements.constEnd(); ++it)
            names.insert(it.key(), it->name);
        startMs = m_measureStartMs;
    }
    if (samples.empty())
        return tr("No measurement samples to export.");

    // One column per GPU engine type seen in any sample.
    QStringList engineTypes;
    for (const auto& s : samples) {
        for (auto it = s.gpuEngines.constBegin(); it != s.gpuEngines.constEnd(); ++it) {
            if (!engineTypes.contains(it.key()))
                engineTypes.append(it.key());
        }
    }
    engineTypes.sort();

    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return tr("Could not write %1: %2").arg(path, file.errorString());

    QTextStream out(&file);
    out << "time_s,node_id,node,fps,gpu_busiest_pct,gpu_3d_pct,gpu_decode_pct";
    for (const QString& t : engineTypes)
        out << ',' << csvText(QStringLiteral("gpu_%1_pct").arg(t));
    out << ",vo_drops,dec_drops,sync_offset_ms,latency_ms,layers\n";

    for (const auto& s : samples) {
        out << QString::number((s.timeMs - startMs) / 1000.0, 'f', 3) << ','
            << s.nodeId << ',' << csvText(names.value(s.nodeId)) << ','
            << QString::number(s.fps, 'f', 2) << ','
            << csvNumber(s.gpuLoad, 1) << ',' << csvNumber(s.gpuRender, 1) << ','
            << csvNumber(s.gpuDecode, 1);
        for (const QString& t : engineTypes) {
            const auto it = s.gpuEngines.constFind(t);
            out << ',' << (it != s.gpuEngines.constEnd() ? csvNumber(it->toDouble(), 1) : QString());
        }
        out << ',' << s.voDrops << ',' << s.decoderDrops << ','
            << QString::number(s.syncOffsetMs, 'f', 2) << ',' << csvNumber(s.latencyMs, 2) << ','
            << s.layerCount << '\n';
    }
    out.flush();
    if (!file.commit())
        return tr("Could not write %1: %2").arg(path, file.errorString());
    return QString();
}

void NodeTelemetryManager::broadcastEnableCommand() {
    json cmd;
    cmd["v"] = 1;
    cmd["cmd"] = "enable";
    cmd["enable"] = m_enabled.load();
    cmd["interval"] = m_intervalMs.load();
    const std::string s = cmd.dump();

    // NetworkManager is destroyed by the SGCT engine on the render thread during shutdown;
    // stop sending once Application has begun tearing SGCT down.
    if (s_telemetryShutdown.load(std::memory_order_acquire))
        return;
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

        // Packets carrying a "kind" discriminator belong to another DataTransfer consumer
        // (NodeLoaderVerifier's load-failure reports) - not telemetry.
        if (root.contains("kind"))
            return;

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
        // Older nodes send only the (summed) "gpu" value.
        row.gpuRender = root.value("gpu3d", -1.0);
        row.gpuDecode = root.value("gpuDec", -1.0);
        if (root.contains("gpuEng") && root["gpuEng"].is_object()) {
            for (const auto& [type, load] : root["gpuEng"].items()) {
                if (load.is_number())
                    row.gpuEngines.insert(QString::fromStdString(type), load.get<double>());
            }
        }
        row.layerCount = root.value("layers", 0);
        row.hwCurrent = QString::fromStdString(root.value("hw", std::string()));
        row.videoCodec = QString::fromStdString(root.value("codec", std::string()));
        row.cacheIdle = root.value("cacheIdle", true);
        if (root.contains("details") && root["details"].is_array()) {
            const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(root["details"].dump()));
            row.layers = doc.array().toVariantList();
        }

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
            recordSampleLocked(row);
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
        m.insert(QStringLiteral("gpuRender"), r.gpuRender);
        m.insert(QStringLiteral("gpuDecode"), r.gpuDecode);
        m.insert(QStringLiteral("gpuEngines"), r.gpuEngines);
        m.insert(QStringLiteral("hwCurrent"), r.hwCurrent);
        m.insert(QStringLiteral("videoCodec"), r.videoCodec);
        m.insert(QStringLiteral("cacheIdle"), r.cacheIdle);
        m.insert(QStringLiteral("layerCount"), r.layerCount);
        m.insert(QStringLiteral("latencyMs"), r.latencyMs);
        m.insert(QStringLiteral("lastSeenMs"), r.lastSeenMs);
        m.insert(QStringLiteral("layers"), r.layers);
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

    // Recursively collect active video layers, descending into MultiVideoLayer sub-players.
    // Layers whose video comes from another engine (Vulkan) are read from that engine:
    // their mpv is idle or audio only, so its handle has no video stats.
    std::vector<MpvLayer*> activeLayers;
    QVariantList details;
    EngineBaselines prevEngineBaselines;
    prevEngineBaselines.swap(m_engineBaselines);
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
                if (!mpv)
                    continue;
                MpvLayer::TelemetryEngineSample sample;
                QVariantMap detail;
                if (mpv->telemetryEngineSample(sample)) {
                    activeLayers.push_back(mpv);
                    detail = aggregateFromEngine(mpv, sample, prevEngineBaselines, now, minFps,
                                                 voDrops, hw, codec, layerCount);
                } else if (mpv->telemetryActive()) {
                    activeLayers.push_back(mpv);
                    detail = aggregateFromHandle(mpv->telemetryHandle(), minFps, voDrops,
                                                 decDrops, hw, codec, cacheIdle, layerCount);
                } else {
                    continue;
                }
                const QString title = QString::fromStdString(mpv->title());
                detail.insert(QStringLiteral("layer"), title);
                details.append(detail);
            }
        };
    walk(layers);

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

    const GpuLoadSample gpu = m_gpuProbe->sample();

    json root;
    root["v"] = 1;
    root["node"] = sgct::ClusterManager::instance().thisNodeId();
    root["fps"] = (minFps == std::numeric_limits<double>::max()) ? 0.0 : minFps;
    root["voDrops"] = voDrops;
    root["decDrops"] = decDrops;
    root["offsetMs"] = offsetMs;
    root["gpu"] = gpu.overall;
    root["gpu3d"] = gpu.render;
    root["gpuDec"] = gpu.videoDecode;
    json engines = json::object();
    for (const auto& [type, load] : gpu.engineTypes)
        engines[type] = load;
    root["gpuEng"] = std::move(engines);
    root["layers"] = layerCount;
    root["hw"] = hw.toStdString();
    root["codec"] = codec.toStdString();
    root["cacheIdle"] = cacheIdle;
    // Per-layer details for the master's tooltips (QVariant -> JSON through QJsonArray).
    json detailsJson = json::parse(
        QJsonDocument(QJsonArray::fromVariantList(details)).toJson(QJsonDocument::Compact).toStdString(),
        nullptr, false);
    if (!detailsJson.is_discarded())
        root["details"] = std::move(detailsJson);

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
