/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ytdlpresolver.h"

#include "playbacksettings.h"

#include <sgct/sgct.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>

#include <cctype>

namespace {
// Same host set as ytdl_hook.lua's default "include" option: any host containing
// "youtube" or "youtu.be" (covers youtube.com, m.youtube.com, music.youtube.com, youtu.be).
const QRegularExpression& youtubeHostPattern() {
    static const QRegularExpression re(QStringLiteral("^(https?://)?(www\\.)?([a-z0-9-]+\\.)*(youtube|youtu\\.be)",
                                                       QRegularExpression::CaseInsensitiveOption));
    return re;
}

// Locate the yt-dlp executable inside dir: first "yt-dlp(.exe)" directly in dir, then - for a
// PyInstaller onedir build (a folder named "yt-dlp" containing yt-dlp.exe + _internal/) - the
// executable inside that folder. Only real files are accepted: QFileInfo::exists() would also
// match the "yt-dlp" folder itself, which is not executable and must never be handed to mpv's
// ytdl_hook (that made a plugins/yt-dlp/ onedir install fail silently). yt-dlp locates its
// _internal scripts relative to the executable itself, so pointing at that file works no matter
// what working directory mpv runs with. Returns an empty string when nothing usable is found.
QString findYtdlpInDir(const QString& dir) {
    if (dir.isEmpty() || !QFileInfo(dir).isDir())
        return {};
    const QDir d(dir);
    for (const QString& name : {QStringLiteral("yt-dlp.exe"), QStringLiteral("yt-dlp")}) {
        const QFileInfo fi(d.filePath(name));
        if (fi.isFile())
            return fi.absoluteFilePath();
    }
    const QFileInfo sub(d.filePath(QStringLiteral("yt-dlp")));
    if (sub.isDir()) {
        for (const QString& name : {QStringLiteral("yt-dlp.exe"), QStringLiteral("yt-dlp")}) {
            const QFileInfo fi(sub.absoluteFilePath() + QStringLiteral("/") + name);
            if (fi.isFile())
                return fi.absoluteFilePath();
        }
    }
    return {};
}
} // namespace

namespace YtdlpResolver {

bool isYouTubeUrl(const std::string& path) {
    if (path.empty())
        return false;
    // Quick reject: anything without "youtube" or "youtu" can never match (avoids regex cost
    // for the common case of plain file paths / rtsp urls on every layer update).
    const std::string lower = [&path]() {
        std::string s = path;
        for (char& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }();
    if (lower.find("youtube") == std::string::npos && lower.find("youtu.be") == std::string::npos)
        return false;
    return youtubeHostPattern().match(QString::fromStdString(path)).hasMatch();
}

std::string resolveYtdlpPath() {
    // 1. Explicit setting, if it points at an existing file (or a folder containing one).
    const QString configured = PlaybackSettings::ytdlpPath();
    if (!configured.isEmpty()) {
        QFileInfo fi(configured);
        if (fi.isFile())
            return fi.absoluteFilePath().toStdString();
        // A folder may be given too (e.g. a PyInstaller onedir build: yt-dlp/yt-dlp.exe + _internal).
        const QString inFolder = findYtdlpInDir(configured);
        if (!inFolder.isEmpty())
            return inFolder.toStdString();
        // The setting may also be a bare name (searched on PATH).
        const QString onPath = QStandardPaths::findExecutable(configured);
        if (!onPath.isEmpty())
            return onPath.toStdString();
        sgct::Log::Warning(std::format("YtdlpResolver: configured yt-dlp path '{}' not found, falling back to auto-detection.",
                                       configured.toStdString()));
    }

    // 2. Next to C-Play.exe - as a file, or inside a "yt-dlp" folder (onedir build).
    const QString appDir = QCoreApplication::applicationDirPath();
    {
        const QString found = findYtdlpInDir(appDir);
        if (!found.isEmpty())
            return found.toStdString();
    }

    // 3. In the plugins folder of the working directory / application directory, as a file or
    // inside a "yt-dlp" subfolder (onedir build). The process working directory is used when
    // C-Play is started from its install folder; the application directory is checked as well,
    // since a GUI app launched from Explorer, the Start menu or a shortcut may have a different
    // current directory.
    for (const QString& dir : {QDir::currentPath(), appDir}) {
        const QString found = findYtdlpInDir(dir + QStringLiteral("/plugins"));
        if (!found.isEmpty())
            return found.toStdString();
    }

    // 4. On PATH.
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
    if (!onPath.isEmpty())
        return onPath.toStdString();

    // Not found: return empty and let ytdl_hook.lua do its own PATH search (and report the error).
    return std::string();
}

} // namespace YtdlpResolver
