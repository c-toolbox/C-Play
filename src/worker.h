/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 * 2020 George Florea Bănuș <georgefb899@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef WORKER_H
#define WORKER_H

#include <KFileMetaData/Properties>
#include <QObject>

class Worker : public QObject {
    Q_OBJECT
public:
    Worker() = default;
    ~Worker() = default;

    static Worker *instance();

    Q_INVOKABLE void getMetaData(int index, const QString &path);

Q_SIGNALS:
    void metaDataReady(int index, KFileMetaData::PropertyMultiMap metadata);

private:
    static Worker *sm_worker;
};

#endif // WORKER_H
