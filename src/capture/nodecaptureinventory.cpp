/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodecaptureinventory.h"
#include <capture/capturebackend.h>
#include <capture/capturepresets.h>
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
constexpr const char *kKindReport = "captureinv";
constexpr const char *kKindRequest = "captureinv-req";

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
            {
                std::lock_guard<std::mutex> lock(m_workerMutex);
                m_rescan = true;
            }
            m_workerCv.notify_all();
        }
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
        m_reports.insert(nodeId, r);
        m_clientToNode.insert(clientIndex, nodeId);
    }
    QMetaObject::invokeMethod(this, "nodesChanged", Qt::QueuedConnection);
}

void NodeCaptureInventory::handleStatus(bool connected, int clientIndex) {
    if (!m_isMaster) {
        // Report again to a (re)connected master.
        if (connected)
            m_forceSend.store(true);
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
    root["role"] = CapturePresets::machineRole(false);
    root["address"] = cm.thisNode().address();

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
