/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "directshowmodel.h"
#include <sgct/sgct.h>
#include <format>

#ifdef _WIN32

namespace {

// Device category CLSIDs for ICreateDevEnum::CreateClassEnumerator() - values from
// the SDK's uuids.h (CLSID_VideoInputDeviceCategory / CLSID_AudioInputDeviceCategory).
// These are NOT the CATID_* category IDs that filters register under; passing those
// makes CreateClassEnumerator return S_FALSE and no devices are listed.
const GUID kCatVideoInput{0x860bb310, 0x5d01, 0x11d0, {0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86}};
const GUID kCatAudioInput{0x33d9a762, 0x90c8, 0x11d0, {0xbd, 0x43, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86}};

// Enumerates all devices in a DirectShow device category (e.g. CAT_VideoInput)
// and returns their friendly names, in the order Windows reports them. An empty
// list is returned when the category has no devices or enumeration fails.
QStringList enumerateCategory(const GUID &category) {
    QStringList devices;

    ICreateDevEnum *pSystemDevices = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ICreateDevEnum, reinterpret_cast<void **>(&pSystemDevices));
    if (FAILED(hr) || !pSystemDevices) {
        sgct::Log::Error("DirectShowModel: failed to create the system device enumerator\n");
        return devices;
    }

    // This SDK's ICreateDevEnum enumerates monikers (not filters) - bind each
    // one to its property bag to read the device's friendly name.
    IEnumMoniker *pCategoryDevices = nullptr;
    hr = pSystemDevices->CreateClassEnumerator(category, &pCategoryDevices, 0);
    // S_OK: an enumerator was created. S_FALSE: the category exists but has no devices.
    if (SUCCEEDED(hr) && pCategoryDevices) {
        IMoniker *punkDevice = nullptr;
        while (pCategoryDevices->Next(1, &punkDevice, nullptr) == S_OK) {
            IPropertyBag *pPropBag = nullptr;
            if (SUCCEEDED(punkDevice->BindToStorage(nullptr, nullptr, IID_IPropertyBag,
                                                    reinterpret_cast<void **>(&pPropBag))) && pPropBag) {
                VARIANT varName{};
                VariantInit(&varName);
                if (SUCCEEDED(pPropBag->Read(L"FriendlyName", &varName, nullptr))
                    && varName.vt == VT_BSTR && varName.bstrVal) {
                    devices.append(QString::fromWCharArray(varName.bstrVal));
                }
                VariantClear(&varName);
                pPropBag->Release();
            }
            punkDevice->Release();
        }
        pCategoryDevices->Release();
    }

    pSystemDevices->Release();
    return devices;
}

} // namespace

#endif // _WIN32

DirectShowModel::DirectShowModel(QObject *parent)
    : QObject(parent) {
}

QStringList DirectShowModel::videoDevices() const {
    return m_videoDevices;
}

QStringList DirectShowModel::audioDevices() const {
    return m_audioDevices;
}

QString DirectShowModel::selectedVideoDevice() const {
    return m_selectedVideoDevice;
}

void DirectShowModel::setSelectedVideoDevice(const QString &device) {
    if (m_selectedVideoDevice == device) {
        return;
    }
    m_selectedVideoDevice = device;
    Q_EMIT videoDeviceSelected();
}

QString DirectShowModel::selectedAudioDevice() const {
    return m_selectedAudioDevice;
}

void DirectShowModel::setSelectedAudioDevice(const QString &device) {
    if (m_selectedAudioDevice == device) {
        return;
    }
    m_selectedAudioDevice = device;
    Q_EMIT audioDeviceSelected();
}

void DirectShowModel::updateDeviceLists() {
#ifdef _WIN32
    // COM must be initialized on this thread before any DirectShow call.
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hrCo) && hrCo != RPC_E_CHANGED_MODE) {
        sgct::Log::Error("DirectShowModel: failed to initialize COM\n");
        return;
    }

    const QStringList video = enumerateCategory(kCatVideoInput);
    const QStringList audio = enumerateCategory(kCatAudioInput);

    if (video != m_videoDevices || audio != m_audioDevices) {
        m_videoDevices = video;
        m_audioDevices = audio;
        Q_EMIT deviceListsChanged();
    }

    // A re-scan may have removed a currently selected device (e.g. unplugged).
    if (!m_selectedVideoDevice.isEmpty() && !video.contains(m_selectedVideoDevice)) {
        m_selectedVideoDevice.clear();
        Q_EMIT videoDeviceSelected();
    }
    if (!m_selectedAudioDevice.isEmpty() && !audio.contains(m_selectedAudioDevice)) {
        m_selectedAudioDevice.clear();
        Q_EMIT audioDeviceSelected();
    }

    sgct::Log::Info(std::format("DirectShowModel: found {} video and {} audio capture devices\n",
                                video.size(), audio.size()));

    // Balance only the COM initialization we performed ourselves.
    if (hrCo == S_OK) {
        CoUninitialize();
    }
#endif // _WIN32
}
