/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "autosave.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

namespace Autosave {

namespace {
// Between the document name and the hash of the document's path in an autosave file name.
const QString nameSuffix = QStringLiteral("_autosave_");
}

QString folder() {
    // Next to C-Play/cplay.conf, see Application::configFolderPath().
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/C-Play/autosave");
}

bool isAutosave(const QString &path) {
    if (path.isEmpty())
        return false;
    QString p = path;
    p.replace(QStringLiteral("file:///"), QStringLiteral(""));
    return QFileInfo(p).absoluteDir() == QDir(folder());
}

QString filePathFor(const QString &sourcePath, const QString &fallbackName, const QString &ext) {
    if (isAutosave(sourcePath) && QFileInfo(sourcePath).suffix() == ext)
        return QFileInfo(sourcePath).absoluteFilePath();

    QString name = sourcePath.isEmpty() ? fallbackName : QFileInfo(sourcePath).completeBaseName();
    name.replace(QRegularExpression(QStringLiteral("[^\\w\\-]+")), QStringLiteral("_"));
    if (name.isEmpty())
        name = QStringLiteral("Untitled");

    // Documents with the same name in different folders get their own autosave.
    const QString key = sourcePath.isEmpty() ? fallbackName : QFileInfo(sourcePath).absoluteFilePath();
    const QString hash = QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1).toHex().left(8));

    return folder() + QStringLiteral("/") + name + nameSuffix + hash + QStringLiteral(".") + ext;
}

bool write(const QString &path, const QJsonDocument &doc) {
    if (!QDir().mkpath(folder())) {
        qDebug() << QStringLiteral("Failed to create autosave folder: ") << folder();
        return false;
    }

    // QSaveFile, so a crash while writing leaves the previous autosave intact.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        qDebug() << QStringLiteral("Failed to open autosave for writing: ") << path;
        return false;
    }
    file.write(doc.toJson());
    if (!file.commit()) {
        qDebug() << QStringLiteral("Failed to write autosave: ") << path;
        return false;
    }

    const QFileInfoList all = files(QFileInfo(path).suffix());
    for (int i = maxFilesPerType; i < all.size(); i++)
        QFile::remove(all[i].absoluteFilePath());

    return true;
}

QFileInfoList files(const QString &ext) {
    QDir dir(folder());
    return dir.entryInfoList({QStringLiteral("*.") + ext}, QDir::Files, QDir::Time);
}

QString displayName(const QFileInfo &file) {
    const QString name = file.completeBaseName();
    const int idx = name.lastIndexOf(nameSuffix);
    return idx > 0 ? name.left(idx) : name;
}

void removeAll() {
    for (const QString &ext : {QStringLiteral("cplaypres"), QStringLiteral("cplaylist"), QStringLiteral("cplayfile")}) {
        for (const QFileInfo &file : files(ext))
            QFile::remove(file.absoluteFilePath());
    }
}

}
