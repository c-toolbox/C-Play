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
    // 1. Explicit setting, if it points at an existing file.
    const QString configured = PlaybackSettings::ytdlpPath();
    if (!configured.isEmpty()) {
        // The setting may be a bare name (searched on PATH) or a path to the executable.
        if (QFileInfo::exists(configured))
            return configured.toStdString();
        const QString onPath = QStandardPaths::findExecutable(configured);
        if (!onPath.isEmpty())
            return onPath.toStdString();
        sgct::Log::Warning(std::format("YtdlpResolver: configured yt-dlp path '{}' not found, falling back to auto-detection.",
                                       configured.toStdString()));
    }

    // 2. Next to C-Play.exe.
    const QString appDir = QCoreApplication::applicationDirPath();
    for (const QString& name : {QStringLiteral("yt-dlp.exe"), QStringLiteral("yt-dlp")}) {
        const QString candidate = appDir + QStringLiteral("/") + name;
        if (QFileInfo::exists(candidate))
            return candidate.toStdString();
    }

    // 3. On PATH.
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
    if (!onPath.isEmpty())
        return onPath.toStdString();

    // Not found: return empty and let ytdl_hook.lua do its own PATH search (and report the error).
    return std::string();
}

} // namespace YtdlpResolver
