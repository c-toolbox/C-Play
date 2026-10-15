/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodecaptureinventory.h"
#include <layers/baselayer.h>
#ifdef CAPTURE_SUPPORT
#include <capture/capturebackend.h>
#include <capture/capturelayer.h>
#endif
#ifdef SPOUT_SUPPORT
#include <layers/spoutlayer.h>
#endif
#ifdef DIRECTSHOW_SUPPORT
#include <layers/directshowmodel.h>
#include <layers/directshowlayer.h>
#endif
#include <utils/nodeidentityconfig.h>

#include <nlohmann/json.hpp>
#include <sgct/sgct.h>

#include <QMetaObject>
#include <QVariantMap>

#include <algorithm>
#include <chrono>

using nlohmann::json;

namespace {

constexpr auto kRescanInterval = std::chrono::seconds(5);
constexpr auto kStatusInterval = std::chrono::seconds(1);
constexpr const char *kKindReport = "captureinv";
constexpr const char *kKindRequest = "captureinv-req";
constexpr const char *kKindStatus = "capturestatus";

// Returns the packet's "kind", or "" when the packet is not a v1 JSON object with one.
std::string packetKind(const void *data, int length, json &root) {
    try {
        root = json::parse(static_cast<const char *>(data), static_cast<const char *>(data) + length);
        if (!root.is_object() || root.value("v", 0) != 1 || !root.contains("kind") || !root["kind"].is_string())
            return {};
        return root["kind"].get<std::string>();
    } catch (const json::exception &) {
        return {};
    }
}

QStringList toStringList(const json &arr) {
    QStringList list;
    if (!arr.is_array())
        return list;
    for (const auto &v : arr)
        if (v.is_string())
            list.append(QString::fromStdString(v.get<std::string>()));
    return list;
}

} // namespace

// Set once the singleton exists; SGCT network callbacks must not construct it (see NodeTelemetryManager).
static std::atomic<bool> s_captureInventoryReady{false};

NodeCaptureInventory &NodeCaptureInventory::instance() {
    static NodeCaptureInventory s_instance;
    s_captureInventoryReady.store(true, std::memory_order_release);
    return s_instance;
}

bool NodeCaptureInventory::isReady() noexcept {
    return s_captureInventoryReady.load(std::memory_order_acquire);
}

NodeCaptureInventory::NodeCaptureInventory(QObject *parent)
    : QObject(parent)
    , m_isMaster(sgct::Engine::instance().isMaster()) {
    if (!m_isMaster)
        m_worker = std::thread(&NodeCaptureInventory::workerLoop, this);
}

NodeCaptureInventory::~NodeCaptureInventory() {
    shutdown();
}

void NodeCaptureInventory::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_workerMutex);
        m_stop = true;
    }
    m_workerCv.notify_all();
    if (m_worker.joinable())
        m_worker.join();
}

// ---------------------------------------------------------------------------
// Master side
// ---------------------------------------------------------------------------

void NodeCaptureInventory::requestFromNodes() {
    if (!m_isMaster)
        return;
    json cmd;
    cmd["v"] = 1;
    cmd["kind"] = kKindRequest;
    const std::string s = cmd.dump();
    try {
        sgct::NetworkManager::instance().transferData(s.data(), static_cast<int>(s.size()), m_packageCounter.fetch_add(1));
    } catch (...) {
        // A node connection is down; it reports again when it reconnects.
    }
}

