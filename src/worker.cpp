/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "worker.h"
#include "_debug.h"
#include "application.h"

#include <KFileMetaData/ExtractorCollection>
#include <KFileMetaData/SimpleExtractionResult>
#include <QMimeDatabase>
#include <QThread>
#include <QUrl>

Worker *Worker::sm_worker = nullptr;

Worker *Worker::instance() {
    if (!sm_worker) {
        sm_worker = new Worker();
    }
    return sm_worker;
}

void Worker::getMetaData(int index, const QString &path) {
    auto url = QUrl::fromUserInput(path);
    if (url.scheme() != QStringLiteral("file")) {
        return;
    }
    QString mimeType = Application::mimeType(url);
    KFileMetaData::ExtractorCollection exCol;
    QList<KFileMetaData::Extractor *> extractors = exCol.fetchExtractors(mimeType);
    KFileMetaData::SimpleExtractionResult result(url.toLocalFile(), mimeType, KFileMetaData::ExtractionResult::ExtractMetaData);
    if (extractors.size() == 0) {
        return;
    }
    KFileMetaData::Extractor *ex = extractors.first();
    ex->extract(&result);

    auto properties = result.properties();

    Q_EMIT metaDataReady(index, properties);
}
