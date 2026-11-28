/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "pathutils.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>

namespace PathUtils {

namespace {

QString legacyDecodedPath(const QString &path) {
    if (!path.contains(QLatin1Char('%')) || QUrl(path).scheme().size() > 1)
        return QString();

    // Old QML saved file URLs with the scheme stripped. Directory separators
    // and dots were left literal. Reject escapes that could change path components.
#ifdef Q_OS_WIN
    static const QRegularExpression invalidEscape(QStringLiteral("%(?:00|2e|2f|5c)"),
                                                  QRegularExpression::CaseInsensitiveOption);
#else
    static const QRegularExpression invalidEscape(QStringLiteral("%(?:00|2e|2f)"),
                                                  QRegularExpression::CaseInsensitiveOption);
#endif
    if (invalidEscape.match(path).hasMatch())
        return QString();

    // A path can have an escaped filename under a directory with a literal '%'.
    static const QRegularExpression literalPercent(QStringLiteral("%(?![0-9a-f]{2})"),
                                                    QRegularExpression::CaseInsensitiveOption);
    QString encodedPath = path;
    encodedPath.replace(literalPercent, QStringLiteral("%25"));
    QStringDecoder decoder(QStringDecoder::Utf8,
                           QStringDecoder::Flag::Stateless | QStringDecoder::Flag::ConvertInitialBom);
    const QString decoded = decoder(QByteArray::fromPercentEncoding(encodedPath.toUtf8()));
    return decoder.hasError() || decoded == path ? QString() : decoded;
}

QString findExistingPath(const QString &filePath, const QStringList &searchPaths,
                         QStringList *legacyLocations = nullptr) {
    const QFileInfo fileInfo(filePath);
    if (fileInfo.exists())
        return filePath;
    if (!fileInfo.isRelative())
        return QString();

    for (const QString &searchPath : searchPaths) {
        // An unset location is empty, which would search the root of the current drive.
        const QString base = toLocalPath(searchPath);
        if (base.isEmpty())
            continue;
        const QString candidate = QDir::cleanPath(base + QLatin1Char('/') + filePath);
        if (QFileInfo::exists(candidate))
            return candidate;
        if (legacyLocations && base == searchPath) {
            const QString decodedBase = legacyDecodedPath(base);
            if (!decodedBase.isEmpty() && !QFileInfo::exists(base) && QFileInfo(decodedBase).isDir())
                legacyLocations->append(decodedBase);
        }
    }
    return QString();
}

} // namespace

QString toLocalPath(const QString &pathOrUrl) {
    if (!pathOrUrl.startsWith(QLatin1String("file:"), Qt::CaseInsensitive))
        return pathOrUrl;

    const QUrl url(pathOrUrl);
    if (!url.isValid() || !url.isLocalFile())
        return pathOrUrl;

    QString path = url.toLocalFile();
    // Qt encodes '#' and '?' in file names, so a query or fragment only comes from a
    // hand-written URL with the literal character: it is part of the file name.
    if (url.hasQuery())
        path += QLatin1Char('?') + url.query(QUrl::FullyDecoded);
    if (url.hasFragment())
        path += QLatin1Char('#') + url.fragment(QUrl::FullyDecoded);
    return path;
}

QUrl toFileUrl(const QString &pathOrUrl) {
    if (pathOrUrl.isEmpty())
        return QUrl();
    return QUrl::fromLocalFile(toLocalPath(pathOrUrl));
}

QString checkAndCorrectPath(const QString &pathOrUrl, const QStringList &searchPaths) {
    const QString filePath = toLocalPath(pathOrUrl);
    if (filePath.isEmpty())
        return QString();

    QStringList legacyLocations;
    const QString literal = findExistingPath(filePath, searchPaths, &legacyLocations);
    if (!literal.isEmpty())
        return literal;
    if (!legacyLocations.isEmpty()) {
        const QString found = findExistingPath(filePath, legacyLocations);
        if (!found.isEmpty())
            return found;
    }

    if (filePath != pathOrUrl)
        return QString();
    const QString decoded = legacyDecodedPath(filePath);
    if (decoded.isEmpty())
        return QString();
    return findExistingPath(decoded, searchPaths + legacyLocations);
}

QString relativeTo(const QString &filePath, const QString &location) {
#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity cs = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity cs = Qt::CaseSensitive;
#endif
    const QString path = QDir::cleanPath(toLocalPath(filePath));
    const QString base = QDir::cleanPath(toLocalPath(location));
    if (base.isEmpty() || !path.startsWith(base, cs))
        return QString();
    // "D:/Media" must not claim "D:/MediaOld/x.mp4".
    if (path.size() > base.size() && path.at(base.size()) != QLatin1Char('/') && !base.endsWith(QLatin1Char('/')))
        return QString();
    return QDir(base).relativeFilePath(path);
}

QString makePathRelativeTo(const QString &filePath, const QStringList &pathsToConsider) {
    for (const QString &location : pathsToConsider) {
        const QString relative = relativeTo(filePath, location);
        if (!relative.isEmpty())
            return relative;
    }
    return toLocalPath(filePath);
}

} // namespace PathUtils
