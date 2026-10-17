/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "audio/audiomappingmodel.h"
#include "audiosettings.h"
#include "gridsettings.h"
#include <QVariantMap>
#include <algorithm>
#include <cmath>

namespace {
double wrapAzimuth(double a) {
    a = std::fmod(a, 360.0);
    if (a > 180.0)
        a -= 360.0;
    else if (a <= -180.0)
        a += 360.0;
    return a;
}

double wrapDegrees(double a) {
    a = std::fmod(a, 360.0);
    return a < 0.0 ? a + 360.0 : a;
}
} // namespace

AudioMappingModel::AudioMappingModel(QObject* parent)
    : QObject(parent) {
    reload();
}

int AudioMappingModel::channelCount() const {
    return m_channelCount;
}

bool AudioMappingModel::mixingEnabled() const {
    return AudioSettings::portAudioMixInputToOutput();
}

bool AudioMappingModel::circularAvailable() const {
    return GridSettings::circularEnabled();
}

int AudioMappingModel::mode() const {
    return std::clamp(AudioSettings::portAudioSpatialMode(), 0, 2);
}

void AudioMappingModel::setMode(int mode) {
    mode = std::clamp(mode, 0, 2);
    if (mode == this->mode())
        return;
    AudioSettings::setPortAudioSpatialMode(mode);
    AudioSettings::self()->save();
    m_selectedSpeaker = -1; // the selection belongs to the other speaker list
    m_placeOnClick = false;
    apply();
    Q_EMIT spatialChanged();
    Q_EMIT viewStateChanged();
}

bool AudioMappingModel::spatialEnabled() const {
    return mixingEnabled() && mode() != 0;
}

double AudioMappingModel::focus() const {
    return AudioSettings::portAudioSpatialFocus();
}

void AudioMappingModel::setFocus(double focus) {
    focus = std::clamp(focus, 1.0, 20.0);
    if (std::abs(focus - this->focus()) < 1e-6)
        return;
    AudioSettings::setPortAudioSpatialFocus(focus);
    AudioSettings::self()->save();
    apply();
    Q_EMIT spatialChanged();
}

int AudioMappingModel::width() const {
    return AudioSettings::portAudioSpatialWidth();
}

void AudioMappingModel::setWidth(int width) {
    width = std::clamp(width, 0, 100);
    if (width == this->width())
        return;
    AudioSettings::setPortAudioSpatialWidth(width);
    AudioSettings::self()->save();
    apply();
    Q_EMIT spatialChanged();
}

int AudioMappingModel::arrayLayout() const {
    return std::clamp(AudioSettings::portAudioArrayLayout(), 0, 1);
}

void AudioMappingModel::setArrayLayout(int layout) {
    layout = std::clamp(layout, 0, 1);
    if (layout == arrayLayout())
        return;
    AudioSettings::setPortAudioArrayLayout(layout);
    AudioSettings::self()->save();
    apply();
    Q_EMIT spatialChanged();
    Q_EMIT speakersChanged(); // the dialog shows the other pair of coordinates
}

double AudioMappingModel::rolloff() const {
    return AudioSettings::portAudioArrayRolloff();
}

void AudioMappingModel::setRolloff(double db) {
    db = std::clamp(db, 0.0, 24.0);
    if (std::abs(db - rolloff()) < 1e-6)
        return;
    AudioSettings::setPortAudioArrayRolloff(db);
    AudioSettings::self()->save();
    apply();
    Q_EMIT spatialChanged();
}

double AudioMappingModel::blur() const {
    return AudioSettings::portAudioArrayBlur();
}

void AudioMappingModel::setBlur(double percent) {
    percent = std::clamp(percent, 0.1, 100.0);
    if (std::abs(percent - blur()) < 1e-6)
        return;
    AudioSettings::setPortAudioArrayBlur(percent);
    AudioSettings::self()->save();
    apply();
    Q_EMIT spatialChanged();
}

bool AudioMappingModel::constantPower() const {
    return AudioSettings::portAudioArrayConstantPower();
}

void AudioMappingModel::setConstantPower(bool on) {
    if (on == constantPower())
        return;
    AudioSettings::setPortAudioArrayConstantPower(on);
    AudioSettings::self()->save();
    apply();
    Q_EMIT spatialChanged();
}

int AudioMappingModel::arrayChannelCount() const {
    int highest = 0;
    for (const auto& spk : m_array)
        highest = std::max(highest, spk.channel);
    return highest;
}

