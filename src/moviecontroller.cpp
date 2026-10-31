/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "moviecontroller.h"
#include "audiosettings.h"
#include "mpvobject.h"
#include "playercontroller.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmodel.h"
#include "qthelper.h"
#ifdef MOVIE_PREVIEW_SUPPORT
#include "webrtc/moviepreviewstreamer.h"
#endif

#include "locationsettings.h"

#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QMutex>
#include <QSaveFile>
#include <algorithm>

namespace {
QJsonArray sectionsAsJson(const PlayListItem &item) {
    QJsonArray sections;
    for (int i = 0; i < item.numberOfSections(); ++i) {
        const auto &section = item.getSection(i);
        sections.append(QJsonObject{{QStringLiteral("index"), i},
            {QStringLiteral("title"), section.title},
            {QStringLiteral("start"), section.startTime},
            {QStringLiteral("end"), section.endTime},
            // 0 pause, 1 fade out, 2 continue, 3 next section, 4 loop.
            {QStringLiteral("endMode"), section.eosMode}});
    }
    return sections;
}

// The audio tracks (same ids and order as when C-Play loads the file) and duration of a file.
QJsonObject probeMedia(const QString &media, const QString &audio, bool audioFileAuto) {
    mpv_handle *mpv = mpv_create();
    if (!mpv)
        return {};
    const QList<QPair<const char *, const char *>> options{{"config", "no"}, {"terminal", "no"}, {"idle", "yes"},
        {"load-scripts", "no"}, {"ytdl", "no"}, {"vo", "null"}, {"ao", "null"}, {"pause", "yes"},
        {"track-auto-selection", "no"}, {"audio-file-auto", audioFileAuto ? "all" : "no"}};
    for (const auto &[name, value] : options)
        mpv_set_option_string(mpv, name, value);

    QJsonObject result;
    if (mpv_initialize(mpv) >= 0) {
        QStringList command{QStringLiteral("loadfile"), media, QStringLiteral("replace")};
#if MPV_CLIENT_API_VERSION >= MPV_MAKE_VERSION(2, 3)
        command << QStringLiteral("0");
#endif
        if (!audio.isEmpty())
            command << QStringLiteral("audio-file=") + audio;
        mpv::qt::command_async(mpv, command);

        QElapsedTimer elapsed;
        elapsed.start();
        constexpr qint64 timeoutMs = 8000;
        bool loaded = false, ended = false;
        while (!loaded && !ended && elapsed.elapsed() < timeoutMs) {
            const mpv_event *event = mpv_wait_event(mpv, double(timeoutMs - elapsed.elapsed()) / 1000.0);
            loaded = event->event_id == MPV_EVENT_FILE_LOADED;
            ended = event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN;
        }
        if (loaded) {
            QJsonArray rows;
            const QList<QVariant> tracks = mpv::qt::get_property(mpv, QStringLiteral("track-list")).toList();
            for (const QVariant &track : tracks) {
                const auto t = track.toMap();
                if (t[QStringLiteral("type")] != QStringLiteral("audio"))
                    continue;
                QString text;
                for (const auto *key : {"title", "lang", "codec"}) {
                    const QString value = t[QString::fromLatin1(key)].toString();
                    if (!value.isEmpty())
                        text += value + QStringLiteral(" ");
                }
                rows.append(QJsonObject{{QStringLiteral("index"), rows.size()},
                    {QStringLiteral("id"), t[QStringLiteral("id")].toLongLong()},
                    {QStringLiteral("text"), text}});
            }
            result = {{QStringLiteral("tracks"), rows},
                {QStringLiteral("duration"), mpv::qt::get_property(mpv, QStringLiteral("duration")).toDouble()}};
        }
    }
    mpv_terminate_destroy(mpv);
    return result;
}
}

MovieController::MovieController(PlayerController *player)
    : QObject(player),
      m_player(player) {
#ifdef MOVIE_PREVIEW_SUPPORT
    MoviePreviewStreamer::create();
#endif
}

void MovieController::setMpv(MpvObject *mpv) {
    disconnect(m_pendingCue);
    m_mpv = mpv;
}

QString MovieController::mediaFile(int itemIdx) const {
    if (!m_mpv)
        return {};
    if (itemIdx < 0) {
        PlaySectionsModel *sectionsModel = m_mpv->playSectionsModel();
        return sectionsModel && sectionsModel->currentEditItem() ? sectionsModel->currentEditItem()->mediaFile() : QString();
    }
    PlayListModel *playlist = m_mpv->playlistModel();
    if (!playlist || itemIdx >= playlist->getPlayListSize())
        return {};
    const QPointer<PlayListItem> item = playlist->getItem(itemIdx);
    return item ? item->mediaFile() : QString();
}

