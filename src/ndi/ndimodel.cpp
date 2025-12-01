/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ndimodel.h"
#include "ndilayer.h"
#include "audiosettings.h"
#include <QRegularExpression>

NDISendersModel::NDISendersModel(QObject *parent)
    : QAbstractListModel(parent) {
}

NDISendersModel::~NDISendersModel() {
}

int NDISendersModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid())
        return 0;

    return m_NDIsenders.size();
}

QVariant NDISendersModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || m_NDIsenders.empty())
        return QVariant();

    if (!checkIndex(index)) {
        return QVariant();
    }
    if (role == textRole) {
        return m_NDIsenders.at(index.row());
    }
    return QVariant();
}

QHash<int, QByteArray> NDISendersModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[textRole] = "typeName";
    return roles;
}

void NDISendersModel::updateSendersList() {
    NdiFinder::instance().findSenders();
    std::vector<std::string> sendersList = NdiFinder::instance().getSendersList();

    beginResetModel();
    m_NDIsenders.clear();
    for (auto s : sendersList) {
        m_NDIsenders.append(QString::fromStdString(s));
    }
    endResetModel();

    Q_EMIT sendersListChanged();
}

int NDISendersModel::getNumberOfSenders() {
    return m_NDIsenders.size();
}

QString NDISendersModel::getNDIVersionString() {
    QString ndiVersion = QString::fromStdString(NdiFinder::instance().getNDIVersionString());
    QRegularExpression regExp(QStringLiteral("\\d*\\.\\d*\\.\\d*"));
    QRegularExpressionMatch match = regExp.match(ndiVersion);
    if (match.hasMatch()) {
        return match.captured(0);
    }
    return ndiVersion;
}

PortAudioModel::PortAudioModel(QObject* parent)
    : QAbstractListModel(parent), m_currentDevice(-1) {
}

PortAudioModel::~PortAudioModel() {
    if (m_paIsInitialized) {
        Pa_Terminate();
    }
}

int PortAudioModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;

    return m_audioOutputs.size();
}

QVariant PortAudioModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || m_audioOutputs.empty())
        return QVariant();

    if (!checkIndex(index)) {
        return QVariant();
    }

    switch (role) {
    case textRole:
        return QVariant(m_audioOutputs.at(index.row()).deviceName + QStringLiteral(" with ") + m_audioOutputs.at(index.row()).apiName);
    }

    return QVariant();
}

QHash<int, QByteArray> PortAudioModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[textRole] = "name";
    return roles;
}

int PortAudioModel::getCurrentDevice() {
    return m_currentDevice;
}

void PortAudioModel::setCurrentDevice(int idx) {
    m_currentDevice = idx;
    Q_EMIT portAudioListChanged();
}

void PortAudioModel::updatePortAudioList() {
    beginResetModel();
    m_audioOutputs.clear();

    if (!m_paIsInitialized) {
        PaError error = Pa_Initialize();
        m_paIsInitialized = (error == paNoError);
    }

    if (m_paIsInitialized) {
        int numDevices = Pa_GetDeviceCount();
        if (numDevices > 0) {
            const PaDeviceInfo* deviceInfo;
            const PaHostApiInfo* apiInfo;
            for (int i = 0; i < numDevices; i++) {
                deviceInfo = Pa_GetDeviceInfo(i);
                apiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);

                // Only considering devices with minimum 2 channel output
                if (deviceInfo->maxOutputChannels > 1) {
                    m_audioOutputs.push_back(PortAudioModel::PortAudioDevice(i,
                        QString::fromUtf8(deviceInfo->name), QString::fromUtf8(apiInfo->name)));
                }
            }
        }
    }
    endResetModel();

    Q_EMIT portAudioListChanged();
}

int PortAudioModel::getIndexOfCurrentDevice(){
    if (!m_paIsInitialized || m_audioOutputs.empty()) {
        updatePortAudioList();
    }

    // Finding current audio output
    if (AudioSettings::portAudioCustomOutput() 
        && !AudioSettings::portAudioOutputDevice().isEmpty()
        && !AudioSettings::portAudioOutputApi().isEmpty()) {
        for (int i = 0; i < m_audioOutputs.size(); i++) {
            if (m_audioOutputs[i].deviceName == AudioSettings::portAudioOutputDevice()
                && m_audioOutputs[i].apiName == AudioSettings::portAudioOutputApi())
                return i;
        }
    }
    else {
        // Look for default device
        PaDeviceIndex deviceIndex = Pa_GetDefaultOutputDevice(); /* default output device */
        if (deviceIndex == paNoDevice) {
            return -1;
        }
        for (int i = 0; i < m_audioOutputs.size(); i++) {
            if (m_audioOutputs[i].id == deviceIndex)
                return i;
        }
    }

    return -1;
}

QString PortAudioModel::getDeviceInfo() {
    if (!m_paIsInitialized || m_audioOutputs.empty()) {
        updatePortAudioList();
    }

    if (!m_paIsInitialized) {
        return QStringLiteral("Port audio not initialized");
    }
    else {
        PaDeviceIndex deviceIndex = getIndexOfCurrentDevice();
        QString prePend = QStringLiteral("Using device ");
        if (deviceIndex < 0) {
            deviceIndex = Pa_GetDefaultOutputDevice(); /* default output device */
            if (deviceIndex == paNoDevice) {
                return QStringLiteral("No default audio output device.");
            }
            else {
                prePend = QStringLiteral("Using default device which is ");
            }
        }
        else if(deviceIndex < m_audioOutputs.size()) {
            deviceIndex = m_audioOutputs[deviceIndex].id;
        }

        int numDevices = Pa_GetDeviceCount();
        if (numDevices > deviceIndex) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(deviceIndex);
            const PaHostApiInfo* apiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);

            QString deviceName = QString::fromUtf8(deviceInfo->name);
            QString apiName = QString::fromUtf8(apiInfo->name);

            return prePend + deviceName + QStringLiteral(" with ") + apiName + QStringLiteral(" with maximum ")
                + QString::number(deviceInfo->maxOutputChannels) + QStringLiteral(" output channels");
        }
        else {
            return QStringLiteral("Device range out of scope...");
        }
    }
}

QString PortAudioModel::getDeviceName() {
    if (!m_paIsInitialized || m_audioOutputs.empty()) {
        updatePortAudioList();
    }

    if (m_currentDevice >= 0 && m_currentDevice < m_audioOutputs.size()) {
        return m_audioOutputs[m_currentDevice].deviceName;
    }
    else { // Looking for 
        return QStringLiteral("");
    }
}

QString PortAudioModel::getApiName() {
    if (!m_paIsInitialized || m_audioOutputs.empty()) {
        updatePortAudioList();
    }

    if (m_currentDevice >= 0 && m_currentDevice < m_audioOutputs.size()) {
        return m_audioOutputs[m_currentDevice].apiName;
    }
    else { // Looking for 
        return QStringLiteral("");
    }
}