QVariantList AudioMappingModel::speakers() const {
    QVariantList list;
    for (int i = 0; i < m_channelCount; ++i) {
        const glm::vec2& p = m_positions[static_cast<std::size_t>(i)];
        list.append(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("label"), QString::fromLatin1(cplay::audio::channelLabel(m_channelCount, i))},
            {QStringLiteral("lfe"), cplay::audio::isLfeChannel(m_channelCount, i)},
            {QStringLiteral("azimuth"), static_cast<double>(p.x)},
            {QStringLiteral("elevation"), static_cast<double>(p.y)},
        });
    }
    return list;
}

QVariantList AudioMappingModel::arraySpeakers() const {
    QVariantList list;
    for (int i = 0; i < static_cast<int>(m_array.size()); ++i) {
        const auto& spk = m_array[static_cast<std::size_t>(i)];
        list.append(QVariantMap{
            {QStringLiteral("index"), i},
            {QStringLiteral("channel"), spk.channel},
            {QStringLiteral("azimuth"), static_cast<double>(spk.azimuth)},
            {QStringLiteral("elevation"), static_cast<double>(spk.elevation)},
            {QStringLiteral("degrees"), static_cast<double>(spk.degrees)},
            {QStringLiteral("height"), static_cast<double>(spk.heightCm)},
        });
    }
    return list;
}

bool AudioMappingModel::showSpeakersInView() const {
    return m_showSpeakersInView;
}

void AudioMappingModel::setShowSpeakersInView(bool show) {
    if (m_showSpeakersInView == show)
        return;
    m_showSpeakersInView = show;
    if (!show)
        m_placeOnClick = false;
    Q_EMIT viewStateChanged();
}

int AudioMappingModel::selectedSpeaker() const {
    return m_selectedSpeaker;
}

void AudioMappingModel::setSelectedSpeaker(int index) {
    if (mode() == static_cast<int>(cplay::audio::SpatialMode::SpeakerArray)) {
        if (!validArrayIndex(index))
            index = -1;
    } else if (index < -1 || index >= m_channelCount || cplay::audio::isLfeChannel(m_channelCount, index)) {
        index = -1;
    }
    if (m_selectedSpeaker == index)
        return;
    m_selectedSpeaker = index;
    Q_EMIT viewStateChanged();
}

bool AudioMappingModel::placeOnClick() const {
    return m_placeOnClick;
}

void AudioMappingModel::setPlaceOnClick(bool place) {
    if (m_placeOnClick == place)
        return;
    m_placeOnClick = place;
    Q_EMIT viewStateChanged();
}

void AudioMappingModel::setSpeakerPosition(int index, double azimuth, double elevation, bool save) {
    if (index < 0 || index >= m_channelCount || cplay::audio::isLfeChannel(m_channelCount, index)
        || !std::isfinite(azimuth) || !std::isfinite(elevation))
        return;
    m_positions[static_cast<std::size_t>(index)] = glm::vec2(static_cast<float>(wrapAzimuth(azimuth)),
                                                             static_cast<float>(std::clamp(elevation, -90.0, 90.0)));
    apply();
    if (save)
        saveSpeakerPositions();
    Q_EMIT speakersChanged();
}

void AudioMappingModel::saveSpeakerPositions() {
    AudioSettings::setPortAudioSpeakerPositions(cplay::audio::formatSpeakerPositions(m_positions, m_channelCount));
    AudioSettings::self()->save();
}

void AudioMappingModel::resetSpeakerPositions() {
    m_positions = cplay::audio::defaultSpeakerPositions(m_channelCount);
    saveSpeakerPositions();
    apply();
    Q_EMIT speakersChanged();
}

bool AudioMappingModel::validArrayIndex(int index) const {
    return index >= 0 && index < static_cast<int>(m_array.size());
}

void AudioMappingModel::addArraySpeaker() {
    if (static_cast<int>(m_array.size()) >= cplay::audio::kMaxArraySpeakers)
        return;
    cplay::audio::ArraySpeakerSettings spk;
    spk.channel = std::min(cplay::audio::kMaxOutputChannels, arrayChannelCount() + 1);
    if (!m_array.empty()) {
        // Next to the previous speaker, so a new one is easy to find in the view.
        const auto& last = m_array.back();
        spk.azimuth = static_cast<float>(wrapAzimuth(last.azimuth + 30.0));
        spk.elevation = last.elevation;
        spk.degrees = static_cast<float>(wrapDegrees(last.degrees + 30.0));
        spk.heightCm = last.heightCm;
    }
    m_array.push_back(spk);
    saveArraySpeakers();
    apply();
    Q_EMIT speakersChanged();
}

