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

#include <QFileInfo>
#include <QJsonArray>
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
