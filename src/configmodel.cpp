/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "configmodel.h"

#include <sgct/log.h>

#include <QQmlEngine>
#include <QStringList>

#include <algorithm>
#include <format>

// ---------------------------------------------------------------------------
// ConfigWindowModel
// ---------------------------------------------------------------------------

ConfigWindowModel::ConfigWindowModel(QObject* parent)
    : QAbstractListModel(parent) {
}

void ConfigWindowModel::setRows(std::vector<Row> rows) {
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

int ConfigWindowModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ConfigWindowModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size()))
        return {};

    const Row& r = m_rows[static_cast<size_t>(index.row())];
    switch (role) {
        case NodeIndexRole: return r.nodeIndex;
        case NodeAddressRole: return r.nodeAddress;
        case WindowIdRole: return r.windowId;
        case NameRole: return r.name;
        case IsFullScreenRole: return r.isFullScreen;
        case IsDecoratedRole: return r.isDecorated;
        case IsFloatingRole: return r.isFloating;
        case IsHiddenRole: return r.isHidden;
        case HasAlphaRole: return r.hasAlpha;
        case MonitorRole: return r.monitor;
        case PosXRole: return r.posX;
        case PosYRole: return r.posY;
        case SizeXRole: return r.sizeX;
        case SizeYRole: return r.sizeY;
        case TagsRole: return r.tags;
        default: return {};
    }
}

QHash<int, QByteArray> ConfigWindowModel::roleNames() const {
    return {
        { NodeIndexRole, "nodeIndex" },
        { NodeAddressRole, "nodeAddress" },
        { WindowIdRole, "windowId" },
        { NameRole, "windowName" },
        { IsFullScreenRole, "isFullScreen" },
        { IsDecoratedRole, "isDecorated" },
        { IsFloatingRole, "isFloating" },
        { IsHiddenRole, "isHidden" },
        { HasAlphaRole, "hasAlpha" },
        { MonitorRole, "monitor" },
        { PosXRole, "posX" },
        { PosYRole, "posY" },
        { SizeXRole, "sizeX" },
        { SizeYRole, "sizeY" },
        { TagsRole, "tags" },
    };
}

// ---------------------------------------------------------------------------
// ConfigNodeModel
// ---------------------------------------------------------------------------

ConfigNodeModel::ConfigNodeModel(QObject* parent)
    : QAbstractListModel(parent) {
}

void ConfigNodeModel::setRows(std::vector<Row> rows) {
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

void ConfigNodeModel::setRuntimeRole(int thisNodeId) {
    if (m_thisNodeId == thisNodeId)
        return;
    m_thisNodeId = thisNodeId;
    if (!m_rows.empty()) {
        // Re-emit all rows so the IsThisNode bindings update.
        Q_EMIT dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1));
    }
}

int ConfigNodeModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant ConfigNodeModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_rows.size()))
        return {};

    const Row& r = m_rows[static_cast<size_t>(index.row())];
    switch (role) {
        case AddressRole: return r.address;
        case PortRole: return r.port;
        case DataTransferPortRole: return r.dataTransferPort;
        case WindowCountRole: return r.windowCount;
        case FullScreenCountRole: return r.fullScreenCount;
        case NonFullScreenCountRole: return r.nonFullScreenCount;
        case AllFullScreenRole: return r.windowCount > 0 && r.nonFullScreenCount == 0;
        case AnyFullScreenRole: return r.fullScreenCount > 0;
        case IsMasterNodeRole: return r.isMasterAddress;
        case IsThisNodeRole: return index.row() == m_thisNodeId;
        default: return {};
    }
}

QHash<int, QByteArray> ConfigNodeModel::roleNames() const {
    return {
        { AddressRole, "address" },
        { PortRole, "port" },
        { DataTransferPortRole, "dataTransferPort" },
        { WindowCountRole, "windowCount" },
        { FullScreenCountRole, "fullScreenCount" },
        { NonFullScreenCountRole, "nonFullScreenCount" },
        { AllFullScreenRole, "allFullScreen" },
        { AnyFullScreenRole, "anyFullScreen" },
        { IsMasterNodeRole, "isMasterNode" },
        { IsThisNodeRole, "isThisNode" },
    };
}

