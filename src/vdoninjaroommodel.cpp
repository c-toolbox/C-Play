/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "vdoninjaroommodel.h"

#include <QFile>
#include <algorithm>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace {

const QString roomsFilePath = QStringLiteral("./data/vdoninja-rooms.json");

// Must match Arena-Call's directWhepBase and mediaMtxPathPrefix.
const QString directWhepBase = QStringLiteral("https://whep.vdo.ninja/");
const QString mediaMtxPathPrefix = QStringLiteral("arena");

QString percentEncode(const QString &value) {
    return QString::fromUtf8(QUrl::toPercentEncoding(value));
}

QString capitalized(const QString &value) {
    return value.isEmpty() ? value : value.left(1).toUpper() + value.mid(1);
}

} // namespace

// --- VdoNinjaRoomsModel ---

VdoNinjaRoomsModel::VdoNinjaRoomsModel(QObject *parent)
    : QAbstractListModel(parent) {
}

VdoNinjaRoomsModel::~VdoNinjaRoomsModel() {
}

int VdoNinjaRoomsModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_rooms.size();
}

QVariant VdoNinjaRoomsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || !checkIndex(index))
        return QVariant();

    const Room &r = m_rooms.at(index.row());
    switch (role) {
    case nameRole:
        return r.name;
    case titleRole:
        return r.title;
    case orgRole:
        return r.org;
    case roomRole:
        return r.room;
    case keyRole:
        return r.key;
    case slotsRole:
        return r.slots;
    case arenaUrlRole:
        return r.arenaUrl;
    case transportRole:
        return r.transport;
    case mediaMtxServerRole:
        return r.mediaMtxServer;
    case mediaMtxPublicHostRole:
        return r.mediaMtxPublicHost;
    case mediaMtxPublishAuthRole:
        return r.mediaMtxPublishAuth;
    case enabledRole:
        return r.enabled;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> VdoNinjaRoomsModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[nameRole] = "name";
    roles[titleRole] = "title";
    roles[orgRole] = "org";
    roles[roomRole] = "room";
    roles[keyRole] = "key";
    roles[slotsRole] = "slots";
    roles[arenaUrlRole] = "arenaUrl";
    roles[transportRole] = "transport";
    roles[mediaMtxServerRole] = "mediaMtxServer";
    roles[mediaMtxPublicHostRole] = "mediaMtxPublicHost";
    roles[mediaMtxPublishAuthRole] = "mediaMtxPublishAuth";
    roles[enabledRole] = "enabled";
    return roles;
}

VdoNinjaRoomsModel::Room VdoNinjaRoomsModel::roomFromMap(const QVariantMap &map, const Room &base) {
    Room r = base;
    if (map.contains(QStringLiteral("name")))
        r.name = map.value(QStringLiteral("name")).toString().trimmed();
    if (map.contains(QStringLiteral("title")))
        r.title = map.value(QStringLiteral("title")).toString().trimmed();
    if (map.contains(QStringLiteral("org")))
        r.org = map.value(QStringLiteral("org")).toString().trimmed();
    if (map.contains(QStringLiteral("room")))
        r.room = map.value(QStringLiteral("room")).toString().trimmed();
    if (map.contains(QStringLiteral("key")))
        r.key = map.value(QStringLiteral("key")).toString().trimmed();
    if (map.contains(QStringLiteral("slots")))
        r.slots = std::clamp(map.value(QStringLiteral("slots")).toInt(), 1, 32);
    if (map.contains(QStringLiteral("arenaUrl")))
        r.arenaUrl = map.value(QStringLiteral("arenaUrl")).toString().trimmed();
    if (map.contains(QStringLiteral("transport"))) {
        const QString t = map.value(QStringLiteral("transport")).toString().trimmed().toLower();
        r.transport = (t == QStringLiteral("mediamtx")) ? t : QStringLiteral("direct");
    }
    if (map.contains(QStringLiteral("mediaMtxServer")))
        r.mediaMtxServer = map.value(QStringLiteral("mediaMtxServer")).toString();
    if (map.contains(QStringLiteral("mediaMtxPublicHost")))
        r.mediaMtxPublicHost = map.value(QStringLiteral("mediaMtxPublicHost")).toString().trimmed();
    if (map.contains(QStringLiteral("mediaMtxPublishAuth")))
        r.mediaMtxPublishAuth = map.value(QStringLiteral("mediaMtxPublishAuth")).toString().trimmed();
    if (map.contains(QStringLiteral("enabled")))
        r.enabled = map.value(QStringLiteral("enabled")).toBool();
    return r;
}