QVariantList NodeCaptureInventory::nodes() const {
    QList<int> ids;
    QHash<int, NodeReport> reports;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        reports = m_reports;
    }
    ids = reports.keys();
    std::sort(ids.begin(), ids.end());

    QVariantList list;
    auto addRow = [&list](const NodeReport &r, bool reported) {
        QVariantMap m;
        m.insert(QStringLiteral("role"), r.role);
        m.insert(QStringLiteral("address"), r.address);
        m.insert(QStringLiteral("label"), r.address.isEmpty() || r.address == r.role
                                             ? r.role
                                             : QStringLiteral("%1 (%2)").arg(r.role, r.address));
        m.insert(QStringLiteral("online"), r.online);
        m.insert(QStringLiteral("reported"), reported);
        m.insert(QStringLiteral("spoutSenders"), r.spoutSenders);
        m.insert(QStringLiteral("videoDevices"), r.videoDevices);
        m.insert(QStringLiteral("audioDevices"), r.audioDevices);
        m.insert(QStringLiteral("sdks"), r.sdks);
        m.insert(QStringLiteral("inputNames"), r.inputNames);
        m.insert(QStringLiteral("inputSources"), r.inputSources);
        list.append(m);
    };
    for (int id : ids)
        addRow(reports.value(id), true);

    // Nodes listed in nodes.json that have not reported (offline, or no capture build).
    NodeIdentityConfig identity;
    const std::string path = NodeIdentityConfig::findDefaultFilePath();
    if (!path.empty())
        identity.loadFromFile(path);
    for (const auto &[ip, id] : identity.entries()) {
        const QString qid = QString::fromStdString(id);
        const QString qip = QString::fromStdString(ip);
        const bool known = std::any_of(reports.cbegin(), reports.cend(), [&](const NodeReport &r) {
            return r.role == qid || r.address == qip;
        });
        if (known)
            continue;
        NodeReport r;
        r.role = qid;
        r.address = qip;
        addRow(r, false);
    }
    return list;
}

bool NodeCaptureInventory::captureSize(const std::string &role, const std::string &source, int &width, int &height) const {
#ifdef CAPTURE_SUPPORT
    const CaptureSource wanted = CaptureSource::fromString(source);
    const QString qrole = QString::fromStdString(role);
    const std::lock_guard<std::mutex> lock(m_mutex);
    for (const NodeReport &r : m_reports) {
        if (r.role != qrole && r.address != qrole)
            continue;
        // Prefer the running capture (exact source, then same input), then the input's signal size.
        const SizeEntry *best = nullptr;
        int bestRank = 0;
        for (const SizeEntry &e : r.sizes) {
            if (e.width <= 0 || e.height <= 0)
                continue;
            const CaptureSource s = CaptureSource::fromString(e.source);
            if (s.backend != wanted.backend || s.input != wanted.input)
                continue;
            const int rank = e.active ? (e.source == source ? 3 : 2) : 1;
            if (rank > bestRank) {
                best = &e;
                bestRank = rank;
            }
        }
        if (best) {
            width = best->width;
            height = best->height;
            return true;
        }
    }
#else
    (void)role; (void)source; (void)width; (void)height;
#endif
    return false;
}

// ---------------------------------------------------------------------------
// Both sides (network threads)
// ---------------------------------------------------------------------------