// ---------------------------------------------------------------------------
// ConfigModel
// ---------------------------------------------------------------------------

ConfigModel* ConfigModel::_instance = nullptr;

ConfigModel::ConfigModel(QObject* parent)
    : QObject(parent)
    , m_nodesModel(new ConfigNodeModel(this))
    , m_windowsModel(new ConfigWindowModel(this)) {
}

ConfigModel& ConfigModel::instance() {
    if (_instance == nullptr) {
        _instance = new ConfigModel();
    }
    return *_instance;
}

void ConfigModel::initializeCluster(const sgct::config::Cluster& cluster,
                                    const sgct::Configuration& cmdConfig) {
    m_cluster = cluster;
    m_cmdConfig = cmdConfig;
    m_loaded = cluster.success;

    std::vector<ConfigNodeModel::Row> nodeRows;
    std::vector<ConfigWindowModel::Row> windowRows;
    nodeRows.reserve(cluster.nodes.size());

    for (size_t n = 0; n < cluster.nodes.size(); n++) {
        const auto& node = cluster.nodes[n];

        ConfigNodeModel::Row nodeRow;
        nodeRow.address = QString::fromStdString(node.address);
        nodeRow.port = node.port;
        nodeRow.dataTransferPort = node.dataTransferPort.value_or(0);
        nodeRow.windowCount = static_cast<int>(node.windows.size());
        // A node sharing the master's address is the master's own (hidden) entry, not a
        // real node: its windows never count towards the node window statistics.
        nodeRow.isMasterAddress = isMasterAddressNode(static_cast<int>(n));

        for (const auto& w : node.windows) {
            // Defaults match sgct::Window's constructor (what SGCT actually creates).
            const bool fullScreen = w.isFullScreen.value_or(false);
            nodeRow.fullScreenCount += fullScreen ? 1 : 0;
            nodeRow.nonFullScreenCount += fullScreen ? 0 : 1;

            ConfigWindowModel::Row row;
            row.nodeIndex = static_cast<int>(n);
            row.nodeAddress = nodeRow.address;
            row.windowId = w.id;
            row.name = QString::fromStdString(w.name.value_or(""));
            row.isFullScreen = fullScreen;
            row.isDecorated = w.isDecorated.value_or(true);
            row.isFloating = w.isFloating.value_or(false);
            row.isHidden = w.isHidden.value_or(false);
            row.hasAlpha = w.alpha.value_or(false);
            row.monitor = w.monitor.value_or(0);
            row.posX = w.pos.has_value() ? w.pos->x : 0;
            row.posY = w.pos.has_value() ? w.pos->y : 0;
            row.sizeX = w.size.has_value() ? w.size->x : 0;
            row.sizeY = w.size.has_value() ? w.size->y : 0;
            row.tags.clear();
            row.tags.reserve(static_cast<int>(w.tags.size()));
            for (const auto& tag : w.tags)
                row.tags.append(QString::fromStdString(tag));
            windowRows.push_back(std::move(row));
        }

        nodeRows.push_back(std::move(nodeRow));
    }

    m_nodesModel->setRows(std::move(nodeRows));
    m_windowsModel->setRows(std::move(windowRows));

    if (m_loaded) {
        sgct::Log::Info(std::format(
            "ConfigModel: cluster '{}' - {} node(s), {} node window(s), {} windowed (non-fullscreen)",
            m_cmdConfig.configFilename.value_or("<default>"), cluster.nodes.size(),
            nodeWindowCount(), nodeNonFullScreenWindowCount()));
        if (!nodeWindowTransparencySupported()) {
            sgct::Log::Info(
                "ConfigModel: all node windows are fullscreen - the node window "
                "transparency/fading feature is disabled (it requires windowed nodes)");
        }
    }

    Q_EMIT clusterChanged();
}

