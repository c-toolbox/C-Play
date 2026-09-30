/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CONFIGMODEL_H
#define CONFIGMODEL_H

#include <QAbstractListModel>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

#include <sgct/commandline.h>
#include <sgct/config.h>

#include <string>
#include <vector>

class QQmlEngine;
class QJSEngine;

// ---------------------------------------------------------------------------
// ConfigWindowModel: one row per window of every node in the loaded cluster
// configuration. Values already have SGCT's runtime defaults applied (see
// sgct::Window's constructor), so what the model reports is what SGCT will
// actually create the windows with.
// ---------------------------------------------------------------------------
class ConfigWindowModel final : public QAbstractListModel {
    Q_OBJECT

public:
    struct Row {
        int nodeIndex = -1;
        QString nodeAddress;
        int windowId = -1;
        QString name;
        bool isFullScreen = false;
        bool isDecorated = true;
        bool isFloating = false;
        bool isHidden = false;
        bool hasAlpha = false;
        int monitor = 0;
        int posX = 0;
        int posY = 0;
        int sizeX = 0;
        int sizeY = 0;
        QStringList tags;
    };

    enum Roles {
        NodeIndexRole = Qt::UserRole + 1,
        NodeAddressRole,
        WindowIdRole,
        NameRole,
        IsFullScreenRole,
        IsDecoratedRole,
        IsFloatingRole,
        IsHiddenRole,
        HasAlphaRole,
        MonitorRole,
        PosXRole,
        PosYRole,
        SizeXRole,
        SizeYRole,
        TagsRole
    };
    Q_ENUM(Roles)

    explicit ConfigWindowModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(std::vector<Row> rows);

private:
    std::vector<Row> m_rows;
};

// ---------------------------------------------------------------------------
// ConfigNodeModel: one row per node in the loaded cluster configuration.
// ---------------------------------------------------------------------------
class ConfigNodeModel final : public QAbstractListModel {
    Q_OBJECT

public:
    struct Row {
        QString address;
        int port = 0;
        int dataTransferPort = 0;
        int windowCount = 0;
        int fullScreenCount = 0;
        int nonFullScreenCount = 0;
        // True when this node's address equals the cluster's master address. Such nodes
        // are the master's own (hidden) entries, not real nodes: their windows never
        // count towards the node window statistics.
        bool isMasterAddress = false;
    };

    enum Roles {
        AddressRole = Qt::UserRole + 1,
        PortRole,
        DataTransferPortRole,
        WindowCountRole,
        FullScreenCountRole,
        NonFullScreenCountRole,
        AllFullScreenRole,
        AnyFullScreenRole,
        IsMasterNodeRole,
        IsThisNodeRole
    };
    Q_ENUM(Roles)

    explicit ConfigNodeModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(std::vector<Row> rows);
    void setRuntimeRole(int thisNodeId);

private:
    std::vector<Row> m_rows;
    int m_thisNodeId = -1;
};

// ---------------------------------------------------------------------------
// ConfigModel: runtime knowledge of the configuration this C-Play process was
// started with. The cluster configuration file (the SGCT cluster JSON that
// master and nodes load at startup via --config) is analysed once at launch
// and exposed as Qt models plus derived state, so the UI and C++ code can
// check the node/window setup at runtime.
//
// The key derived state is nodeWindowTransparencySupported: the node window
// opacity/fading feature only works on windows that are not fullscreen (a
// fullscreen window acquires an exclusive video mode with an opaque pixel
// format, where per-pixel transparency and whole-window fades are
// unavailable). If every node window in the cluster configuration is
// fullscreen, the feature is disabled in the UI and clamped in the
// PlayerController.
//
// The class also keeps the user-supplyable command line options (SGCT's
// --config/--local/--client/... and C-Play's --mpvconf/--mpvapi/--loglevel/
// --logfile/--loadfile/--allowDirectRendering) so they can be inspected at
// runtime.
// ---------------------------------------------------------------------------
class ConfigModel final : public QObject {
    Q_OBJECT

    // --- Cluster configuration file state ---
    Q_PROPERTY(bool loaded READ isLoaded NOTIFY clusterChanged)
    Q_PROPERTY(QString configFile READ configFile NOTIFY clusterChanged)
    Q_PROPERTY(QString masterAddress READ masterAddress NOTIFY clusterChanged)
    Q_PROPERTY(int numberOfNodes READ numberOfNodes NOTIFY clusterChanged)
    Q_PROPERTY(bool debugLog READ debugLog NOTIFY clusterChanged)
    Q_PROPERTY(bool firmSync READ firmSync NOTIFY clusterChanged)

