/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QString>
#include <QStringList>
#include <QUrl>

// Conversions between local paths and file URLs, and the lookup of media paths that
// .cplayfile, playlist and presentation files store relative to the C-Play locations.
//
// QML hands a url to a QString parameter as QUrl::toString(): "file:///C:/Show %231/a.mp4",
// "file://server/share/a.mp4" or "file:///home/a.mp4". Characters such as '#', '%' and
// brackets stay percent-encoded and a network share keeps its host, so removing "file:///"
// from that string does not give the path back. Use toLocalPath() instead.
namespace PathUtils {

// The local path for a file URL; any other string (a plain, relative, UNC or Linux path,
// or a non-file URL such as a stream) is returned unchanged.
QString toLocalPath(const QString &pathOrUrl);

// A file URL for a local path or file URL; an empty URL for an empty string.
QUrl toFileUrl(const QString &pathOrUrl);

// The path itself when it exists; for a relative path, the first existing match below one of
// searchPaths (empty entries are skipped); otherwise an empty string. Accepts file URLs, also
// as search paths. For older saved paths, tries percent-decoding after literal lookup fails.
QString checkAndCorrectPath(const QString &pathOrUrl, const QStringList &searchPaths);

// filePath relative to location when filePath lies below it, otherwise an empty string.
// Separators and dot segments are normalized before comparing. Windows paths compare case-insensitively.
QString relativeTo(const QString &filePath, const QString &location);

// filePath relative to the first of pathsToConsider it lies below; otherwise filePath as a
// local path. Empty locations are skipped.
QString makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider);

} // namespace PathUtils

#endif // PATHUTILS_H