void NodeCaptureInventory::handleData(void *data, int length, int clientIndex) {
    if (!data || length <= 0)
        return;
    json root;
    const std::string kind = packetKind(data, length, root);

    if (!m_isMaster) {
        if (kind == kKindRequest) {
            m_forceSend.store(true);
            m_statusForceSend.store(true);
            {
                std::lock_guard<std::mutex> lock(m_workerMutex);
                m_rescan = true;
            }
            m_workerCv.notify_all();
        }
        return;
    }

    if (kind == kKindStatus) {
        handleLayerStatus(root, clientIndex);
        return;
    }

    if (kind != kKindReport || !root.contains("node") || !root["node"].is_number_integer())
        return;
    const int nodeId = root["node"].get<int>();
    if (nodeId < 0)
        return;

    NodeReport r;
    try {
        r.role = QString::fromStdString(root.value("role", std::string()));
        r.address = QString::fromStdString(root.value("address", std::string()));
        r.spoutSenders = toStringList(root.value("spoutSenders", json::array()));
        r.videoDevices = toStringList(root.value("videoDevices", json::array()));
        r.audioDevices = toStringList(root.value("audioDevices", json::array()));
        r.sdks = toStringList(root.value("sdks", json::array()));
        const json inputs = root.value("inputs", json::array());
        if (inputs.is_array()) {
            for (const auto &in : inputs) {
                if (!in.is_object())
                    continue;
                const std::string source = in.value("source", std::string());
                if (source.empty())
                    continue;
                r.inputSources.append(QString::fromStdString(source));
                r.inputNames.append(QString::fromStdString(in.value("name", source)));
                r.sizes.push_back({source, in.value("w", 0), in.value("h", 0), false});
            }
        }
        const json active = root.value("active", json::array());
        if (active.is_array()) {
            for (const auto &a : active) {
                if (a.is_object())
                    r.sizes.push_back({a.value("source", std::string()), a.value("w", 0), a.value("h", 0), true});
            }
        }
    } catch (const json::exception &) {
        return;
    }
    if (r.role.isEmpty())
        r.role = r.address.isEmpty() ? QStringLiteral("node %1").arg(nodeId) : r.address;
    r.online = true;

    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        const auto previous = m_reports.constFind(nodeId);
        if (previous != m_reports.constEnd())
            r.layers = previous->layers; // reported separately
        m_reports.insert(nodeId, r);
        m_clientToNode.insert(clientIndex, nodeId);
    }
    QMetaObject::invokeMethod(this, "nodesChanged", Qt::QueuedConnection);
}

void NodeCaptureInventory::handleLayerStatus(const nlohmann::json &root, int clientIndex) {
    QHash<uint32_t, LayerStatus> layers;
    int nodeId = -1;
    QString role, address;
    try {
        nodeId = root.value("node", -1);
        role = QString::fromStdString(root.value("role", std::string()));
        address = QString::fromStdString(root.value("address", std::string()));
        const json list = root.value("layers", json::array());
        if (list.is_array()) {
            for (const auto &l : list) {
                if (!l.is_object() || !l.contains("id") || !l["id"].is_number_unsigned())
                    continue;
                LayerStatus s;
                s.nodeOnline = true;
                s.reported = true;
                s.ok = l.value("ok", false);
                s.status = l.value("status", std::string());
                s.width = l.value("w", 0);
                s.height = l.value("h", 0);
                s.source = l.value("source", std::string());
                layers.insert(l["id"].get<uint32_t>(), s);
            }
        }
    } catch (const json::exception &) {
        return;
    }
    if (nodeId < 0)
        return;

    const std::lock_guard<std::mutex> lock(m_mutex);
    NodeReport &r = m_reports[nodeId];
    if (r.role.isEmpty())
        r.role = role.isEmpty() ? address : role;
    if (r.address.isEmpty())
        r.address = address;
    r.online = true;
    r.layers = std::move(layers);
    m_clientToNode.insert(clientIndex, nodeId);
}

NodeCaptureInventory::LayerStatus NodeCaptureInventory::layerStatus(const std::string &role, uint32_t layerId) const {
    LayerStatus result;
    const QString qrole = QString::fromStdString(role);
    const std::lock_guard<std::mutex> lock(m_mutex);
    for (const NodeReport &r : m_reports) {
        if (r.role != qrole && r.address != qrole)
            continue;
        const auto it = r.layers.constFind(layerId);
        if (r.online && it != r.layers.constEnd())
            return it.value();
        result.nodeOnline = r.online;
        return result;
    }
    return result;
}

std::vector<std::pair<std::string, NodeCaptureInventory::LayerStatus>> NodeCaptureInventory::layerStatuses(uint32_t layerId) const {
    std::vector<std::pair<std::string, LayerStatus>> result;
    const std::lock_guard<std::mutex> lock(m_mutex);
    QList<int> ids = m_reports.keys();
    std::sort(ids.begin(), ids.end());
    for (int id : ids) {
        const NodeReport &r = m_reports.constFind(id).value();
        const auto it = r.layers.constFind(layerId);
        if (it == r.layers.constEnd())
            continue;
        LayerStatus s = it.value();
        s.nodeOnline = r.online;
        result.emplace_back(r.role.toStdString(), std::move(s));
    }
    return result;
}

