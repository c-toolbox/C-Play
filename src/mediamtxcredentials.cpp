/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "mediamtxcredentials.h"

#if defined(Q_OS_WIN)

#include <windows.h>
#include <wincred.h>

// The generic-credential type constant was renamed between Windows SDK generations:
// the CredGeneric enum value in older SDKs, CRED_TYPE_GENERIC in newer ones. Both are 1.
#ifndef CRED_TYPE_GENERIC
#define CRED_TYPE_GENERIC 1 // == CredGeneric
#endif

namespace MediaMtxCredentials {

namespace {

// Credential blobs are not consistently encoded: cmdkey and CredWriteW on modern Windows
// store the password as UTF-16LE, while older tools write plain ANSI/UTF-8. A real text
// password never contains NUL bytes in an 8-bit encoding, so their presence marks a
// little-endian 16-bit blob.
QString decodeCredentialBlob(const void *blob, DWORD size) {
    if (blob == nullptr || size == 0)
        return QString();

    const auto *bytes = static_cast<const unsigned char *>(blob);
    for (DWORD i = 0; i < size; ++i) {
        if (bytes[i] == 0 && size % 2 == 0)
            return QString::fromUtf16(reinterpret_cast<const char16_t *>(blob), int(size / 2));
    }
    return QString::fromUtf8(reinterpret_cast<const char *>(blob), int(size));
}

} // namespace

bool findStoredPassword(const QString &serverName, const QString &username, QString &passwordOut) {
    if (serverName.isEmpty() || username.isEmpty())
        return false;

    // The target name is the only part of a generic credential used for matching, so it
    // carries both the server's friendly name and the API user.
    const QString target = QStringLiteral("MediaMTX/%1/%2").arg(serverName, username);

    CREDENTIALW *credential = nullptr;
    // QChar is wchar_t on Windows, so the internal UTF-16 data can be passed straight through.
    if (!CredReadW(reinterpret_cast<LPCWSTR>(target.utf16()), CRED_TYPE_GENERIC, 0, &credential))
        return false; // ERROR_NOT_FOUND or the Credential Manager is unavailable

    passwordOut = decodeCredentialBlob(credential->CredentialBlob, credential->CredentialBlobSize);
    CredFree(credential);
    return !passwordOut.isEmpty();
}

} // namespace MediaMtxCredentials

#else

namespace MediaMtxCredentials {

bool findStoredPassword(const QString &serverName, const QString &username, QString &passwordOut) {
    Q_UNUSED(serverName);
    Q_UNUSED(username);
    passwordOut.clear();
    return false; // the Windows Credential Manager is only available on Windows
}

} // namespace MediaMtxCredentials

#endif