QJsonObject MovieController::state() const {
    if (!m_mpv)
        return {};

    PlayListModel *playlist = m_mpv->playlistModel();
    PlaySectionsModel *sectionsModel = m_mpv->playSectionsModel();
    const int playing = playlist ? playlist->getPlayingVideo() : -1;

    QJsonArray items;
    for (int i = 0; playlist && i < playlist->getPlayListSize(); ++i) {
        const QPointer<PlayListItem> item = playlist->getItem(i);
        if (!item)
            continue;
        const QModelIndex row = playlist->index(i, 0);
        QJsonObject entry{{QStringLiteral("index"), i},
            {QStringLiteral("title"), playlist->data(row, PlayListModel::TitleRole).toString()},
            {QStringLiteral("file"), QFileInfo(item->mediaFile()).fileName()},
            {QStringLiteral("duration"), item->duration()},
            {QStringLiteral("endMode"), item->eofMode()},
            {QStringLiteral("sections"), sectionsAsJson(*item)}};
        if (item->useListFileStartTime())
            entry.insert(QStringLiteral("listStart"), item->listFileStartTime());
        if (item->useListFileEndTime())
            entry.insert(QStringLiteral("listEnd"), item->listFileEndTime());
        items.append(entry);
    }

    // The loaded file, with its sections as edited in C-Play. Not always in the playlist.
    QJsonObject loaded;
    if (sectionsModel && sectionsModel->currentEditItem()) {
        const PlayListItem *item = sectionsModel->currentEditItem();
        loaded = {{QStringLiteral("title"), m_mpv->mediaTitle()},
            {QStringLiteral("file"), QFileInfo(item->mediaFile()).fileName()},
            {QStringLiteral("sections"), sectionsAsJson(*item)}};
    }

    return {{QStringLiteral("name"), playlist ? playlist->getPlayListName() : QString()},
        {QStringLiteral("playing"), playing},
        {QStringLiteral("section"), sectionsModel ? sectionsModel->getPlayingSection() : -1},
        {QStringLiteral("autoPlay"), m_mpv->autoPlay()},
        {QStringLiteral("loaded"), loaded.isEmpty() ? QJsonValue() : QJsonValue(loaded)},
        {QStringLiteral("items"), items}};
}

QJsonObject MovieController::mediaSource(int itemIdx) const {
    PlayListModel *playlist = m_mpv ? m_mpv->playlistModel() : nullptr;
    if (!playlist || itemIdx < 0 || itemIdx >= playlist->getPlayListSize())
        return {};
    const QPointer<PlayListItem> item = playlist->getItem(itemIdx);
    if (!item)
        return {};
    item->loadDetailsFromDisk();
    return {{QStringLiteral("media"), item->mediaFile()},
        {QStringLiteral("audio"), item->useListAudioFile() && !item->listAudioFile().isEmpty()
            ? item->listAudioFile() : item->separateAudioFile()},
        {QStringLiteral("audioFileAuto"), AudioSettings::loadAudioFileInVideoFolder()}};
}

QJsonObject MovieController::mediaInfo(const QJsonObject &source) {
    static QMutex mutex;
    static QHash<QString, QJsonObject> known;

    const QString media = source.value(QStringLiteral("media")).toString();
    const QString audio = source.value(QStringLiteral("audio")).toString();
    const bool audioFileAuto = source.value(QStringLiteral("audioFileAuto")).toBool();
    if (media.isEmpty())
        return {};
    const auto modified = [](const QString &file) { return QFileInfo(file).lastModified().toMSecsSinceEpoch(); };
    const QString key = QStringList{media, QString::number(modified(media)), audio, QString::number(modified(audio)),
        QString::number(audioFileAuto)}.join(QLatin1Char('|'));
    {
        QMutexLocker lock(&mutex);
        const auto it = known.constFind(key);
        if (it != known.cend())
            return *it;
    }
    const QJsonObject info = probeMedia(media, audio, audioFileAuto);
    if (!info.isEmpty()) {
        QMutexLocker lock(&mutex);
        known.insert(key, info);
    }
    return info;
}