void NodeCaptureInventory::handleStatus(bool connected, int clientIndex) {
    if (!m_isMaster) {
        // Report again to a (re)connected master.
        if (connected) {
            m_forceSend.store(true);
            m_statusForceSend.store(true);
        }
        return;
    }
    if (connected)
        return;
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        const auto it = m_clientToNode.constFind(clientIndex);
        if (it != m_clientToNode.constEnd()) {
            auto report = m_reports.find(it.value());
            if (report != m_reports.end() && report->online) {
                report->online = false;
                changed = true;
            }
        }
    }
    if (changed)
        QMetaObject::invokeMethod(this, "nodesChanged", Qt::QueuedConnection);
}

// ---------------------------------------------------------------------------
// Node side
// ---------------------------------------------------------------------------

std::string NodeCaptureInventory::buildPayload() const {
    json root;
    root["v"] = 1;
    root["kind"] = kKindReport;
    const auto &cm = sgct::ClusterManager::instance();
    root["node"] = cm.thisNodeId();
    root["role"] = machineRole(false);
    root["address"] = cm.thisNode().address();

#ifdef CAPTURE_SUPPORT
    json sdks = json::array();
    for (const std::string &sdk : CaptureBackend::availableSdks())
        sdks.push_back(sdk);
    root["sdks"] = sdks;

    json inputs = json::array();
    for (const CaptureInputInfo &info : CaptureBackend::listInputs())
        inputs.push_back({{"name", info.name}, {"source", info.source.toString()}, {"w", info.width}, {"h", info.height}});
    root["inputs"] = inputs;

    json active = json::array();
    for (const CaptureInputInfo &info : CaptureBackend::activeCaptures())
        active.push_back({{"source", info.source.toString()}, {"w", info.width}, {"h", info.height}});
    root["active"] = active;
#endif
#ifdef SPOUT_SUPPORT
    // Own a separate SDK instance: the GUI's singleton finder must stay on its thread.
    const auto release = [](SPOUTLIBRARY* library) { if (library) library->Release(); };
    std::unique_ptr<SPOUTLIBRARY, decltype(release)> finder(GetSpout(), release);
    root["spoutSenders"] = json::array();
    if (finder) {
        const int count = finder->GetSenderCount();
        for (int i = 0; i < count; ++i) {
            char name[256] = {};
            if (finder->GetSender(i, name)) root["spoutSenders"].push_back(name);
        }
    }
#endif
#ifdef DIRECTSHOW_SUPPORT
    DirectShowModel devices;
    devices.updateDeviceLists(false);
    root["videoDevices"] = json::array();
    root["audioDevices"] = json::array();
    for (const QString& device : devices.videoDevices()) root["videoDevices"].push_back(device.toStdString());
    for (const QString& device : devices.audioDevices()) root["audioDevices"].push_back(device.toStdString());
#endif
    return root.dump();
}

void NodeCaptureInventory::workerLoop() {
    std::unique_lock<std::mutex> lock(m_workerMutex);
    while (!m_stop) {
        lock.unlock();
        std::string payload;
        try {
            payload = buildPayload();
        } catch (...) {
        }
        lock.lock();
        if (!payload.empty() && payload != m_payload) {
            m_payload = std::move(payload);
            m_payloadDirty = true;
        }
        m_workerCv.wait_for(lock, kRescanInterval, [this] { return m_stop || m_rescan; });
        m_rescan = false;
    }
}

