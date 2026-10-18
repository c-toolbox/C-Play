/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef AUTOSAVE_H
#define AUTOSAVE_H

#include <QFileInfoList>
#include <QJsonDocument>
#include <QString>

// Autosaves of edited presentations (*.cplaypres), playlists (*.cplaylist) and C-Play files
// (*.cplayfile). They are kept in a folder next to the application settings, never on top of
// the file that is loaded, and are listed under File -> Open Autosaved.
namespace Autosave {

// Throttle between an edit and its autosave, so continuous edits write at most this often.
constexpr int intervalMs = 10000;
// Autosaves kept per file type, older ones are removed.
constexpr int maxFilesPerType = 10;

QString folder();
// True when path is a file in the autosave folder.
bool isAutosave(const QString &path);
// Autosave file of a document loaded from sourcePath (empty when it was never saved), named
// after fallbackName when there is no sourcePath. A document loaded from the autosave folder
// is autosaved onto itself.
QString filePathFor(const QString &sourcePath, const QString &fallbackName, const QString &ext);
// Writes doc to path (in the autosave folder) and removes the oldest autosaves of that type.
bool write(const QString &path, const QJsonDocument &doc);
// Autosaves with extension ext, newest first.
QFileInfoList files(const QString &ext);
// The document name of an autosave, without the suffix that tells autosaves apart.
QString displayName(const QFileInfo &file);
void removeAll();

}

#endif // AUTOSAVE_H