void VdoNinjaRoomsModel::updateRoomsList() {
    QFile roomsFile(roomsFilePath);
    if (!roomsFile.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonDocument doc(QJsonDocument::fromJson(roomsFile.readAll()));
    roomsFile.close();
    if (!doc.isObject())
        return;

    QJsonArray arr = doc.object().value(QStringLiteral("rooms")).toArray();

    beginResetModel();
    m_rooms.clear();
    for (auto v : arr) {
        Room r = roomFromMap(v.toObject().toVariantMap());
        if (!r.name.isEmpty() && !r.org.isEmpty() && !r.room.isEmpty())
            m_rooms.append(r);
    }
    endResetModel();
    Q_EMIT roomsListChanged();
}

void VdoNinjaRoomsModel::saveRoomsToFile() {
    QJsonArray arr;
    for (const Room &r : std::as_const(m_rooms)) {
        QJsonObject o;
        o.insert(QStringLiteral("name"), r.name);
        o.insert(QStringLiteral("title"), r.title);
        o.insert(QStringLiteral("org"), r.org);
        o.insert(QStringLiteral("room"), r.room);
        o.insert(QStringLiteral("key"), r.key);
        o.insert(QStringLiteral("slots"), r.slots);
        o.insert(QStringLiteral("arenaUrl"), r.arenaUrl);
        o.insert(QStringLiteral("transport"), r.transport);
        o.insert(QStringLiteral("mediaMtxServer"), r.mediaMtxServer);
        o.insert(QStringLiteral("mediaMtxPublicHost"), r.mediaMtxPublicHost);
        o.insert(QStringLiteral("mediaMtxPublishAuth"), r.mediaMtxPublishAuth);
        o.insert(QStringLiteral("enabled"), r.enabled);
        arr.append(o);
    }
    QJsonObject root;
    root.insert(QStringLiteral("rooms"), arr);

    QFile roomsFile(roomsFilePath);
    if (!roomsFile.open(QIODevice::WriteOnly)) {
        return;
    }
    roomsFile.write(QJsonDocument(root).toJson());
    roomsFile.close();
}

int VdoNinjaRoomsModel::getNumberOfRooms() const {
    return m_rooms.size();
}

bool VdoNinjaRoomsModel::isValidIndex(int index) const {
    return index >= 0 && index < m_rooms.size();
}

void VdoNinjaRoomsModel::addRoom(const QVariantMap &room) {
    Room r = roomFromMap(room);
    if (r.name.isEmpty() || r.org.isEmpty() || r.room.isEmpty())
        return;

    beginInsertRows(QModelIndex(), m_rooms.size(), m_rooms.size());
    m_rooms.append(r);
    endInsertRows();

    saveRoomsToFile();
    Q_EMIT roomsListChanged();
}

void VdoNinjaRoomsModel::updateRoom(int index, const QVariantMap &room) {
    if (!isValidIndex(index))
        return;
    Room r = roomFromMap(room, m_rooms.at(index));
    if (r.name.isEmpty() || r.org.isEmpty() || r.room.isEmpty())
        return;

    m_rooms[index] = r;
    Q_EMIT dataChanged(this->index(index, 0), this->index(index, 0));
    saveRoomsToFile();
    Q_EMIT roomsListChanged();
}

void VdoNinjaRoomsModel::removeRoom(int index) {
    if (!isValidIndex(index))
        return;

    beginRemoveRows(QModelIndex(), index, index);
    m_rooms.removeAt(index);
    endRemoveRows();

    saveRoomsToFile();
    Q_EMIT roomsListChanged();
}

void VdoNinjaRoomsModel::moveRoom(int from, int to) {
    if (!isValidIndex(from) || !isValidIndex(to) || from == to)
        return;

    beginResetModel();
    m_rooms.move(from, to);
    endResetModel();

    saveRoomsToFile();
    Q_EMIT roomsListChanged();
}

QVariantMap VdoNinjaRoomsModel::roomAt(int index) const {
    QVariantMap map;
    if (!isValidIndex(index))
        return map;

    const Room &r = m_rooms.at(index);
    map.insert(QStringLiteral("name"), r.name);
    map.insert(QStringLiteral("title"), r.title);
    map.insert(QStringLiteral("org"), r.org);
    map.insert(QStringLiteral("room"), r.room);
    map.insert(QStringLiteral("key"), r.key);
    map.insert(QStringLiteral("slots"), r.slots);
    map.insert(QStringLiteral("arenaUrl"), r.arenaUrl);
    map.insert(QStringLiteral("transport"), r.transport);
    map.insert(QStringLiteral("mediaMtxServer"), r.mediaMtxServer);
    map.insert(QStringLiteral("mediaMtxPublicHost"), r.mediaMtxPublicHost);
    map.insert(QStringLiteral("mediaMtxPublishAuth"), r.mediaMtxPublishAuth);
    map.insert(QStringLiteral("enabled"), r.enabled);
    map.insert(QStringLiteral("roomId"), roomId(index));
    return map;
}

// --- Arena-Call stream id contract ---

QString VdoNinjaRoomsModel::urlSafeName(const QString &value) {
    // Arena-Call replaces whitespace runs with "_" before a name enters a link.
    static const QRegularExpression whitespace(QStringLiteral("\\s+"));
    QString out = value;
    return out.replace(whitespace, QStringLiteral("_"));
}

QString VdoNinjaRoomsModel::roomIdFor(const QString &org, const QString &room, const QString &key) {
    // genRoomId(): every character outside [a-zA-Z0-9_] becomes the last two hex digits of
    // (UTF-16 code unit % 255), exactly like the JavaScript in VDON Call / Arena-Call.
    const QString roomPart = key.isEmpty() ? urlSafeName(room) : urlSafeName(room) + QLatin1Char('_') + key;
    const QString input = urlSafeName(org) + QLatin1Char('_') + roomPart;
    QString out;
    out.reserve(input.size() * 2);
    for (const QChar ch : input) {
        const ushort c = ch.unicode();
        const bool plain = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
        if (plain)
            out.append(ch);
        else
            out.append(QStringLiteral("0%1").arg(c % 255, 0, 16).right(2));
    }
    return out;
}

QString VdoNinjaRoomsModel::streamIdFor(const QString &roomId, const QString &slot, const QString &kind) {
    return roomId + QLatin1Char('_') + slot + (kind == QStringLiteral("screen") ? QStringLiteral("S") : QStringLiteral("C"));
}

QString VdoNinjaRoomsModel::mediaMtxPublicBase(const QString &hostOrUrl) {
    // Same rules as Arena-Call's mediaMtxBase() and the VDO.Ninja &mediamtx parameter.
    QString base = hostOrUrl.trimmed();
    if (base.isEmpty())
        return QString();
    static const QRegularExpression scheme(QStringLiteral("^https?://"));
    const bool hasScheme = scheme.match(base).hasMatch();
    QString hostPort = base;
    hostPort.remove(scheme);
    const int slash = hostPort.indexOf(QLatin1Char('/'));
    if (slash >= 0)
        hostPort.truncate(slash);
    if (!hostPort.contains(QLatin1Char(':'))) {
        const int at = base.indexOf(hostPort);
        if (at >= 0)
            base.insert(at + hostPort.size(), QStringLiteral(":8889"));
    }
    if (!hasScheme) {
        static const QRegularExpression local(QStringLiteral("^(localhost|127\\.0\\.0\\.1)(:|$)"));
        base.prepend(local.match(hostPort).hasMatch() ? QStringLiteral("http://") : QStringLiteral("https://"));
    }
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    return base;
}

QString VdoNinjaRoomsModel::mediaMtxPathFor(const QString &streamId) {
    return mediaMtxPathPrefix + QLatin1Char('/') + streamId;
}

QString VdoNinjaRoomsModel::roomId(int index) const {
    if (!isValidIndex(index))
        return QString();
    const Room &r = m_rooms.at(index);
    return roomIdFor(r.org, r.room, r.key);
}

QString VdoNinjaRoomsModel::whepUrl(int index, const QString &slot, const QString &kind,
                                    const QString &mediaMtxWhepBase) const {
    if (!isValidIndex(index))
        return QString();
    const Room &r = m_rooms.at(index);
    const QString streamId = streamIdFor(roomIdFor(r.org, r.room, r.key), slot, kind);
    if (r.transport != QStringLiteral("mediamtx"))
        return directWhepBase + streamId;

    // C-Play usually reaches MediaMTX on the LAN, while callers publish to its public address.
    QString base = mediaMtxWhepBase.trimmed();
    if (base.isEmpty())
        base = mediaMtxPublicBase(r.mediaMtxPublicHost);
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    if (base.isEmpty())
        return QString();
    return base + QLatin1Char('/') + mediaMtxPathFor(streamId) + QStringLiteral("/whep");
}

QString VdoNinjaRoomsModel::arenaBase(const Room &r) const {
    QString base = r.arenaUrl;
    const int cut = base.indexOf(QRegularExpression(QStringLiteral("[?#]")));
    if (cut >= 0)
        base.truncate(cut);
    return base;
}

QString VdoNinjaRoomsModel::commonParams(const Room &r) const {
    QStringList p;
    if (r.transport == QStringLiteral("mediamtx") && !r.mediaMtxPublicHost.isEmpty())
        p << QStringLiteral("mediamtx=") + percentEncode(r.mediaMtxPublicHost);
    // Publish-only credentials end up in every caller link (Arena-Call sends them as bearer token).
    if (r.transport == QStringLiteral("mediamtx") && !r.mediaMtxPublishAuth.isEmpty())
        p << QStringLiteral("mtxauth=") + percentEncode(r.mediaMtxPublishAuth);
    if (!r.key.isEmpty())
        p << QStringLiteral("key=") + percentEncode(r.key);
    if (!r.title.isEmpty())
        p << QStringLiteral("title=") + percentEncode(r.title);
    return p.join(QLatin1Char('&'));
}

QString VdoNinjaRoomsModel::callerUrl(int index, const QString &slot, const QString &share) const {
    if (!isValidIndex(index))
        return QString();
    const Room &r = m_rooms.at(index);
    const QString base = arenaBase(r);
    if (base.isEmpty())
        return QString();
    const QString mode = (share == QStringLiteral("camera") || share == QStringLiteral("screen")) ? share : QStringLiteral("both");
    QString url = base + QStringLiteral("#/") + percentEncode(urlSafeName(r.org)) + QLatin1Char('/')
                  + percentEncode(urlSafeName(r.room)) + QLatin1Char('/') + slot + QStringLiteral("?share=") + mode;
    const QString common = commonParams(r);
    if (!common.isEmpty())
        url += QLatin1Char('&') + common;
    return url;
}

QString VdoNinjaRoomsModel::inviteUrl(int index, bool operatorView) const {
    if (!isValidIndex(index))
        return QString();
    const Room &r = m_rooms.at(index);
    const QString base = arenaBase(r);
    if (base.isEmpty())
        return QString();
    QString url = base + QStringLiteral("?org=") + percentEncode(urlSafeName(r.org))
                  + QStringLiteral("&room=") + percentEncode(urlSafeName(r.room))
                  + QStringLiteral("&slots=") + QString::number(r.slots);
    const QString common = commonParams(r);
    if (!common.isEmpty())
        url += QLatin1Char('&') + common;
    if (operatorView)
        url += QStringLiteral("&operator");
    return url;
}

QString VdoNinjaRoomsModel::directorUrl(int index) const {
    if (!isValidIndex(index))
        return QString();
    const Room &r = m_rooms.at(index);
    const QString base = arenaBase(r);
    if (base.isEmpty())
        return QString();
    QString url = base + QStringLiteral("#/") + percentEncode(urlSafeName(r.org)) + QLatin1Char('/')
                  + percentEncode(urlSafeName(r.room)) + QStringLiteral("/D1");
    const QString common = commonParams(r);
    if (!common.isEmpty())
        url += QLatin1Char('?') + common;
    return url;
}

QString VdoNinjaRoomsModel::returnFeedUrl(int index) const {
    QString url = directorUrl(index);
    if (url.isEmpty())
        return url;
    return url.replace(QStringLiteral("/D1"), QStringLiteral("/R1"));
}

// --- VdoNinjaSlotsModel ---

VdoNinjaSlotsModel::VdoNinjaSlotsModel(QObject *parent)
    : QAbstractListModel(parent) {
}

int VdoNinjaSlotsModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;
    return m_rows.size();
}

