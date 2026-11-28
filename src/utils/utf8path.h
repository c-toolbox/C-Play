/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UTF8PATH_H
#define UTF8PATH_H

#include <filesystem>
#include <string>

// C-Play keeps file paths as UTF-8 in std::string (QString::toStdString(), SyncHelper).
// On Windows, std::filesystem::path(std::string), std::ifstream(std::string) and
// path::string() use the ANSI code page instead: they miss files whose names hold
// non-ASCII characters, and path::string() throws for characters outside the code page.
// Convert through these helpers whenever a UTF-8 path meets the standard library.

inline std::filesystem::path utf8Path(const std::string &utf8) {
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

inline std::string pathToUtf8(const std::filesystem::path &path) {
    const std::u8string u8 = path.u8string();
    return std::string(u8.begin(), u8.end());
}

#endif // UTF8PATH_H