void AudioMappingModel::removeArraySpeaker(int index) {
    if (!validArrayIndex(index))
        return;
    m_array.erase(m_array.begin() + index);
    if (m_selectedSpeaker >= static_cast<int>(m_array.size()))
        m_selectedSpeaker = -1;
    saveArraySpeakers();
    apply();
    Q_EMIT speakersChanged();
    Q_EMIT viewStateChanged();
}

void AudioMappingModel::setArraySpeakerChannel(int index, int channel) {
    if (!validArrayIndex(index))
        return;
    m_array[static_cast<std::size_t>(index)].channel = std::clamp(channel, 1, cplay::audio::kMaxOutputChannels);
    saveArraySpeakers();
    apply();
    Q_EMIT speakersChanged();
}

void AudioMappingModel::setArraySpeakerDomePosition(int index, double azimuth, double elevation, bool save) {
    if (!validArrayIndex(index) || !std::isfinite(azimuth) || !std::isfinite(elevation))
        return;
    auto& spk = m_array[static_cast<std::size_t>(index)];
    spk.azimuth = static_cast<float>(wrapAzimuth(azimuth));
    spk.elevation = static_cast<float>(std::clamp(elevation, -90.0, 90.0));
    apply();
    if (save)
        saveArraySpeakers();
    Q_EMIT speakersChanged();
}

void AudioMappingModel::setArraySpeakerCircularPosition(int index, double degrees, double heightCm, bool save) {
    if (!validArrayIndex(index) || !std::isfinite(degrees) || !std::isfinite(heightCm))
        return;
    auto& spk = m_array[static_cast<std::size_t>(index)];
    spk.degrees = static_cast<float>(wrapDegrees(degrees));
    spk.heightCm = static_cast<float>(std::clamp(heightCm, -100000.0, 100000.0));
    apply();
    if (save)
        saveArraySpeakers();
    Q_EMIT speakersChanged();
}

void AudioMappingModel::saveArraySpeakers() {
    AudioSettings::setPortAudioSpeakerArray(cplay::audio::formatSpeakerArray(m_array));
    AudioSettings::self()->save();
}

void AudioMappingModel::createEvenArray(int count) {
    count = std::clamp(count, 1, cplay::audio::kMaxArraySpeakers);
    m_array.clear();
    for (int i = 0; i < count; ++i) {
        cplay::audio::ArraySpeakerSettings spk;
        spk.channel = std::min(i + 1, cplay::audio::kMaxOutputChannels);
        const double step = 360.0 / count;
        spk.azimuth = static_cast<float>(wrapAzimuth(i * step));
        spk.degrees = static_cast<float>(wrapDegrees(GridSettings::circularSeamDegrees() + (i + 0.5) * step));
        m_array.push_back(spk);
    }
    m_selectedSpeaker = -1;
    saveArraySpeakers();
    apply();
    Q_EMIT speakersChanged();
    Q_EMIT viewStateChanged();
}

void AudioMappingModel::reload() {
    m_channelCount = std::clamp(AudioSettings::portAudioOutputChannels(), 1, cplay::audio::kMaxChannels);
    m_positions = cplay::audio::parseSpeakerPositions(AudioSettings::portAudioSpeakerPositions(), m_channelCount);
    m_array = cplay::audio::parseSpeakerArray(AudioSettings::portAudioSpeakerArray());
    if (mode() == static_cast<int>(cplay::audio::SpatialMode::SpeakerArray) ? !validArrayIndex(m_selectedSpeaker)
            : (m_selectedSpeaker >= m_channelCount || cplay::audio::isLfeChannel(m_channelCount, m_selectedSpeaker)))
        m_selectedSpeaker = -1;
    apply();
    Q_EMIT layoutChanged();
    Q_EMIT spatialChanged();
    Q_EMIT speakersChanged();
    Q_EMIT viewStateChanged();
}

void AudioMappingModel::apply() {
    cplay::audio::setSpeakerSetup(cplay::audio::speakerSetupFromSettings(m_positions, m_array));
}