bool MovieController::cue(int itemIdx, int sectionIdx, double position, int play, int audio) {
    if (!m_mpv || !m_player)
        return false;
    PlayListModel *playlist = m_mpv->playlistModel();
    if (itemIdx < -1 || (itemIdx >= 0 && (!playlist || itemIdx >= playlist->getPlayListSize() || !playlist->getItem(itemIdx))))
        return false;
    const bool load = itemIdx >= 0 && itemIdx != playlist->getPlayingVideo();
    // The sections of the item to go to, as the sections model has them once it is loaded.
    const int sections = load ? playlist->getItem(itemIdx)->numberOfSections()
        : (m_mpv->playSectionsModel() ? m_mpv->playSectionsModel()->getNumberOfSections() : 0);
    if (sectionIdx < -2 || sectionIdx >= std::max(sections, 0))
        return false;

    const auto apply = [this, sectionIdx, position, play, audio]() {
        if (!m_mpv || !m_player)
            return;
        if (audio >= 0) {
            // From mpv: the tracks model may still have the file before when this one has just loaded.
            const QList<QVariant> tracks = m_mpv->getProperty(QStringLiteral("track-list")).toList();
            const auto count = std::count_if(tracks.cbegin(), tracks.cend(), [](const QVariant &track) {
                return track.toMap()[QStringLiteral("type")] == QStringLiteral("audio");
            });
            if (audio < count)
                m_player->LoadFromAudioTracks(audio);
        }
        if (sectionIdx >= -1)
            m_player->LoadFromSections(sectionIdx);
        if (position >= 0)
            m_player->SetPosition(position);
        if (play == 1)
            m_player->Play();
        else if (play == 0)
            m_player->Pause();
    };

    disconnect(m_pendingCue);
    if (!load) {
        apply();
        return true;
    }
    m_player->LoadFromPlaylist(itemIdx);
    // The position and section take effect once mpv has the file (and C-Play its sections).
    m_pendingCue = connect(m_mpv, &MpvObject::fileLoaded, this, apply, Qt::SingleShotConnection);
    return true;
}

QString MovieController::cueListFolder() const {
    if (!LocationSettings::cueListLocation().isEmpty())
        return LocationSettings::cueListLocation();
    PlayListModel *playlist = m_mpv ? m_mpv->playlistModel() : nullptr;
    if (playlist && !playlist->getPlayListPath().isEmpty())
        return QFileInfo(playlist->getPlayListPath()).absolutePath();
    return LocationSettings::cPlayFileLocation();
}

QString MovieController::cueListPath(const QString &name) const {
    QString fileName = name.trimmed();
    if (fileName.endsWith(QStringLiteral(".cplaycue"), Qt::CaseInsensitive))
        fileName.chop(9);
    static const QString forbidden = QStringLiteral("/\\:*?\"<>|");
    const bool plain = !fileName.isEmpty() && fileName.size() <= 120 && !fileName.startsWith(QLatin1Char('.'))
        && std::none_of(fileName.cbegin(), fileName.cend(), [](QChar c) { return c.unicode() < 32 || forbidden.contains(c); });
    const QString folder = cueListFolder();
    if (!plain || folder.isEmpty())
        return {};
    return QDir(folder).absoluteFilePath(fileName + QStringLiteral(".cplaycue"));
}

QJsonObject MovieController::cueLists() const {
    const QString folder = cueListFolder();
    QJsonArray files;
    if (!folder.isEmpty()) {
        const auto entries = QDir(folder).entryInfoList({QStringLiteral("*.cplaycue")}, QDir::Files, QDir::Time);
        for (const QFileInfo &entry : entries)
            files.append(QJsonObject{{QStringLiteral("name"), entry.completeBaseName()},
                {QStringLiteral("modified"), entry.lastModified().toString(Qt::ISODate)}});
    }
    PlayListModel *playlist = m_mpv ? m_mpv->playlistModel() : nullptr;
    return {{QStringLiteral("folder"), folder},
        {QStringLiteral("playlist"), playlist ? playlist->getPlayListName() : QString()},
        {QStringLiteral("files"), files}};
}

QJsonObject MovieController::loadCueList(const QString &name, QString *error, int *status) const {
    const QString path = cueListPath(name);
    if (path.isEmpty()) {
        *status = cueListFolder().isEmpty() ? 409 : 400;
        *error = *status == 409 ? QStringLiteral("No folder for cue lists: open a saved playlist or set the C-Play file location")
                                : QStringLiteral("Invalid cue list name");
        return {};
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        *status = 404;
        *error = QStringLiteral("No cue list ") + QFileInfo(path).fileName();
        return {};
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject() || !doc.object().value(QStringLiteral("cues")).isArray()) {
        *status = 422;
        *error = QFileInfo(path).fileName() + QStringLiteral(" is not a cue list");
        return {};
    }
    return doc.object();
}

QString MovieController::saveCueList(const QString &name, const QJsonObject &cueList, int *status) const {
    const QString path = cueListPath(name);
    if (path.isEmpty()) {
        *status = cueListFolder().isEmpty() ? 409 : 400;
        return *status == 409 ? QStringLiteral("No folder for cue lists: open a saved playlist or set the C-Play file location")
                              : QStringLiteral("Invalid cue list name");
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(QJsonDocument(cueList).toJson()) < 0 || !file.commit()) {
        *status = 500;
        return QStringLiteral("Could not write ") + path;
    }
    return {};
}
