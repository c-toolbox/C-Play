/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NODECAPTUREINVENTORY_H
#define NODECAPTUREINVENTORY_H

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVariantList>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Capture card inputs of every cluster node, reported to the master over SGCT's DataTransfer
// channel (shared with NodeTelemetryManager and NodeLoaderVerifier; packets carry
// "kind":"captureinv" and "kind":"captureinv-req"). The master uses them to create Capture
// layers that only capture on one node (CaptureLayer::targetNode).
//
// Node side: a worker thread rescans the inputs every few seconds; postDraw() sends the result
// when it changed or the master asked (all node sends stay on the render thread, since SGCT's
// sockets are not safe for concurrent writers).
class NodeCaptureInventory : public QObject {
    Q_OBJECT

public:
    // Main thread only, once Q(Core)Application exists. Other threads must check isReady() first.
    static NodeCaptureInventory &instance();
    static bool isReady() noexcept;

    NodeCaptureInventory(const NodeCaptureInventory &) = delete;
    NodeCaptureInventory &operator=(const NodeCaptureInventory &) = delete;

    // SGCT network threads.
    void handleData(void *data, int length, int clientIndex);
    void handleStatus(bool connected, int clientIndex);

    // Master, GUI thread: asks all nodes to report their inputs now.
    void requestFromNodes();
    // Master: one row per node in nodes.json and/or with a received report:
    // {role, label, address, online, reported, sdks, inputNames, inputSources}.
    QVariantList nodes() const;
    // Master, any thread: frame size of source on the node named role (nodes.json id or IP), from
    // its running capture or else the input's signal. False when unknown.
    bool captureSize(const std::string &role, const std::string &source, int &width, int &height) const;

    // Node, render thread: sends the latest inventory when it changed or was requested.
    void sendPending();
    // Node: stops the scan worker.
    void shutdown();

Q_SIGNALS:
    void nodesChanged();

private:
    explicit NodeCaptureInventory(QObject *parent = nullptr);
    ~NodeCaptureInventory() override;

    void workerLoop();
    std::string buildPayload() const;

    struct SizeEntry {
        std::string source;
        int width = 0;
        int height = 0;
        bool active = false; // running capture rather than input signal
    };

    struct NodeReport {
        QString role;
        QString address;
        bool online = false;
        QStringList sdks;
        QStringList inputNames;
        QStringList inputSources;
        std::vector<SizeEntry> sizes;
    };

    const bool m_isMaster;
    std::atomic_int m_packageCounter = 0;

    // Master side.
    mutable std::mutex m_mutex;
    QHash<int, NodeReport> m_reports; // keyed by SGCT node index
    QHash<int, int> m_clientToNode;   // DataTransfer client index -> node index

    // Node side.
    std::thread m_worker;
    std::mutex m_workerMutex;
    std::condition_variable m_workerCv;
    bool m_stop = false;
    bool m_rescan = false;
    std::string m_payload;
    bool m_payloadDirty = false;
    std::atomic_bool m_forceSend{true};
};

#endif // NODECAPTUREINVENTORY_H
