/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "moviecontroller.h"
#include "mpvobject.h"
#include "playercontroller.h"
#include "playlist/playlistitem.h"
#include "playlist/playlistmodel.h"
#ifdef MOVIE_PREVIEW_SUPPORT
#include "webrtc/moviepreviewstreamer.h"
#endif

#include "locationsettings.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
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
        items.append(QJsonObject{{QStringLiteral("index"), i},
            {QStringLiteral("title"), playlist->data(row, PlayListModel::TitleRole).toString()},
            {QStringLiteral("file"), QFileInfo(item->mediaFile()).fileName()},
            {QStringLiteral("duration"), item->duration()},
            {QStringLiteral("endMode"), item->eofMode()},
            {QStringLiteral("sections"), sectionsAsJson(*item)}});
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

bool MovieController::cue(int itemIdx, int sectionIdx, double position, int play) {
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

    const auto apply = [this, sectionIdx, position, play]() {
        if (!m_mpv || !m_player)
            return;
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
