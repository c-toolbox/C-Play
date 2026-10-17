/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef VDONINJAROOMMODEL_H
#define VDONINJAROOMMODEL_H

#include <QAbstractListModel>
#include <QStringList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

// Holds the list of known VDO.Ninja arena rooms, persisted in data/vdoninja-rooms.json.
//
// A room is the C-Play side of an Arena-Call deployment (the caller web page, see its README):
// remote callers open an Arena-Call slot link in their browser and publish their camera and/or
// screen into a VDO.Ninja room. Every slot stream is exposed as a WHEP endpoint that a WebRTC
// layer pulls, either
//  - "direct":   the caller's browser hosts WHEP itself (VDO.Ninja &whepout), reachable through
//                https://whep.vdo.ninja/<stream>; no own server is needed, or
//  - "mediamtx": the caller's browser pushes WHIP into an own MediaMTX server (VDO.Ninja &whipout),
//                and C-Play pulls <mediamtx>/arena/<stream>/whep.
//
// The stream naming is a contract shared with Arena-Call's index.html and must stay identical:
//   roomId        = genRoomId(org, key ? room + "_" + key : room)
//   camera stream = <roomId>_P<n>C,  screen stream = <roomId>_P<n>S,  return feed = <roomId>_R1
class VdoNinjaRoomsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit VdoNinjaRoomsModel(QObject *parent = nullptr);
    ~VdoNinjaRoomsModel();

    enum {
        nameRole = Qt::UserRole,
        titleRole,
        orgRole,
        roomRole,
        keyRole,
        slotsRole,
        arenaUrlRole,
        transportRole,
        mediaMtxServerRole,
        mediaMtxPublicHostRole,
        mediaMtxPublishAuthRole,
        enabledRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void updateRoomsList();

    Q_PROPERTY(int numberOfRooms READ getNumberOfRooms NOTIFY roomsListChanged)
    int getNumberOfRooms() const;

    // Rooms are passed as maps with the role names above as keys, missing keys use the defaults.
    Q_INVOKABLE void addRoom(const QVariantMap &room);
    Q_INVOKABLE void updateRoom(int index, const QVariantMap &room);
    Q_INVOKABLE void removeRoom(int index);
    Q_INVOKABLE void moveRoom(int from, int to);
    Q_INVOKABLE QVariantMap roomAt(int index) const;

    // --- Arena-Call stream id contract (see the class comment) ---

    // VDO.Ninja room id, identical to VDON Call / Arena-Call genRoomId().
    Q_INVOKABLE static QString roomIdFor(const QString &org, const QString &room, const QString &key);
    // "camera" -> <roomId>_<slot>C, "screen" -> <roomId>_<slot>S
    Q_INVOKABLE static QString streamIdFor(const QString &roomId, const QString &slot, const QString &kind);
    // Base URL callers' browsers publish WHIP to (same normalization as VDO.Ninja &mediamtx).
    Q_INVOKABLE static QString mediaMtxPublicBase(const QString &hostOrUrl);
    // MediaMTX path of a stream, "arena/<stream>".
    Q_INVOKABLE static QString mediaMtxPathFor(const QString &streamId);

    Q_INVOKABLE QString roomId(int index) const;
    // WHEP URL C-Play pulls for a slot stream. For "mediamtx" rooms \p mediaMtxWhepBase is the
    // WebRTC base URL of the selected MediaMTX server as seen from C-Play ("http://10.0.0.5:8889");
    // it is ignored for "direct" rooms.
    Q_INVOKABLE QString whepUrl(int index, const QString &slot, const QString &kind,
                                const QString &mediaMtxWhepBase = QString()) const;
    // Arena-Call link a caller opens to publish into a slot ("camera", "screen" or "both").
    Q_INVOKABLE QString callerUrl(int index, const QString &slot, const QString &share) const;
    // Arena-Call landing page that lists all slots of the room (shared with the callers).
    Q_INVOKABLE QString inviteUrl(int index, bool operatorView = false) const;
    // VDO.Ninja director view of the room (mute, kick, chat with callers).
    Q_INVOKABLE QString directorUrl(int index) const;
    // Arena-Call return feed sender: a screen share of the arena that all callers see.
    Q_INVOKABLE QString returnFeedUrl(int index) const;

Q_SIGNALS:
    void roomsListChanged();

private:
    struct Room {
        QString name;
        QString title = QStringLiteral("Decision Arena");
        QString org = QStringLiteral("LIU");
        QString room = QStringLiteral("DecisionArena");
        QString key;
        int slots = 8;
        QString arenaUrl;
        QString transport = QStringLiteral("direct"); // "direct" or "mediamtx"
        QString mediaMtxServer;                       // name in data/mediamtx-servers.json
        QString mediaMtxPublicHost;                   // host[:port] callers publish WHIP to
        QString mediaMtxPublishAuth;                  // optional "user:pass" callers publish with
        bool enabled = true;
    };

    static Room roomFromMap(const QVariantMap &map, const Room &base = Room());
    static QString urlSafeName(const QString &value);
    // Query parameters shared by all Arena-Call links of a room ("mediamtx=..&key=..").
    QString commonParams(const Room &r) const;
    QString arenaBase(const Room &r) const;
    void saveRoomsToFile();
    bool isValidIndex(int index) const;

    QList<Room> m_rooms;
};

// The slot streams of one room (two rows per slot: camera and screen), with their WHEP URL,
// caller link and, when a MediaMTX relay is used, whether the stream is currently published.
class VdoNinjaSlotsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit VdoNinjaSlotsModel(QObject *parent = nullptr);

    enum {
        slotRole = Qt::UserRole,
        kindRole,
        streamIdRole,
        whepUrlRole,
        callerUrlRole,
        onlineRole, // -1 unknown, 0 offline, 1 online
        layerTitleRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRoomsModel(VdoNinjaRoomsModel *rooms);

    // Rebuilds the rows for the room at \p roomIndex (-1 clears). See VdoNinjaRoomsModel::whepUrl()
    // for \p mediaMtxWhepBase.
    Q_INVOKABLE void setRoom(int roomIndex, const QString &mediaMtxWhepBase = QString());
    // Marks the rows whose MediaMTX path is in \p onlinePaths as online, all others as offline.
    Q_INVOKABLE void setOnlinePaths(const QStringList &onlinePaths);
    Q_INVOKABLE void clearOnline();

    Q_INVOKABLE QVariantMap rowAt(int index) const;

    Q_PROPERTY(int numberOfStreams READ getNumberOfStreams NOTIFY slotsChanged)
    int getNumberOfStreams() const;

    Q_PROPERTY(int roomIndex READ roomIndex NOTIFY slotsChanged)
    int roomIndex() const;

    Q_PROPERTY(int onlineCount READ onlineCount NOTIFY slotsChanged)
    int onlineCount() const;

Q_SIGNALS:
    void slotsChanged();

private:
    struct Row {
        QString slot;
        QString kind;
        QString streamId;
        QString whepUrl;
        QString callerUrl;
        int online = -1;
        QString layerTitle;
    };

    VdoNinjaRoomsModel *m_rooms = nullptr;
    QList<Row> m_rows;
    int m_roomIndex = -1;
};

#endif // VDONINJAROOMMODEL_H
