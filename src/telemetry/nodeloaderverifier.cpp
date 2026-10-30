/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nodeloaderverifier.h"

#include <nlohmann/json.hpp>
#include <sgct/sgct.h>

#include <QJsonArray>
#include <QMetaObject>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include <algorithm>
#include <format>
#include <functional>

using nlohmann::json;

namespace {

// A node's failures expire when no packet (failure or clear) arrives for this long - i.e. the
// node went offline or was restarted. Clear-on-success is handled by explicit clear packets, so
// an ongoing failure stays in the table until then.
constexpr qint64 kFailureTtlMs = 30000;

// Node-side collection cadence (postDraw runs every frame).
constexpr int kCollectIntervalMs = 1000;

// Playback is held at most this long for nodes that do not answer for a main video load (an
// older node build, a hung decoder), so a show is never blocked for good.
constexpr qint64 kMainLoadTimeoutMs = 30000;

qint64 nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

// Node display name/address from the cluster config (best effort), mirroring the telemetry manager.
QString nodeNameForId(int nodeId) {
    const auto& cm = sgct::ClusterManager::instance();
    if (nodeId >= 0 && nodeId < cm.numberOfNodes()) {
        const QString address = QString::fromStdString(cm.node(nodeId).address());
        if (!address.isEmpty())
            return address;
    }
    return QStringLiteral("node %1").arg(nodeId);
}

// Ids (the clientIndex of the dataTransfer callbacks) of the connected DataTransfer connections.
QList<int> connectedDataTransferClients() {
    QList<int> ids;
    const auto& nm = sgct::NetworkManager::instance();
    for (int i = 0; i < nm.connectionsCount(); ++i) {
        const sgct::Network& connection = nm.connection(i);
        if (connection.type() == sgct::Network::ConnectionType::DataTransfer && connection.isConnected())
            ids.append(connection.id());
    }
    return ids;
}

const char* progressName(MpvLayer::LoadProgress progress) {
    switch (progress) {
    case MpvLayer::LoadProgress::Ready:
        return "ready";
    case MpvLayer::LoadProgress::Failed:
        return "failed";
    default:
        return "loading";
    }
}

} // namespace

// Set to true inside instance() once the singleton has been constructed. The SGCT dataTransfer
// callbacks run on network threads and must not construct this QObject singleton there (a QObject
// created before QApplication exists - or from a non-main thread winning the construction race -
// corrupts Qt's main-thread bookkeeping and crashes at launch); until this flag is set they drop
// events instead. That is harmless: nodes re-report their failures after any DataTransfer
// reconnect, so a late-constructed master picks them up within one collect interval.
static std::atomic<bool> s_loaderVerifierReady{false};

NodeLoaderVerifier& NodeLoaderVerifier::instance() {
    // C++11 guarantees this static is constructed exactly once, even under concurrent calls -
    // but it would be constructed *on whichever thread calls first*. Only the main thread may
    // trigger construction; every non-main-thread entry point checks isReady() first.
    static NodeLoaderVerifier s_instance;
    s_loaderVerifierReady.store(true, std::memory_order_release);
    return s_instance;
}

bool NodeLoaderVerifier::isReady() noexcept {
    return s_loaderVerifierReady.load(std::memory_order_acquire);
}

NodeLoaderVerifier::NodeLoaderVerifier(QObject* parent)
    : QObject(parent)
    , m_isMaster(sgct::Engine::instance().isMaster()) {
    if (m_isMaster) {
        // Always-on expiry sweep: drops failures of nodes that went silent (offline/restart).
        m_sweepTimer = new QTimer(this);
        m_sweepTimer->setInterval(1000);
        connect(m_sweepTimer, &QTimer::timeout, this, &NodeLoaderVerifier::sweepAndExpire);
        m_sweepTimer->start();
        updateMainVideoReady(); // no load yet: ready
    }
}

// ---------------------------------------------------------------------------
// Master side
// ---------------------------------------------------------------------------

