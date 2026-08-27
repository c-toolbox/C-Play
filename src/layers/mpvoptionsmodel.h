/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MPVOPTIONSMODEL_H
#define MPVOPTIONSMODEL_H

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

class MpvOptionsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit MpvOptionsModel(QObject *parent = nullptr);
    ~MpvOptionsModel();

    enum {
        nameRole = Qt::UserRole,
        titleRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    // Scan the mpv-conf root folder for files matching "<name><suffix>.json".
    // The suffix is layer type specific, e.g. "_video", "_audio" or "_stream".
    // The first entry is always the "Use global settings" default (empty name).
    Q_INVOKABLE void updateOptionsList(const QString &suffix);

    // Index of the given option name in the current list, or 0 (global) if not found.
    Q_INVOKABLE int indexOfOption(const QString &name) const;

    // Option name at the given index, or an empty string for the default entry.
    Q_INVOKABLE QString optionNameAt(int index) const;

    Q_PROPERTY(int numberOfOptions READ getNumberOfOptions NOTIFY optionsListChanged)
    int getNumberOfOptions();

Q_SIGNALS:
    void optionsListChanged();

private:
    QStringList m_optionTitles;
    QStringList m_optionNames;
};

#endif // MPVOPTIONSMODEL_H
