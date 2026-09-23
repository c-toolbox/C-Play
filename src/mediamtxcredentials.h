/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MEDIAMTXCREDENTIALS_H
#define MEDIAMTXCREDENTIALS_H

#include <QString>

// Looks up MediaMTX server credentials in the Windows Credential Manager.
//
// Credentials are stored as generic entries whose target name is built from the
// server's friendly name and API user: "MediaMTX/<serverName>/<username>". The
// password lives in the credential blob (UTF-8). On non-Windows platforms the
// lookup always fails so callers fall back to a manually entered password.
namespace MediaMtxCredentials {

// Returns true when a stored credential exists for this server/user pair and fills
// `passwordOut` with it. An empty name or username never matches anything.
bool findStoredPassword(const QString &serverName, const QString &username, QString &passwordOut);

} // namespace MediaMtxCredentials

#endif // MEDIAMTXCREDENTIALS_H
