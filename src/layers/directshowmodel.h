/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIRECTSHOWMODEL_H
#define DIRECTSHOWMODEL_H

#include <QObject>
#include <QStringList>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <dshow.h>
#endif

/**
 * QML facing model listing the DirectShow capture devices on this machine.
 *
 * Video and audio are enumerated separately, from the video input and audio
 * input device categories respectively, since a capture card often registers
 * its camera and its microphone as two different devices. The selected video
 * and audio device can therefore be chosen independently, so both streams can
 * be captured at the same time - possibly from different devices.
 */
class DirectShowModel : public QObject {
    Q_OBJECT

public:
    explicit DirectShowModel(QObject *parent = nullptr);

    // Friendly names of all registered video capture devices (cameras, capture cards).
    Q_PROPERTY(QStringList videoDevices READ videoDevices NOTIFY deviceListsChanged)
    QStringList videoDevices() const;

    // Friendly names of all registered audio capture devices (microphones, line inputs).
    Q_PROPERTY(QStringList audioDevices READ audioDevices NOTIFY deviceListsChanged)
    QStringList audioDevices() const;

    // The currently selected capture devices. An empty string means "no device".
    // They are independent on purpose: video and audio may come from different cards.
    Q_PROPERTY(QString selectedVideoDevice READ selectedVideoDevice WRITE setSelectedVideoDevice NOTIFY videoDeviceSelected)
    QString selectedVideoDevice() const;
    void setSelectedVideoDevice(const QString &device);

    Q_PROPERTY(QString selectedAudioDevice READ selectedAudioDevice WRITE setSelectedAudioDevice NOTIFY audioDeviceSelected)
    QString selectedAudioDevice() const;
    void setSelectedAudioDevice(const QString &device);

    // Re-enumerates the video and audio capture devices. Call this when a new
    // device may have been plugged in (e.g. from a refresh button).
    Q_INVOKABLE void updateDeviceLists();

Q_SIGNALS:
    void deviceListsChanged();
    void videoDeviceSelected();
    void audioDeviceSelected();

private:
    QStringList m_videoDevices;
    QStringList m_audioDevices;
    QString m_selectedVideoDevice;
    QString m_selectedAudioDevice;
};

#endif // DIRECTSHOWMODEL_H
