/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef AUDIOMAPPINGMODEL_H
#define AUDIOMAPPINGMODEL_H

#include "audio/speakersettings.h"
#include <QObject>
#include <QVariantList>
#include <vector>

// Backs the Audio mapping dialog (opened from the audio settings): the spatial mapping mode
// and its options, the speaker positions of the surround output layout and the speakers of
// the speaker array. Every change is pushed to the mixers of the PortAudio layers right away
// (cplay::audio::setSpeakerSetup) and saved to AudioSettings. The speakers can be shown,
// selected and placed in the master 3D view.
class AudioMappingModel : public QObject {
    Q_OBJECT
    // Output layout from the audio settings and whether up/down mixing is on (the spatial
    // mapping needs it).
    Q_PROPERTY(int channelCount READ channelCount NOTIFY layoutChanged)
    Q_PROPERTY(bool mixingEnabled READ mixingEnabled NOTIFY layoutChanged)
    // Circular surface mapping (Grid settings) on: the speaker array can follow the circular screen.
    Q_PROPERTY(bool circularAvailable READ circularAvailable NOTIFY layoutChanged)
    // 0 = off (standard mapping), 1 = surround panning, 2 = mono speaker array.
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY spatialChanged)
    Q_PROPERTY(bool spatialEnabled READ spatialEnabled NOTIFY spatialChanged)
    // Surround panning options.
    Q_PROPERTY(double focus READ focus WRITE setFocus NOTIFY spatialChanged)
    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY spatialChanged)
    // Speaker array options: 0 = dome positions, 1 = circular screen positions.
    Q_PROPERTY(int arrayLayout READ arrayLayout WRITE setArrayLayout NOTIFY spatialChanged)
    Q_PROPERTY(double rolloff READ rolloff WRITE setRolloff NOTIFY spatialChanged)
    Q_PROPERTY(double blur READ blur WRITE setBlur NOTIFY spatialChanged)
    Q_PROPERTY(bool constantPower READ constantPower WRITE setConstantPower NOTIFY spatialChanged)
    // Output channels the speaker array needs (highest speaker channel).
    Q_PROPERTY(int arrayChannelCount READ arrayChannelCount NOTIFY speakersChanged)
    // Surround layout: one entry per output channel {index, label, lfe, azimuth, elevation}.
    Q_PROPERTY(QVariantList speakers READ speakers NOTIFY speakersChanged)
    // Speaker array: {index, channel, azimuth, elevation, degrees, height}.
    Q_PROPERTY(QVariantList arraySpeakers READ arraySpeakers NOTIFY speakersChanged)
    // Master 3D view state (not saved): show the speakers, the selected one (of the surround
    // layout or the speaker array, following the mode), and whether a click places it.
    Q_PROPERTY(bool showSpeakersInView READ showSpeakersInView WRITE setShowSpeakersInView NOTIFY viewStateChanged)
    Q_PROPERTY(int selectedSpeaker READ selectedSpeaker WRITE setSelectedSpeaker NOTIFY viewStateChanged)
    Q_PROPERTY(bool placeOnClick READ placeOnClick WRITE setPlaceOnClick NOTIFY viewStateChanged)

public:
    explicit AudioMappingModel(QObject* parent = nullptr);

    int channelCount() const;
    bool mixingEnabled() const;
    bool circularAvailable() const;

    int mode() const;
    void setMode(int mode);
    bool spatialEnabled() const;
    double focus() const;
    void setFocus(double focus);
    int width() const;
    void setWidth(int width);

    int arrayLayout() const;
    void setArrayLayout(int layout);
    double rolloff() const;
    void setRolloff(double db);
    double blur() const;
    void setBlur(double percent);
    bool constantPower() const;
    void setConstantPower(bool on);
    int arrayChannelCount() const;

    QVariantList speakers() const;
    QVariantList arraySpeakers() const;

    bool showSpeakersInView() const;
    void setShowSpeakersInView(bool show);
    int selectedSpeaker() const;
    void setSelectedSpeaker(int index);
    bool placeOnClick() const;
    void setPlaceOnClick(bool place);

    // Surround layout: moves a speaker (degrees, dome frame). save=false while dragging; call
    // saveSpeakerPositions() when the drag ends. LFE channels have no position.
    Q_INVOKABLE void setSpeakerPosition(int index, double azimuth, double elevation, bool save = true);
    Q_INVOKABLE void saveSpeakerPositions();
    Q_INVOKABLE void resetSpeakerPositions();

    // Speaker array editing. Positions: dome (azimuth/elevation) or circular screen
    // (degrees/height in cm). save=false while dragging, then saveArraySpeakers().
    Q_INVOKABLE void addArraySpeaker();
    Q_INVOKABLE void removeArraySpeaker(int index);
    Q_INVOKABLE void setArraySpeakerChannel(int index, int channel);
    Q_INVOKABLE void setArraySpeakerDomePosition(int index, double azimuth, double elevation, bool save = true);
    Q_INVOKABLE void setArraySpeakerCircularPosition(int index, double degrees, double heightCm, bool save = true);
    Q_INVOKABLE void saveArraySpeakers();
    // Evenly spaced speakers around the dome or the circular screen, on channels 1..count.
    Q_INVOKABLE void createEvenArray(int count);

    // Re-reads the audio settings, e.g. after the output channels or the mixing option changed.
    Q_INVOKABLE void reload();

Q_SIGNALS:
    void layoutChanged();
    void spatialChanged();
    void speakersChanged();
    void viewStateChanged();

private:
    void apply();
    bool validArrayIndex(int index) const;

    int m_channelCount = 2;
    cplay::audio::SpeakerPositions m_positions{};
    std::vector<cplay::audio::ArraySpeakerSettings> m_array;
    bool m_showSpeakersInView = false;
    int m_selectedSpeaker = -1;
    bool m_placeOnClick = false;
};

#endif // AUDIOMAPPINGMODEL_H
