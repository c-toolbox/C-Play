/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "imagesequenceutils.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <limits>

int ImageSequenceUtils::trailingDigitCount(const QString &text)
{
    int count = 0;
    for (int i = text.size() - 1; i >= 0; --i) {
        if (!text.at(i).isDigit()) {
            break;
        }
        ++count;
    }
    return count;
}

bool ImageSequenceUtils::containsOnlyDigits(const QString &text)
{
    if (text.isEmpty()) {
        return false;
    }
    for (const QChar c : text) {
        if (!c.isDigit()) {
            return false;
        }
    }
    return true;
}

ImageSequenceScanResult ImageSequenceUtils::scanImageSequence(const QString &path)
{
    ImageSequenceScanResult result;
    const QFileInfo selected(path);
    if (!selected.exists() || !selected.isFile()) {
        result.message = QStringLiteral("Input file does not exist: %1").arg(path);
        return result;
    }

    const QString selectedStem = selected.completeBaseName();
    const int digitCount = trailingDigitCount(selectedStem);
    if (digitCount == 0) {
        result.message = QStringLiteral("Selected input is not a numbered image-sequence frame: %1").arg(selected.fileName());
        result.count = 1;
        return result;
    }

    const QString prefix = selectedStem.left(selectedStem.size() - digitCount);
    const QString selectedDigits = selectedStem.right(digitCount);
    bool selectedOk = false;
    result.selectedIndex = selectedDigits.toInt(&selectedOk);
    if (!selectedOk) {
        result.message = QStringLiteral("Failed to parse frame number from selected input: %1").arg(selected.fileName());
        return result;
    }

    const QString suffix = selected.suffix();
    const QStringList filters = suffix.isEmpty()
        ? QStringList{ QStringLiteral("*") }
        : QStringList{ QStringLiteral("*.%1").arg(suffix) };
    const QFileInfoList entries = QDir(selected.absolutePath()).entryInfoList(filters, QDir::Files, QDir::Name);

    int minIndex = std::numeric_limits<int>::max();
    int maxIndex = std::numeric_limits<int>::min();
    for (const QFileInfo &entry : entries) {
        if (entry.suffix().compare(suffix, Qt::CaseInsensitive) != 0) {
            continue;
        }

        const QString stem = entry.completeBaseName();
        if (!stem.startsWith(prefix)) {
            continue;
        }

        const QString digits = stem.mid(prefix.size());
        if (digits.size() != digitCount || !containsOnlyDigits(digits)) {
            continue;
        }

        bool ok = false;
        const int frameIndex = digits.toInt(&ok);
        if (!ok) {
            continue;
        }

        ++result.count;
        minIndex = std::min(minIndex, frameIndex);
        maxIndex = std::max(maxIndex, frameIndex);
    }

    if (result.count == 0) {
        result.message = QStringLiteral("No matching numbered image sequence files found for: %1").arg(selected.fileName());
        return result;
    }

    result.ok = true;
    result.firstIndex = minIndex;
    result.lastIndex = maxIndex;
    result.prefix = prefix;
    result.suffix = suffix;
    result.digitCount = digitCount;
    result.missingFrames = (1 + result.lastIndex - result.firstIndex) != result.count;
    return result;
}

QString ImageSequenceUtils::buildFramePath(const QString &directory, const QString &prefix,
                                           int digitCount, const QString &suffix, int frameIndex)
{
    const QString digits = QStringLiteral("%1").arg(frameIndex, digitCount, 10, QLatin1Char('0'));
    QString filename = prefix + digits;
    if (!suffix.isEmpty()) {
        filename += QLatin1Char('.') + suffix;
    }
    return QDir(directory).filePath(filename);
}

int ImageSequenceUtils::expectedFrameCount(int startIndex, int stopIndex, int steps)
{
    if (startIndex == stopIndex) {
        return 1;
    }
    const int step = std::max(1, steps);
    const int first = std::min(startIndex, stopIndex);
    const int last = std::max(startIndex, stopIndex);
    return ((last - first) / step) + 1;
}
