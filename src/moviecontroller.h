/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MOVIECONTROLLER_H
#define MOVIECONTROLLER_H

#include <QJsonObject>
#include <QObject>
#include <QPointer>

class MpvObject;
class PlayerController;

// Backs the movie view (web/movie_mode), served by the HTTP server at /movie/: the playlist and
// sections state it shows, and cues that load a playlist item at a section or position in one
// step. Lives on the GUI thread; the HTTP server calls it through queued calls.
class MovieController : public QObject {
    Q_OBJECT

public:
    explicit MovieController(PlayerController *player);

    void setMpv(MpvObject *mpv);

    // The playlist, its sections and what plays, sampled into the "movie" event topic. The
    // position is left out, it has its own topic.
    QJsonObject state() const;

    // Loads playlist item itemIdx unless it is the loaded one (-1 keeps the loaded file), then
    // goes to section sectionIdx (-1 leaves the playing section, -2 keeps it) and to position
    // (seconds in the file, < 0 keeps it; applied after the section). play: 1 plays, 0 pauses,
    // -1 leaves it to C-Play (auto play). A new item is positioned once it is loaded.
    // Returns false for invalid indices.
    bool cue(int itemIdx, int sectionIdx, double position, int play);

    // The media file of playlist item itemIdx, or of the loaded file for -1, as mpv opens it.
    // Empty when there is none.
    QString mediaFile(int itemIdx) const;

private:
    QPointer<PlayerController> m_player;
    QPointer<MpvObject> m_mpv;
    // A cue waiting for its file to load, replaced by the next cue.
    QMetaObject::Connection m_pendingCue;
};

#endif // MOVIECONTROLLER_H
