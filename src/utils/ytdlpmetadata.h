/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef YTDLPMETADATA_H
#define YTDLPMETADATA_H

#include <QVariantList>
#include <atomic>
#include <mutex>
#include <thread>

class QProcess;
class QObject;

// Fetches machine-readable yt-dlp metadata for a stream URL (YouTube and other supported
// sites) WITHOUT starting playback, so the Add Layer dialog can pre-fill the layer name from
// the video title as soon as a YouTube URL is pasted.
//
// Runs "<yt-dlp> --no-warnings -J --no-playlist <url>" on a worker thread using the same
// executable discovery as YtdlpResolver (so what we inspect here is exactly what mpv's
// ytdl_hook will use for playback). The JSON output yields:
//   - title:          the video title, used to pre-fill the layer name in the Add Layer dialog
//   - playbackFormat: what mpv's ytdl_hook will actually play - either the configured Settings
//                     -> Playback yt-dlp format selector or, when that setting is empty, a
//                     description of the default "bv*+ba/b" best-video + best-audio combination
//                     resolved from the formats list. Shown in the Add Layer dialog status line
//                     and logged on success.
//   - videoFormats / audioTracks / subtitles: reduced lists of {id, label} entries (kept for
//     a future quality/audio/subtitle selection UI; not shown in the UI yet)
class YtdlpMetadataModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY dataChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY dataChanged)
    Q_PROPERTY(QString title READ title NOTIFY dataChanged)
    Q_PROPERTY(QString playbackFormat READ playbackFormat NOTIFY dataChanged)
    Q_PROPERTY(QVariantList videoFormats READ videoFormats NOTIFY dataChanged)
    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY dataChanged)
    Q_PROPERTY(QVariantList subtitles READ subtitles NOTIFY dataChanged)

public:
    explicit YtdlpMetadataModel(QObject* parent = nullptr);
    ~YtdlpMetadataModel() override;

    bool isLoading() const;
    QString errorText() const;
    QString title() const;
    QString playbackFormat() const;
    QVariantList videoFormats() const;
    QVariantList audioTracks() const;
    QVariantList subtitles() const;

    // Start (or restart) a metadata fetch for url. Any in-flight fetch is cancelled first.
    Q_INVOKABLE void fetch(const QString& url);
    // Cancel any in-flight fetch and clear all state.
    Q_INVOKABLE void clear();

Q_SIGNALS:
    // Emitted on the main thread whenever any of the properties above changed (fetch started,
    // finished, failed, or cleared). QML rebuilds its combo-box models from this signal.
    void dataChanged();

private:
    struct FetchState {
        QProcess* process = nullptr;
        std::atomic<bool> cancelled{false};
    };

    void runFetch(const QString& ytdlpPath, const QString& url, const QString& configuredFormat);

    mutable std::mutex m_mutex; // guards all state below and the in-flight fetch
    bool m_isLoading = false;
    QString m_errorText;
    QString m_title;
    QString m_playbackFormat; // what mpv's ytdl_hook will play (see class comment)
    QVariantList m_videoFormats;
    QVariantList m_audioTracks;
    QVariantList m_subtitles;
    FetchState* m_activeFetch = nullptr; // owned by the worker thread until it finishes
    std::thread m_worker;                // always started/joined on the main (QML) thread
};

#endif // YTDLPMETADATA_H
