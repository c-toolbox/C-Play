/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIRECTSHOWPRESETSMODEL_H
#define DIRECTSHOWPRESETSMODEL_H

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>

// QML facing model listing the predefined DirectShow capture setups from
// data/predefined-directshows.json. Each entry is a nickname ("title") for a
// selected combination of video and audio capture devices; either device may
// be empty, which describes an audio-only or video-only setup respectively.
// The plain videoDevice/audioDevice are the defaults shown in the UI and used
// as fallback on machines without a local entry; per-machine overrides live in
// the optional "devices" object (see DirectShowPathsConfig) and are resolved
// at capture time by each machine from its own copy of the file.
//
// JSON format:
// {
//   "directshows": [
//     {
//       "title": "HDMI Capture 1",
//       "videoDevice": "DELTA-hmi Video Source (card0 RX0)",
//       "audioDevice": "",
//       "enabled": true,
//       "devices": {
//         "master": { "videoDevice": "...", "audioDevice": "" },
//         "node-A": { "videoDevice": "...", "audioDevice": "..." }
//       }
//     }
//   ]
// }

class DirectShowPresetsModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit DirectShowPresetsModel(QObject *parent = nullptr);
    ~DirectShowPresetsModel();

    enum {
        videoDeviceRole = Qt::UserRole,
        audioDeviceRole,
        titleRole
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual QHash<int, QByteArray> roleNames() const override;

    // (Re-)reads ./data/predefined-directshows.json. A missing or invalid file results in an empty list.
    Q_INVOKABLE void updatePresetsList();

    Q_PROPERTY(int numberOfPresets READ getNumberOfPresets NOTIFY presetsListChanged)
    int getNumberOfPresets();

Q_SIGNALS:
    void presetsListChanged();

private:
    QStringList m_titles;
    QStringList m_videoDevices;
    QStringList m_audioDevices;
};

#endif // DIRECTSHOWPRESETSMODEL_H