void ConfigModel::setCPlayCommandLineOptions(const QString& mpvConfFolder,
                                             const QString& mpvApiOverride,
                                             const QString& logLevel,
                                             const QString& logFile,
                                             const QString& startupFile,
                                             bool allowDirectRendering) {
    m_mpvConfFolder = mpvConfFolder;
    m_mpvApiOverride = mpvApiOverride;
    m_logLevel = logLevel;
    m_logFile = logFile;
    m_startupFile = startupFile;
    m_allowDirectRendering = allowDirectRendering;
    Q_EMIT commandLineChanged();
}

void ConfigModel::setRuntimeRole(int thisNodeId, bool isMaster) {
    m_thisNodeId = thisNodeId;
    m_isMaster = isMaster;
    m_runtimeRoleSet = true;
    m_nodesModel->setRuntimeRole(thisNodeId);
    Q_EMIT runtimeRoleChanged();
}

bool ConfigModel::isMasterAddressNode(int nodeIndex) const {
    if (!m_loaded || nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= m_cluster.nodes.size())
        return false;
    // An empty master address matches no node (same as SGCT's address matching).
    if (m_cluster.masterAddress.empty())
        return false;
    // Hostnames are case-insensitive; compare accordingly.
    return QString::fromStdString(m_cluster.nodes[static_cast<size_t>(nodeIndex)].address)
               .compare(QString::fromStdString(m_cluster.masterAddress), Qt::CaseInsensitive) == 0;
}

bool ConfigModel::countsAsNodeWindow(int nodeIndex) const {
    // Nodes whose address equals the cluster's master address are the master's own
    // entries: their windows are hidden at startup and never faded, so they must not
    // influence whether the transparency feature is usable. Only nodes with another
    // address than the master address count as nodes.
    return !isMasterAddressNode(nodeIndex);
}

bool ConfigModel::isLoaded() const {
    return m_loaded;
}

int ConfigModel::numberOfNodes() const {
    return m_loaded ? static_cast<int>(m_cluster.nodes.size()) : 0;
}

int ConfigModel::nodeCount() const {
    int count = 0;
    for (int i = 0; i < numberOfNodes(); i++)
        if (countsAsNodeWindow(i))
            count++;
    return count;
}

bool ConfigModel::nodeHasFullScreenWindow(int nodeId) const {
    if (!m_loaded || nodeId < 0 || static_cast<size_t>(nodeId) >= m_cluster.nodes.size())
        return false;
    const auto& node = m_cluster.nodes[static_cast<size_t>(nodeId)];
    return std::any_of(node.windows.begin(), node.windows.end(),
                       [](const sgct::config::Window& w) { return w.isFullScreen.value_or(false); });
}

bool ConfigModel::nodeHasNonFullScreenWindow(int nodeId) const {
    if (!m_loaded || nodeId < 0 || static_cast<size_t>(nodeId) >= m_cluster.nodes.size())
        return false;
    const auto& node = m_cluster.nodes[static_cast<size_t>(nodeId)];
    return std::any_of(node.windows.begin(), node.windows.end(),
                       [](const sgct::config::Window& w) { return !w.isFullScreen.value_or(false); });
}

bool ConfigModel::windowIsFullScreen(int nodeId, int windowIndex) const {
    if (!m_loaded || nodeId < 0 || static_cast<size_t>(nodeId) >= m_cluster.nodes.size())
        return false;
    const auto& node = m_cluster.nodes[static_cast<size_t>(nodeId)];
    if (windowIndex < 0 || static_cast<size_t>(windowIndex) >= node.windows.size())
        return false;
    return node.windows[static_cast<size_t>(windowIndex)].isFullScreen.value_or(false);
}

QString ConfigModel::configFile() const {
    return m_cmdConfig.configFilename ? QString::fromStdString(*m_cmdConfig.configFilename)
                                      : QString();
}

QString ConfigModel::masterAddress() const {
    return QString::fromStdString(m_cluster.masterAddress);
}

bool ConfigModel::debugLog() const {
    return m_cluster.debugLog.value_or(false);
}

bool ConfigModel::firmSync() const {
    return m_cluster.firmSync.value_or(false);
}

