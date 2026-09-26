/*
 * SPDX-FileCopyrightText: 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CLUXCLIENT_H
#define CLUXCLIENT_H

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <QUrl>
#include <QNetworkAccessManager>

class QNetworkReply;

// Client for the C-Lux light server: its REST API and its SSE frame stream.
//
// The UI built on this runs in two modes:
//  - Live mode: commands sent here drive the physical lights, following whatever
//    blackout state the server has. While connected, the full server state is also
//    polled once per second, so changes made from another control surface (the C-Lux
//    web editor, a show application, ...) show up in the UI within a second.
//  - Preview mode (the default): scene and pattern commands are applied to the local
//    state only and queued as pending changes; nothing that alters the server is sent.
//    Entering it blackouts the server so the real output stays dark while the frame
//    stream switches to the raw (pre-mask) feed, so the editor can still watch the show
//    at full brightness. Scene data is refreshed on demand with refreshScenes().
//    Leaving preview replays the queued changes against the server - one request at a
//    time, in the order they were made - and then restores the blackout, so the lights
//    fade back in with exactly the look that was built up.
class CLuxClient : public QObject {
    Q_OBJECT

    public:
    explicit CLuxClient(QObject* parent = nullptr);
    ~CLuxClient();

    // Base URL of the C-Lux server, e.g. "http://localhost:8787". A trailing slash is ignored.
    Q_PROPERTY(QString serverUrl READ serverUrl WRITE setServerUrl NOTIFY serverUrlChanged)
    QString serverUrl() const;
    void setServerUrl(const QString& url);

    // Loads the single configured C-Lux server URL from data/clux-server.json at startup,
    // falling back to a built-in default when nothing is stored. Does not write to disk.
    void loadServerConfig();

    // Persists the current server URL to data/clux-server.json (one entry only). Called
    // automatically whenever setServerUrl() changes the value.
    void saveServerConfig() const;

    // True while the SSE frame stream is open, i.e. connected to a running server.
    Q_PROPERTY(bool connected READ connected NOTIFY connectionStateChanged)
    bool connected() const { return m_connected; }

    // C-Lux can require an edit password for some endpoints (pattern toggles included).
    // When required and not logged in yet, the UI should offer login(). The token is kept
    // for this session only and never persisted.
    Q_PROPERTY(bool authRequired READ authRequired NOTIFY authStateChanged)
    bool authRequired() const { return m_authRequired; }

    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authStateChanged)
    bool authenticated() const { return !m_token.isEmpty(); }

    // Live mode, see the class comment. In live mode operator commands are sent to the
    // server and its state is polled once per second; in preview mode scene/pattern
    // commands only update the local state and are queued as pending changes instead.
    // Changing it while connected reconfigures the server blackout and restarts the
    // frame stream with or without the raw feed; entering live mode first replays any
    // pending changes (see below).
    Q_PROPERTY(bool liveMode READ liveMode WRITE setLiveMode NOTIFY liveModeChanged)
    bool liveMode() const { return m_liveMode; }
    void setLiveMode(bool on);

    // Number of scene/pattern changes made in preview mode that have not been sent to the
    // server yet. They are replayed, in order, when live mode is entered while connected.
    Q_PROPERTY(int pendingChanges READ pendingChanges NOTIFY pendingChangesChanged)
    int pendingChanges() const { return m_pendingOps.size(); }

    // Number of lights, derived from the first received frame (frame size / 3).
    Q_PROPERTY(int nLights READ nLights NOTIFY stateChanged)
    int nLights() const { return m_nLights; }

    // Server state, refreshed by refreshState() and after each command. Patterns carry
    // name/type/enabled/opacity; scenes carry name plus the number of patterns in them.
    Q_PROPERTY(QVariantList patterns READ patterns NOTIFY stateChanged)
    QVariantList patterns() const { return m_patterns; }

    Q_PROPERTY(QVariantList scenes READ scenes NOTIFY stateChanged)
    QVariantList scenes() const { return m_scenes; }

    // Names of the scenes currently switched on.
    Q_PROPERTY(QStringList appliedScenes READ appliedScenes NOTIFY stateChanged)
    QStringList appliedScenes() const { return m_appliedScenes; }

    Q_PROPERTY(bool blackout READ blackout NOTIFY stateChanged)
    bool blackout() const { return m_blackout; }

    Q_PROPERTY(bool halfLight READ halfLight NOTIFY stateChanged)
    bool halfLight() const { return m_halfLight; }

    // The solid work-light layer, a hardcoded bottom layer on the server.
    Q_PROPERTY(int solidR READ solidR NOTIFY stateChanged)
    int solidR() const { return m_solidColor[0]; }

    Q_PROPERTY(int solidG READ solidG NOTIFY stateChanged)
    int solidG() const { return m_solidColor[1]; }

    Q_PROPERTY(int solidB READ solidB NOTIFY stateChanged)
    int solidB() const { return m_solidColor[2]; }

    Q_PROPERTY(bool solidEnabled READ solidEnabled NOTIFY stateChanged)
    bool solidEnabled() const { return m_solidEnabled; }

    // Latest frame from the SSE stream: nLights*3 ints (r, g, b per light).
    Q_PROPERTY(QVariantList frame READ frame NOTIFY frameChanged)
    QVariantList frame() const { return m_frame; }

    Q_SIGNALS:
    void serverUrlChanged();
    void connectionStateChanged();
    void authStateChanged();
    void liveModeChanged();
    // Emitted whenever the number of queued preview-mode changes (pendingChanges) changes.
    void pendingChangesChanged();
    // Emitted whenever any of the state properties above changes.
    void stateChanged();
    void frameChanged();
    void errorOccurred(const QString& message);

    public Q_SLOTS:
    // Connect to the server: check auth, refresh the state and start the frame stream.
    void connectToServer();
    void disconnectFromServer();

    // Log in with the edit password; only needed while authRequired() is true.
    void login(const QString& password);

    // Re-fetch all server state (patterns, scenes, applied scenes, blackout, half-light,
    // solid color). No-op while disconnected. In live mode this also runs automatically
    // once per second while connected.
    void refreshState();

    // Re-fetch just the scene list and the applied scenes. This is what the "Update
    // scenes" button in preview mode calls; live mode does not need it because its
    // per-second poll already keeps the state current. No-op while disconnected.
    void refreshScenes();

    // Operator commands. All are no-ops while disconnected. The scene/pattern commands
    // work in both modes: live mode sends them to the server at once, preview mode
    // applies them to the local state and queues them for replay (see pendingChanges()).
    void setPatternEnabled(const QString& name, bool enabled);
    void clearPatterns();
    void applyScene(const QString& name);
    void unapplyScene(const QString& name);
    void replaceWithScene(const QString& name);

    // Output commands. These are live-mode only: preview mode already blackouts the real
    // output, so there is nothing to adjust while in it (the UI keeps them disabled).
    void setBlackout(bool on);
    void setHalfLight(bool on);
    void setSolidColor(int r, int g, int b, bool enabled);

    private:
    QUrl apiUrl(const QString& path) const;
    QNetworkReply* request(QNetworkAccessManager::Operation op, const QString& path,
                           const QByteArray& body = {});
    // Returns false when the server could not be reached at all.
    bool handleAuthReply(QNetworkReply* reply);
    void reportStateError(QNetworkReply* reply);
    void startStream();
    void stopStream();
    // Starts or stops the per-second state poll to match m_connected && m_liveMode.
    void updatePolling();

    // Preview-mode bookkeeping: queue a change for later replay and notify the UI, or drop
    // all queued changes.
    void queuePendingOp(const QVariantMap& op);
    void clearPendingOps();
    // Replays m_pendingOps against the server, one request at a time in order; when the
    // last one is done (or the list was empty) it restores the blackout and restarts the
    // stream. Called from setLiveMode(true) while connected.
    void flushPendingOps();
    void sendPendingOp(const QVariantMap& op);
    void finishEnteringLiveMode();
    // The scene with this name as stored in m_scenes, or an empty map when unknown.
    QVariantMap sceneByName(const QString& name) const;

    QNetworkAccessManager* m_nam;

    QString m_serverUrl;
    bool m_connected = false;
    bool m_authRequired = false;
    QByteArray m_token;
    bool m_liveMode = false;
    // Scene/pattern changes made in preview mode, replayed when live mode is entered.
    QVariantList m_pendingOps;
    // Fires refreshState() once per second while connected and in live mode.
    QTimer m_pollTimer;

    int m_nLights = 0;
    QVariantList m_patterns;
    QVariantList m_scenes;
    QStringList m_appliedScenes;
    bool m_blackout = false;
    bool m_halfLight = false;
    int m_solidColor[3] = {0, 0, 0};
    bool m_solidEnabled = false;

    QVariantList m_frame;
    QByteArray m_streamBuffer;
    QNetworkReply* m_streamReply = nullptr;
};

#endif // CLUXCLIENT_H