void NodeLoaderVerifier::handleNodeData(void* data, int length, int /*packageId*/, int clientIndex) {
    if (!data || length <= 0 || !m_isMaster)
        return; // nodes never receive load-failure packets

    json root;
    try {
        root = json::parse(static_cast<const char*>(data), static_cast<const char*>(data) + length);
        if (!root.is_object() || !root.contains("v") || !root["v"].is_number_integer() ||
            root["v"].get<int>() != 1) {
            return;
        }

        // Packet discriminator: only "loadfail" and "mainload" packets belong to this
        // consumer. Telemetry packets carry no "kind" and are handled by NodeTelemetryManager.
        if (!root.contains("kind") || !root["kind"].is_string())
            return;
        const std::string kind = root["kind"].get<std::string>();
        if (kind != "loadfail" && kind != "mainload")
            return;

        if (!root.contains("node") || !root["node"].is_number_integer())
            return;
        const int nodeId = root["node"].get<int>();
        if (nodeId < 0)
            return;

        if (kind == "mainload") {
            if (!root.contains("load") || !root["load"].is_number_integer())
                return;
            handleMainLoadPacket(nodeId, clientIndex, root["load"].get<int>(),
                                 root.value("state", std::string()));
            return;
        }

        const QString path = QString::fromStdString(root.value("path", std::string()));
        const QString layerTitle = QString::fromStdString(root.value("layerTitle", std::string()));
        const QString layerType = QString::fromStdString(root.value("layerType", std::string()));
        const QString error = QString::fromStdString(root.value("error", std::string()));
        if (path.isEmpty())
            return;

        bool changed = false;
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            m_nodeLastSeenMs.insert(nodeId, nowMs());
            m_clientToNode.insert(clientIndex, nodeId);

            auto& list = m_failures[nodeId];
            if (error.isEmpty()) {
                // Clear packet: the node loaded this path successfully - drop its row(s).
                for (int i = list.size() - 1; i >= 0; --i) {
                    if (list[i].path == path && list[i].layerTitle == layerTitle) {
                        list.removeAt(i);
                        changed = true;
                    }
                }
            } else {
                bool found = false;
                for (auto& f : list) {
                    if (f.path == path && f.layerTitle == layerTitle) {
                        // ts keeps the first-report time so QML can show "failed since".
                        if (f.error != error || f.layerType != layerType) {
                            f.error = error;
                            f.layerType = layerType;
                            changed = true;
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    NodeLoadFailure f;
                    f.nodeId = nodeId;
                    f.name = nodeNameForId(nodeId);
                    f.path = path;
                    f.layerTitle = layerTitle;
                    f.layerType = layerType;
                    f.error = error;
                    f.ts = nowMs();
                    list.append(f);
                    changed = true;
                }
            }
        }

        if (changed)
            QMetaObject::invokeMethod(this, "loadFailuresChanged", Qt::QueuedConnection);
    } catch (const json::exception&) {
        return; // not our JSON (or malformed) - ignore
    }
}

void NodeLoaderVerifier::handleNodeStatus(bool connected, int clientIndex) {
    if (m_isMaster) {
        // A node's DataTransfer connection dropped - its failures are stale now; drop them
        // immediately instead of waiting for the TTL sweep.
        if (connected)
            return;

        bool changed = false;
        bool mainChanged = false;
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_clientToNode.constFind(clientIndex);
            if (it != m_clientToNode.constEnd()) {
                const int nodeId = it.value();
                m_clientToNode.erase(it);
                changed = m_failures.remove(nodeId);
                m_nodeLastSeenMs.remove(nodeId);
            }
            // A node that is gone cannot be waited for.
            mainChanged = m_mainLoad.remove(clientIndex);
            mainChanged = m_mainLoadExpected.removeAll(clientIndex) > 0 || mainChanged;
        }
        if (changed)
            QMetaObject::invokeMethod(this, "loadFailuresChanged", Qt::QueuedConnection);
        if (mainChanged)
            QMetaObject::invokeMethod(this, &NodeLoaderVerifier::updateMainVideoReady, Qt::QueuedConnection);
    } else {
        // Node side: when the DataTransfer connection comes back after a drop (master restart,
        // network blip), reset the dedupe snapshot so still-failing paths are re-reported to the
        // fresh master session.
        const std::lock_guard<std::mutex> lock(m_nodeMutex);
        if (connected && !m_nodeConnected) {
            m_lastSent.clear();
            m_mainSentLoadId = 0; // the main video progress is sent again too
        }
        m_nodeConnected = connected;
    }
}

void NodeLoaderVerifier::sweepAndExpire() {
    const qint64 cutoff = nowMs() - kFailureTtlMs;
    bool changed = false;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_nodeLastSeenMs.begin(); it != m_nodeLastSeenMs.end();) {
            if (it.value() < cutoff) {
                m_failures.remove(it.key());
                changed = true;
                it = m_nodeLastSeenMs.erase(it);
            } else {
                ++it;
            }
        }
    }
    // The sweep timer runs on the GUI thread - a direct emit is fine here.
    if (changed)
        Q_EMIT loadFailuresChanged();

    // Releases a main video load that waits too long.
    if (!m_mainReady)
        updateMainVideoReady();
}

// ---------------------------------------------------------------------------
// Master side: main video load tracking
// ---------------------------------------------------------------------------