int ConfigModel::nodeWindowCount() const {
    int count = 0;
    for (int i = 0; i < numberOfNodes(); i++)
        if (countsAsNodeWindow(i))
            count += static_cast<int>(m_cluster.nodes[static_cast<size_t>(i)].windows.size());
    return count;
}

int ConfigModel::nodeFullScreenWindowCount() const {
    int count = 0;
    for (int i = 0; i < numberOfNodes(); i++) {
        if (!countsAsNodeWindow(i))
            continue;
        for (const auto& w : m_cluster.nodes[static_cast<size_t>(i)].windows)
            if (w.isFullScreen.value_or(false))
                count++;
    }
    return count;
}

int ConfigModel::nodeNonFullScreenWindowCount() const {
    int count = 0;
    for (int i = 0; i < numberOfNodes(); i++) {
        if (!countsAsNodeWindow(i))
            continue;
        for (const auto& w : m_cluster.nodes[static_cast<size_t>(i)].windows)
            if (!w.isFullScreen.value_or(false))
                count++;
    }
    return count;
}

bool ConfigModel::anyNodeWindowFullScreen() const {
    return nodeFullScreenWindowCount() > 0;
}

bool ConfigModel::allNodeWindowsFullScreen() const {
    return nodeWindowCount() > 0 && nodeNonFullScreenWindowCount() == 0;
}

bool ConfigModel::nodeWindowTransparencySupported() const {
    // Whole-window fades and per-pixel alpha require a windowed (non-fullscreen)
    // window: a fullscreen window acquires an exclusive video mode with an opaque
    // pixel format, where GLFW transparency has no effect.
    return nodeNonFullScreenWindowCount() > 0;
}

bool ConfigModel::isMaster() const {
    return m_isMaster;
}

int ConfigModel::thisNodeId() const {
    return m_thisNodeId;
}

bool ConfigModel::isServer() const {
    return m_cmdConfig.isServer.value_or(false);
}

int ConfigModel::nodeId() const {
    return m_cmdConfig.nodeId.value_or(-1);
}

bool ConfigModel::ignoreSync() const {
    return m_cmdConfig.ignoreSync.value_or(false);
}

QString ConfigModel::logLevel() const {
    return m_logLevel;
}

QString ConfigModel::logFile() const {
    return m_logFile;
}

QString ConfigModel::mpvConfFolder() const {
    return m_mpvConfFolder;
}

QString ConfigModel::mpvApiOverride() const {
    return m_mpvApiOverride;
}

QString ConfigModel::startupFile() const {
    return m_startupFile;
}

bool ConfigModel::allowDirectRendering() const {
    return m_allowDirectRendering;
}

QString ConfigModel::screenshotPath() const {
    return m_cmdConfig.screenshotPath ? QString::fromStdString(m_cmdConfig.screenshotPath->string())
                                      : QString();
}

QString ConfigModel::screenshotPrefix() const {
    return m_cmdConfig.screenshotPrefix ? QString::fromStdString(*m_cmdConfig.screenshotPrefix)
                                        : QString();
}

QAbstractListModel* ConfigModel::nodesModel() {
    return m_nodesModel;
}

QAbstractListModel* ConfigModel::windowsModel() {
    return m_windowsModel;
}

QString ConfigModel::nodeAddress(int nodeIndex) const {
    if (!m_loaded || nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= m_cluster.nodes.size())
        return QString();
    return QString::fromStdString(m_cluster.nodes[static_cast<size_t>(nodeIndex)].address);
}

int ConfigModel::nodeWindowCountForNode(int nodeIndex) const {
    if (!m_loaded || nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= m_cluster.nodes.size())
        return 0;
    return static_cast<int>(m_cluster.nodes[static_cast<size_t>(nodeIndex)].windows.size());
}

int ConfigModel::nodeFullScreenWindowCountForNode(int nodeIndex) const {
    if (!m_loaded || nodeIndex < 0 || static_cast<size_t>(nodeIndex) >= m_cluster.nodes.size())
        return 0;
    int count = 0;
    for (const auto& w : m_cluster.nodes[static_cast<size_t>(nodeIndex)].windows)
        if (w.isFullScreen.value_or(false))
            count++;
    return count;
}