    // --- Window statistics over the node windows (nodes whose address equals the
    // cluster's master address are the master's own hidden entries and never count,
    // only nodes with another address than the master address do) ---
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY clusterChanged)
    Q_PROPERTY(int nodeWindowCount READ nodeWindowCount NOTIFY clusterChanged)
    Q_PROPERTY(int nodeFullScreenWindowCount READ nodeFullScreenWindowCount NOTIFY clusterChanged)
    Q_PROPERTY(int nodeNonFullScreenWindowCount READ nodeNonFullScreenWindowCount NOTIFY clusterChanged)
    Q_PROPERTY(bool anyNodeWindowFullScreen READ anyNodeWindowFullScreen NOTIFY clusterChanged)
    Q_PROPERTY(bool allNodeWindowsFullScreen READ allNodeWindowsFullScreen NOTIFY clusterChanged)
    // True when at least one node window is windowed (non-fullscreen), i.e. the
    // node window transparency/fading feature can work. False when every node
    // window is fullscreen (or no node windows exist at all).
    Q_PROPERTY(bool nodeWindowTransparencySupported READ nodeWindowTransparencySupported NOTIFY clusterChanged)

    // --- Runtime role of this process (set after Engine::create) ---
    Q_PROPERTY(bool isMaster READ isMaster NOTIFY runtimeRoleChanged)
    Q_PROPERTY(int thisNodeId READ thisNodeId NOTIFY runtimeRoleChanged)

    // --- User-supplied command line options ---
    Q_PROPERTY(bool isServer READ isServer NOTIFY commandLineChanged)
    Q_PROPERTY(int nodeId READ nodeId NOTIFY commandLineChanged)
    Q_PROPERTY(bool ignoreSync READ ignoreSync NOTIFY commandLineChanged)
    Q_PROPERTY(QString logLevel READ logLevel NOTIFY commandLineChanged)
    Q_PROPERTY(QString logFile READ logFile NOTIFY commandLineChanged)
    Q_PROPERTY(QString mpvConfFolder READ mpvConfFolder NOTIFY commandLineChanged)
    Q_PROPERTY(QString mpvApiOverride READ mpvApiOverride NOTIFY commandLineChanged)
    Q_PROPERTY(QString startupFile READ startupFile NOTIFY commandLineChanged)
    Q_PROPERTY(bool allowDirectRendering READ allowDirectRendering NOTIFY commandLineChanged)
    Q_PROPERTY(QString screenshotPath READ screenshotPath NOTIFY commandLineChanged)
    Q_PROPERTY(QString screenshotPrefix READ screenshotPrefix NOTIFY commandLineChanged)

    // --- Models for UI access ---
    Q_PROPERTY(QAbstractListModel* nodesModel READ nodesModel CONSTANT)
    Q_PROPERTY(QAbstractListModel* windowsModel READ windowsModel CONSTANT)

public:
    static ConfigModel& instance();

    // Called from main() once the cluster configuration has been loaded (and
    // the DataTransfer port defaults injected), before Engine::create().
    void initializeCluster(const sgct::config::Cluster& cluster, const sgct::Configuration& cmdConfig);

    // Called from main() after the C-Play-specific command line arguments have
    // been parsed (they are consumed after loadCluster in main()).
    void setCPlayCommandLineOptions(const QString& mpvConfFolder, const QString& mpvApiOverride,
                                    const QString& logLevel, const QString& logFile,
                                    const QString& startupFile, bool allowDirectRendering);

    // Called from main() after Engine::create(), when ClusterManager knows
    // which node this process runs as and whether it is the master.
    void setRuntimeRole(int thisNodeId, bool isMaster);

    // --- C++ query API (used by main.cpp's preWindow callback and the node
    // window feature sync) ---
    bool isLoaded() const;
    int numberOfNodes() const;
    // True when any of the node's windows is fullscreen (per the config, with
    // SGCT's defaults applied). Out-of-range node indices report false.
    bool nodeHasFullScreenWindow(int nodeId) const;
    // True when any of the node's windows is windowed (non-fullscreen).
    bool nodeHasNonFullScreenWindow(int nodeId) const;
    // Fullscreen state of one window of a node, with SGCT's default (false).
    bool windowIsFullScreen(int nodeId, int windowIndex) const;

    // --- Property readers ---
    QString configFile() const;
    QString masterAddress() const;
    bool debugLog() const;
    bool firmSync() const;
    int nodeCount() const;
    int nodeWindowCount() const;
    int nodeFullScreenWindowCount() const;
    int nodeNonFullScreenWindowCount() const;
    bool anyNodeWindowFullScreen() const;
    bool allNodeWindowsFullScreen() const;
    bool nodeWindowTransparencySupported() const;
    bool isMaster() const;
    int thisNodeId() const;
    bool isServer() const;
    int nodeId() const;
    bool ignoreSync() const;
    QString logLevel() const;
    QString logFile() const;
    QString mpvConfFolder() const;
    QString mpvApiOverride() const;
    QString startupFile() const;
    bool allowDirectRendering() const;
    QString screenshotPath() const;
    QString screenshotPrefix() const;
    QAbstractListModel* nodesModel();
    QAbstractListModel* windowsModel();

    // --- QML helpers ---
    Q_INVOKABLE QString nodeAddress(int nodeIndex) const;
    Q_INVOKABLE int nodeWindowCountForNode(int nodeIndex) const;
    Q_INVOKABLE int nodeFullScreenWindowCountForNode(int nodeIndex) const;

Q_SIGNALS:
    void clusterChanged();
    void runtimeRoleChanged();
    void commandLineChanged();

private:
    explicit ConfigModel(QObject* parent = nullptr);
    ConfigModel(const ConfigModel&) = delete;
    ConfigModel& operator=(const ConfigModel&) = delete;

    // True when the node's address equals the cluster's master address. Such nodes are
    // the master's own entries (their windows are hidden at startup and never faded),
    // so they must not count as nodes for the node window statistics.
    bool isMasterAddressNode(int nodeIndex) const;
    // True when the window at (nodeIndex, windowIndex) should count towards the
    // "node window" statistics, i.e. its node does not share the master address.
    bool countsAsNodeWindow(int nodeIndex) const;

    static ConfigModel* _instance;

    sgct::config::Cluster m_cluster;
    sgct::Configuration m_cmdConfig;
    bool m_loaded = false;

    int m_thisNodeId = -1;
    bool m_isMaster = false;
    bool m_runtimeRoleSet = false;

    // C-Play-specific command line options (parsed in main() after loadCluster).
    QString m_mpvConfFolder;
    QString m_mpvApiOverride;
    QString m_logLevel;
    QString m_logFile;
    QString m_startupFile;
    bool m_allowDirectRendering = false;

    ConfigNodeModel* m_nodesModel = nullptr;
    ConfigWindowModel* m_windowsModel = nullptr;
};

#endif // CONFIGMODEL_H