void NodeLoaderVerifier::handleMainLoadPacket(int nodeId, int clientIndex, int loadId,
                                              const std::string& state) {
    MainLoadReport report;
    report.nodeId = nodeId;
    report.loadId = loadId;
    report.progress = state == "ready"    ? MpvLayer::LoadProgress::Ready
                      : state == "failed" ? MpvLayer::LoadProgress::Failed
                                          : MpvLayer::LoadProgress::Loading;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_clientToNode.insert(clientIndex, nodeId);
        m_mainLoad.insert(clientIndex, report);
    }
    QMetaObject::invokeMethod(this, &NodeLoaderVerifier::updateMainVideoReady, Qt::QueuedConnection);
}

int NodeLoaderVerifier::beginMainVideoLoad(bool tracked) {
    ++m_mainLoadId;
    m_mainLoadTracked = tracked && m_isMaster;
    m_mainLoadTimedOut = false;
    m_mainLoadStartMs = nowMs();
    {
        // Only the nodes connected now get the load with the next sync frame.
        const std::lock_guard<std::mutex> lock(m_mutex);
        m_mainLoadExpected = m_mainLoadTracked ? connectedDataTransferClients() : QList<int>();
    }
    updateMainVideoReady();
    return m_mainLoadId;
}

bool NodeLoaderVerifier::mainVideoReady() const {
    return m_mainReady;
}

QJsonObject NodeLoaderVerifier::mainVideoLoadState() const {
    return m_mainLoadState;
}

void NodeLoaderVerifier::updateMainVideoReady() {
    QStringList waiting;
    QStringList failed;
    qsizetype nodes = 0;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        nodes = m_mainLoadExpected.size();
        for (int client : std::as_const(m_mainLoadExpected)) {
            const auto it = m_mainLoad.constFind(client);
            if (it == m_mainLoad.constEnd() || it->loadId != m_mainLoadId) {
                // Not heard from for this load yet; its node is known from earlier packets.
                const auto node = m_clientToNode.constFind(client);
                waiting.append(node != m_clientToNode.constEnd() ? nodeNameForId(node.value())
                                                                 : QStringLiteral("connection %1").arg(client));
            } else if (it->progress == MpvLayer::LoadProgress::Loading) {
                waiting.append(nodeNameForId(it->nodeId));
            } else if (it->progress == MpvLayer::LoadProgress::Failed) {
                failed.append(nodeNameForId(it->nodeId));
            }
        }
    }
    waiting.sort();
    failed.sort();

    if (m_mainLoadTracked && !waiting.isEmpty() && !m_mainLoadTimedOut &&
        nowMs() - m_mainLoadStartMs > kMainLoadTimeoutMs) {
        m_mainLoadTimedOut = true;
        sgct::Log::Warning(std::format("Main video load {}: no ready report from {} after {} s, playback allowed",
                                       m_mainLoadId, waiting.join(QStringLiteral(", ")).toStdString(),
                                       kMainLoadTimeoutMs / 1000));
    }

    const bool ready = !m_mainLoadTracked || waiting.isEmpty() || m_mainLoadTimedOut;
    if (ready && !m_mainReady && !failed.isEmpty()) {
        sgct::Log::Warning(std::format("Main video load {}: failed on {}", m_mainLoadId,
                                       failed.join(QStringLiteral(", ")).toStdString()));
    }

    const QJsonObject state{{QStringLiteral("id"), m_mainLoadId},
                            {QStringLiteral("ready"), ready},
                            {QStringLiteral("timedOut"), m_mainLoadTimedOut},
                            {QStringLiteral("nodes"), static_cast<int>(nodes)},
                            {QStringLiteral("waiting"), QJsonArray::fromStringList(waiting)},
                            {QStringLiteral("failed"), QJsonArray::fromStringList(failed)}};
    if (state == m_mainLoadState)
        return;
    m_mainLoadState = state;
    m_mainReady = ready;
    Q_EMIT mainVideoLoadChanged();
}

QVariantList NodeLoaderVerifier::loadFailuresList() const {
    QList<int> ids;
    {
        const std::lock_guard<std::mutex> lock(m_mutex);
        for (auto it = m_failures.constBegin(); it != m_failures.constEnd(); ++it)
            if (!it.value().isEmpty())
                ids.append(it.key());
    }
    std::sort(ids.begin(), ids.end());

    QVariantList list;
    for (int id : ids) {
        const QString name = nodeNameForId(id);
        QVector<NodeLoadFailure> rows;
        {
            const std::lock_guard<std::mutex> lock(m_mutex);
            rows = m_failures.value(id);
        }
        for (const NodeLoadFailure& f : rows) {
            QVariantMap m;
            m.insert(QStringLiteral("nodeId"), f.nodeId);
            m.insert(QStringLiteral("name"), name);
            m.insert(QStringLiteral("path"), f.path);
            m.insert(QStringLiteral("layerTitle"), f.layerTitle);
            m.insert(QStringLiteral("layerType"), f.layerType);
            m.insert(QStringLiteral("error"), f.error);
            list.append(m);
        }
    }
    return list;
}