QVariant VdoNinjaSlotsModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || !checkIndex(index))
        return QVariant();

    const Row &row = m_rows.at(index.row());
    switch (role) {
    case slotRole:
        return row.slot;
    case kindRole:
        return row.kind;
    case streamIdRole:
        return row.streamId;
    case whepUrlRole:
        return row.whepUrl;
    case callerUrlRole:
        return row.callerUrl;
    case onlineRole:
        return row.online;
    case layerTitleRole:
        return row.layerTitle;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> VdoNinjaSlotsModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[slotRole] = "slot";
    roles[kindRole] = "kind";
    roles[streamIdRole] = "streamId";
    roles[whepUrlRole] = "whepUrl";
    roles[callerUrlRole] = "callerUrl";
    roles[onlineRole] = "online";
    roles[layerTitleRole] = "layerTitle";
    return roles;
}

void VdoNinjaSlotsModel::setRoomsModel(VdoNinjaRoomsModel *rooms) {
    m_rooms = rooms;
}

void VdoNinjaSlotsModel::setRoom(int roomIndex, const QString &mediaMtxWhepBase) {
    beginResetModel();
    m_rows.clear();
    m_roomIndex = -1;
    if (m_rooms && roomIndex >= 0 && roomIndex < m_rooms->getNumberOfRooms()) {
        m_roomIndex = roomIndex;
        const QVariantMap room = m_rooms->roomAt(roomIndex);
        const QString roomId = room.value(QStringLiteral("roomId")).toString();
        const QString title = room.value(QStringLiteral("title")).toString();
        const int slots = room.value(QStringLiteral("slots")).toInt();
        for (int n = 1; n <= slots; n++) {
            const QString slot = QStringLiteral("P%1").arg(n);
            for (const QString kind : { QStringLiteral("camera"), QStringLiteral("screen") }) {
                Row row;
                row.slot = slot;
                row.kind = kind;
                row.streamId = VdoNinjaRoomsModel::streamIdFor(roomId, slot, kind);
                row.whepUrl = m_rooms->whepUrl(roomIndex, slot, kind, mediaMtxWhepBase);
                row.callerUrl = m_rooms->callerUrl(roomIndex, slot, QStringLiteral("both"));
                row.layerTitle = (title.isEmpty() ? QString() : title + QLatin1Char(' '))
                                 + QStringLiteral("Slot-%1 ").arg(n) + capitalized(kind);
                m_rows.append(row);
            }
        }
    }
    endResetModel();
    Q_EMIT slotsChanged();
}

