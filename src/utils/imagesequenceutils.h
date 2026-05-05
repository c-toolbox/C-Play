/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef IMAGESEQUENCEUTILS_H
#define IMAGESEQUENCEUTILS_H

#include <QString>

struct ImageSequenceScanResult {
    bool ok = false;
    bool missingFrames = false;
    int count = 0;
    int firstIndex = 0;
    int selectedIndex = 0;
    int lastIndex = 0;
    QString prefix;
    QString suffix;
    int digitCount = 0;
    QString message;
};

class ImageSequenceUtils {
public:
    /**
     * Scan the directory of the given file path for numbered image sequence frames.
     * Returns information about the sequence (first/last index, count, etc.).
     */
    static ImageSequenceScanResult scanImageSequence(const QString &path);

    /**
     * Build a file path for a specific frame index given the sequence parameters.
     * E.g. prefix="img_", digitCount=4, suffix="png", frameIndex=42 -> "img_0042.png"
     */
    static QString buildFramePath(const QString &directory, const QString &prefix,
                                  int digitCount, const QString &suffix, int frameIndex);

    /**
     * Count the number of trailing digits in the given text.
     */
    static int trailingDigitCount(const QString &text);

    /**
     * Check if the text contains only digit characters.
     */
    static bool containsOnlyDigits(const QString &text);

    /**
     * Calculate the expected frame count for a sequence with given parameters.
     */
    static int expectedFrameCount(int startIndex, int stopIndex, int steps);
};

#endif // IMAGESEQUENCEUTILS_H