void NodeCaptureInventory::sendLayerStatus(const std::vector<std::shared_ptr<BaseLayer>> &layers) {
    if (m_isMaster)
        return;
    const auto now = std::chrono::steady_clock::now();
    if (m_lastStatusCollect.time_since_epoch().count() != 0 && now - m_lastStatusCollect < kStatusInterval)
        return;
    m_lastStatusCollect = now;

    json list = json::array();
    for (const auto &layer : layers) {
        if (!layer)
            continue;
        const std::string target = layer->targetNode();
        // Capture layers without target node capture on every machine (per-machine presets).
#ifdef CAPTURE_SUPPORT
        const bool everyMachine = target.empty() && layer->type() == BaseLayer::CAPTURE;
#else
        const bool everyMachine = false;
#endif
        if (!everyMachine && (target.empty() || !isThisMachine(target, false)))
            continue;
        bool ok = false;
        std::string status;
        std::string source;
#ifdef CAPTURE_SUPPORT
        if (layer->type() == BaseLayer::CAPTURE) {
            const auto* capture = static_cast<const CaptureLayer*>(layer.get());
            ok = capture->captureOk();
            status = capture->statusText();
            source = capture->sourceText();
        }
#endif
#ifdef SPOUT_SUPPORT
        if (layer->type() == BaseLayer::SPOUT) {
            ok = static_cast<const SpoutLayer*>(layer.get())->captureOk();
            status = ok ? "ready" : "Waiting for Spout sender";
        }
#endif
#ifdef DIRECTSHOW_SUPPORT
        if (layer->type() == BaseLayer::DIRECTSHOW) {
            ok = static_cast<const DirectShowLayer*>(layer.get())->captureOk();
            status = ok ? "ready" : "Waiting for DirectShow frames";
        }
#endif
        list.push_back({{"id", layer->identifier()}, {"ok", ok}, {"status", status}, {"source", source},
                        {"w", ok ? layer->width() : 0}, {"h", ok ? layer->height() : 0}});
    }

    json root;
    root["v"] = 1;
    root["kind"] = kKindStatus;
    const auto &cm = sgct::ClusterManager::instance();
    root["node"] = cm.thisNodeId();
    root["role"] = machineRole(false);
    root["address"] = cm.thisNode().address();
    root["layers"] = list;
    const std::string payload = root.dump();
    if (payload == m_lastStatusPayload && !m_statusForceSend.load())
        return;
    m_statusForceSend.store(false);
    try {
        sgct::NetworkManager::instance().transferData(payload.data(), static_cast<int>(payload.size()),
                                                      m_packageCounter.fetch_add(1));
        m_lastStatusPayload = payload;
    } catch (...) {
        m_statusForceSend.store(true); // connection down - retry next interval
    }
}

void NodeCaptureInventory::sendPending() {
    if (m_isMaster)
        return;
    std::string payload;
    {
        std::lock_guard<std::mutex> lock(m_workerMutex);
        if (m_payload.empty() || (!m_payloadDirty && !m_forceSend.load()))
            return;
        payload = m_payload;
        m_payloadDirty = false;
    }
    m_forceSend.store(false);
    try {
        sgct::NetworkManager::instance().transferData(payload.data(), static_cast<int>(payload.size()),
                                                      m_packageCounter.fetch_add(1));
    } catch (...) {
        m_forceSend.store(true); // connection down - retry next frame
    }
}

std::string NodeCaptureInventory::machineRole(bool isMaster) {
    if (isMaster) return "master";
    static const NodeIdentityConfig identity = [] {
        NodeIdentityConfig config;
        const std::string path = NodeIdentityConfig::findDefaultFilePath();
        if (!path.empty()) config.loadFromFile(path);
        return config;
    }();
    const std::string role = identity.thisNodeId();
    return role.empty() ? sgct::ClusterManager::instance().thisNode().address() : role;
}

bool NodeCaptureInventory::isThisMachine(const std::string& role, bool isMaster) {
    if (role.empty()) return true;
    if (role == machineRole(isMaster)) return true;
    return !isMaster && role == sgct::ClusterManager::instance().thisNode().address();
}