// ---------------------------------------------------------------------------
// Node side
// ---------------------------------------------------------------------------

void NodeLoaderVerifier::collectAndSend(const std::vector<std::shared_ptr<BaseLayer>>& layers) {
    if (m_isMaster)
        return;

    const auto now = std::chrono::steady_clock::now();
    if (m_lastCollect.time_since_epoch().count() != 0 &&
        now - m_lastCollect < std::chrono::milliseconds(kCollectIntervalMs)) {
        return;
    }
    m_lastCollect = now;

    // Current failure set: walk all layers recursively (MultiVideo sub-layers are full layers),
    // refreshing each one's load status first. Disabled layers are skipped - an operator who
    // disabled a layer does not want its stale failures reported.
    struct Failure {
        QString path;
        QString title;
        QString type;
        QString error;
    };
    QHash<QString, Failure> current;

    std::function<void(const std::vector<std::shared_ptr<BaseLayer>>&)> walk =
        [&](const std::vector<std::shared_ptr<BaseLayer>>& ls) {
            for (const auto& layer : ls) {
                if (!layer || !layer->isEnabled())
                    continue;
                if (layer->hasSubLayers()) {
                    walk(layer->getSubLayers());
                    continue;
                }
                layer->collectLoadStatus();
                if (!layer->loadFailed())
                    continue;
                Failure f;
                f.path = QString::fromStdString(layer->loadStatusPath());
                f.title = QString::fromStdString(layer->title());
                f.type = QString::fromStdString(layer->typeName());
                f.error = QString::fromStdString(layer->loadError());
                if (f.path.isEmpty())
                    continue;
                current.insert(f.path + QLatin1Char('\0') + f.title, f);
            }
        };
    walk(layers);

    // Diff against what was last reported: send new/changed failures and clears for recovered
    // paths. Steady state (nothing changed) sends nothing. The snapshot only records entries
    // that were actually sent, so a failed transfer is retried on the next tick; a DataTransfer
    // reconnect resets it entirely (see handleNodeStatus).
    const int nodeId = sgct::ClusterManager::instance().thisNodeId();

    auto sendPacket = [this, nodeId](const QString& path, const QString& title, const QString& type,
                                     const QString& error) {
        json root;
        root["v"] = 1;
        root["kind"] = "loadfail";
        root["node"] = nodeId;
        root["path"] = path.toStdString();
        root["layerTitle"] = title.toStdString();
        root["layerType"] = type.toStdString();
        root["error"] = error.toStdString(); // empty error = clear packet
        const std::string s = root.dump();
        try {
            sgct::NetworkManager::instance().transferData(
                s.data(), static_cast<int>(s.size()), m_packageCounter.fetch_add(1));
            return true;
        } catch (...) {
            // transferData may throw if the connection is down - retry on the next tick.
            return false;
        }
    };

    QHash<QString, SentState> next;
    for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
        const Failure& f = it.value();
        auto prev = m_lastSent.constFind(it.key());
        if (prev != m_lastSent.constEnd() && prev.value().error == f.error &&
            prev.value().type == f.type) {
            next.insert(it.key(), prev.value()); // unchanged - already reported
            continue;
        }
        if (sendPacket(f.path, f.title, f.type, f.error))
            next.insert(it.key(), SentState{f.path, f.title, f.type, f.error});
    }

    for (auto it = m_lastSent.constBegin(); it != m_lastSent.constEnd(); ++it) {
        if (next.contains(it.key()))
            continue; // still failing - handled above
        // Previously failed path now loads - tell the master to drop its row.
        sendPacket(it.value().path, it.value().title, it.value().type, QString());
    }

    const std::lock_guard<std::mutex> lock(m_nodeMutex);
    m_lastSent = std::move(next);
}

void NodeLoaderVerifier::reportMainVideo(int loadId, MpvLayer::LoadProgress progress) {
    if (m_isMaster || loadId <= 0)
        return;
    {
        const std::lock_guard<std::mutex> lock(m_nodeMutex);
        if (loadId == m_mainSentLoadId && progress == m_mainSentProgress)
            return;
    }
    // transferData drops the packet without a connection; try again on a later frame then.
    if (connectedDataTransferClients().isEmpty())
        return;

    json root;
    root["v"] = 1;
    root["kind"] = "mainload";
    root["node"] = sgct::ClusterManager::instance().thisNodeId();
    root["load"] = loadId;
    root["state"] = progressName(progress);
    const std::string s = root.dump();
    try {
        sgct::NetworkManager::instance().transferData(
            s.data(), static_cast<int>(s.size()), m_packageCounter.fetch_add(1));
    } catch (...) {
        return; // retried on the next frame
    }
    const std::lock_guard<std::mutex> lock(m_nodeMutex);
    m_mainSentLoadId = loadId;
    m_mainSentProgress = progress;
}