void VdoNinjaSlotsModel::setOnlinePaths(const QStringList &onlinePaths) {
    for (Row &row : m_rows)
        row.online = onlinePaths.contains(VdoNinjaRoomsModel::mediaMtxPathFor(row.streamId)) ? 1 : 0;
    if (!m_rows.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(m_rows.size() - 1, 0), { onlineRole });
    Q_EMIT slotsChanged();
}

void VdoNinjaSlotsModel::clearOnline() {
    for (Row &row : m_rows)
        row.online = -1;
    if (!m_rows.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(m_rows.size() - 1, 0), { onlineRole });
    Q_EMIT slotsChanged();
}

QVariantMap VdoNinjaSlotsModel::rowAt(int index) const {
    QVariantMap map;
    if (index < 0 || index >= m_rows.size())
        return map;
    const Row &row = m_rows.at(index);
    map.insert(QStringLiteral("slot"), row.slot);
    map.insert(QStringLiteral("kind"), row.kind);
    map.insert(QStringLiteral("streamId"), row.streamId);
    map.insert(QStringLiteral("whepUrl"), row.whepUrl);
    map.insert(QStringLiteral("callerUrl"), row.callerUrl);
    map.insert(QStringLiteral("online"), row.online);
    map.insert(QStringLiteral("layerTitle"), row.layerTitle);
    return map;
}

int VdoNinjaSlotsModel::getNumberOfStreams() const {
    return m_rows.size();
}

int VdoNinjaSlotsModel::roomIndex() const {
    return m_roomIndex;
}

int VdoNinjaSlotsModel::onlineCount() const {
    int n = 0;
    for (const Row &row : m_rows)
        n += (row.online == 1) ? 1 : 0;
    return n;
}
