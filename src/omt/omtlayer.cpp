/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sund�n <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "omtlayer.h"
#include "audiosettings.h"
#include <sgct/sgct.h>
#include <utils/texturesubdivision.h>
#include <audio/portaudiolock.h>
#include <cstring>
#include <cmath>
#include <algorithm>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

// ============================================================
// OmtFinder
// ============================================================
OmtFinder* OmtFinder::_instance = nullptr;

OmtFinder::OmtFinder() {
}

OmtFinder::~OmtFinder() {
}

void OmtFinder::destroy() {
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

bool OmtFinder::checkVideoDecoderAvailable() {
#if defined(_WIN32)
    const char* libName = "libvmx.dll";
    // Kept loaded; libomt resolves the same module later.
    const bool available = LoadLibraryA(libName) != nullptr;
#elif defined(__APPLE__)
    const char* libName = "libvmx.dylib";
    const bool available = dlopen(libName, RTLD_NOW) != nullptr;
#else
    const char* libName = "libvmx.so";
    const bool available = dlopen(libName, RTLD_NOW) != nullptr;
#endif
    if (!available) {
        sgct::Log::Error(std::string("OMT Error: ") + libName
            + " not found. OMT layers will receive audio but no video frames.");
    }
    return available;
}

OmtFinder& OmtFinder::instance() {
    if (!_instance) {
        _instance = new OmtFinder();
    }
    return *_instance;
}

std::vector<std::string> OmtFinder::getSendersList() {
    std::vector<std::string> sendersList;
    int count = 0;
    char** addresses = omt_discovery_getaddresses(&count);
    if (addresses && count > 0) {
        for (int i = 0; i < count; i++) {
            if (addresses[i]) {
                sendersList.push_back(addresses[i]);
            }
        }
    }
    return sendersList;
}

bool OmtFinder::senderExists(const std::string& senderName) {
    std::vector<std::string> senders = getSendersList();
    for (const auto& s : senders) {
        if (s == senderName) {
            return true;
        }
    }
    return false;
}

std::string OmtFinder::getOMTVersionString() {
    return "1.0";
}

// ============================================================
// OmtLayer
// ============================================================
OmtLayer::OmtLayer() {
    setType(BaseLayer::LayerType::OMT);
    // OMT frames arrive top-down; stored presentations override this via "flipY".
    setFlipY(true);
    enableTextureSubdivision();
}

OmtLayer::~OmtLayer() {
    std::lock_guard<std::recursive_mutex> lock(m_audioMutex);
    std::unique_lock<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
    // Close whenever open: a stream left open would keep a shared device stream running.
    if (m_audioStream && m_audioStreamOpen) {
        m_audioError = cplay::audio::paCloseStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamOpen = false;
        }
    }
    if (m_portAudioInitialized) {
        Pa_Terminate();
        m_portAudioInitialized = false;
    }
    paLock.unlock();

    if (m_receiver) {
        omt_receive_destroy(m_receiver);
        m_receiver = nullptr;
    }

    if (renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId);
    }

}

void OmtLayer::initialize() {
    m_hasInitialized = true;

    // Only receive audio on master
    if (isAudioEnabled()) {
        std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
        m_audioError = Pa_Initialize();
        if (m_audioError != paNoError) {
            sgct::Log::Error("OmtLayer Error: PortAudio initialization failed.\n");
        }
        else {
            m_portAudioInitialized = true;
            m_receiveAudio = true;
            m_audioOutputParameters.device = Pa_GetDefaultOutputDevice();
            if (m_audioOutputParameters.device == paNoDevice) {
                sgct::Log::Error("OmtLayer Error: No default audio output device.\n");
            }
        }
    }
}

void OmtLayer::update(bool updateRendering) {
    if (m_typePropertiesDecoded) {
        m_typePropertiesDecoded = false;
        setVolume(m_volume_Dec);
        setVolumeMute(m_volumeMute_Dec);
    }

    // Check if sender exists
    if (!OmtFinder::instance().senderExists(filepath())) {
        m_isReady = false;
        if (m_receiver) {
            omt_receive_destroy(m_receiver);
            m_receiver = nullptr;
        }
        return;
    }

    // Create receiver if needed � request both video and audio if audio is enabled
    if (!m_receiver) {
        OMTFrameType frameTypes = OMTFrameType_Video;
        if (m_receiveAudio) {
            frameTypes = static_cast<OMTFrameType>(OMTFrameType_Video | OMTFrameType_Audio);
        }
        m_receiver = omt_receive_create(
            filepath().c_str(),
            frameTypes,
            OMTPreferredVideoFormat_BGRA,
            OMTReceiveFlags_None
        );
        if (!m_receiver) {
            sgct::Log::Error("OmtLayer Error: Failed to create OMT receiver.\n");
            m_isReady = false;
            return;
        }
    }

    if (m_receiver) {
        m_isReady = true;
    }

    // Receive video frame
    if (updateRendering) {
        updateFrame();
    }

    // Receive audio frame (non-blocking)
    if (m_receiveAudio && m_receiver) {
        OMTMediaFrame* audioFrame = omt_receive(m_receiver, OMTFrameType_Audio, 0);
        if (audioFrame && audioFrame->Type == OMTFrameType_Audio && audioFrame->Data && audioFrame->DataLength > 0) {
            std::lock_guard<std::recursive_mutex> lock(m_audioMutex);
            // Start audio stream on first audio frame if not yet started
            if (!m_audioStreamOpen) {
                m_audioSampleRate = audioFrame->SampleRate;
                m_audioChannels = audioFrame->Channels;
                StartAudioStream();
            }

            ProcessAudioFrame(audioFrame);
        }
    }
}

void OmtLayer::updateFrame() {
    if (!m_receiver) {
        return;
    }

    OMTMediaFrame* frame = omt_receive(m_receiver, OMTFrameType_Video, 0);
    if (!frame || frame->Type != OMTFrameType_Video || !frame->Data || frame->DataLength <= 0) {
        return;
    }

    unsigned int width = static_cast<unsigned int>(frame->Width);
    unsigned int height = static_cast<unsigned int>(frame->Height);

    // Check for changed sender dimensions
    if (width != static_cast<unsigned int>(renderData.width) ||
        height != static_cast<unsigned int>(renderData.height)) {
        if (renderData.width > 0) {
            glDeleteTextures(1, &renderData.texId);
        }
        GenerateTexture(renderData.texId, width, height);
        renderData.width = static_cast<int>(width);
        renderData.height = static_cast<int>(height);
    }

    // QR control frames are never uploaded (two-phase scheme).
    if (m_subdivision->isControlFrame(static_cast<unsigned char*>(frame->Data), width, height, GL_BGRA))
        return;

    // Upload pixel data to texture
    glBindTexture(GL_TEXTURE_2D, renderData.texId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
        GL_BGRA, GL_UNSIGNED_BYTE, frame->Data);

    m_subdivision->updateSubLayers(renderData.texId, renderData.width, renderData.height);
}

void OmtLayer::ProcessAudioFrame(OMTMediaFrame* frame) {
    if (!frame || !frame->Data || frame->DataLength <= 0)
        return;

    if (!m_audioStreamOpen || !m_audioStreamStarted)
        return;

    int channels = frame->Channels;
    int samplesPerChannel = frame->SamplesPerChannel;

    if (channels <= 0 || samplesPerChannel <= 0)
        return;

    // Determine output channel count
    int outChannels = m_audioOutputChannels;

    // OMT audio is planar 32-bit float: [ch0_sample0..ch0_sampleN][ch1_sample0..ch1_sampleN]...
    // PortAudio with paFloat32 expects interleaved float: [s0_ch0, s0_ch1, ..., s1_ch0, s1_ch1, ...]
    // We interleave and apply volume � no format conversion needed.
    const float* srcData = static_cast<const float*>(frame->Data);
    // Mute zeroes the per-frame volume so no audio is written to the PortAudio stream.
    const float vol = m_volumeMute ? 0.f : m_audioVolume;

    size_t totalSamples = static_cast<size_t>(samplesPerChannel) * static_cast<size_t>(outChannels);
    if (m_interleavedAudioBuf.size() < totalSamples) {
        m_interleavedAudioBuf.resize(totalSamples);
    }

    // Planar in, interleaved out: standard up/down mix or spatial mapping (shared with the
    // other PortAudio layers).
    m_audioMixer.process(srcData, channels, 1, static_cast<size_t>(samplesPerChannel),
                         m_interleavedAudioBuf.data(), outChannels, static_cast<size_t>(samplesPerChannel), vol);

    // Report the peak of this frame for the audio level meter in the LayerView, but only
    // while the meter is enabled so the per-sample scan stays out of the hot path.
    if (audioLevelsEnabled()) {
        float maxAbs = 0.f;
        for (size_t i = 0; i < totalSamples; ++i) {
            float v = m_interleavedAudioBuf[i];
            if (v < 0.f) v = -v;
            if (v > maxAbs) maxAbs = v;
        }
        reportAudioLevel(maxAbs);
    }

    // Write directly to the PortAudio stream (blocking)
    m_audioError = cplay::audio::paWriteStream(m_audioStream, m_interleavedAudioBuf.data(), static_cast<unsigned long>(samplesPerChannel));
    if (m_audioError != paNoError && m_audioError != paOutputUnderflowed) {
        sgct::Log::Error("OmtLayer Error: Pa_WriteStream failed.\n");
    }
}

bool OmtLayer::StartAudioStream() {
    std::lock_guard<std::recursive_mutex> lock(m_audioMutex);
    if (!m_receiveAudio)
        return false;
    if (m_audioStreamOpen)
        return true;
    std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());

    // Determine output channel count
    if (audioMixingEnabled()) {
        const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(m_audioOutputParameters.device);
        if (devInfo) {
            m_audioOutputChannels = cplay::audio::configuredOutputChannels(devInfo->maxOutputChannels);
        }
    }
    else {
        m_audioOutputChannels = m_audioChannels;
    }

    m_audioOutputParameters.channelCount = m_audioOutputChannels;
    m_audioOutputParameters.sampleFormat = paFloat32;
    const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(m_audioOutputParameters.device);
    if (devInfo) {
        m_audioOutputParameters.suggestedLatency = devInfo->defaultLowOutputLatency;
    }
    else {
        m_audioOutputParameters.suggestedLatency = 0.05;
    }
    m_audioOutputParameters.hostApiSpecificStreamInfo = NULL;

    // Open stream without callback � we use Pa_WriteStream instead
    m_audioError = cplay::audio::paOpenStream(
        &m_audioStream,
        NULL,
        &m_audioOutputParameters,
        m_audioSampleRate,
        paFramesPerBufferUnspecified,
        paClipOff,
        NULL,   // no callback
        NULL    // no userData
    );

    if (m_audioError == paNoError) {
        m_audioStreamOpen = true;
        start();
        return true;
    }
    else {
        sgct::Log::Error("OmtLayer Error: Failed to open PortAudio stream.\n");
    }

    return false;
}

PaDeviceIndex OmtLayer::GetChosenApplicationAudioDevice() {
    PaDeviceIndex choseDeviceIdx = Pa_GetDefaultOutputDevice();
    if (choseDeviceIdx == paNoDevice) {
        sgct::Log::Error("OmtLayer Error: No default audio output device.\n");
    }
    if (AudioSettings::portAudioCustomOutput() && !cplay::audio::jackOutputSelected()) { // JACK: any device stands in
        if (!AudioSettings::portAudioOutputDevice().isEmpty()
            && !AudioSettings::portAudioOutputApi().isEmpty()) {
            int numDevices = Pa_GetDeviceCount();
            if (numDevices < 0) {
                return choseDeviceIdx;
            }
            const PaDeviceInfo* deviceInfo;
            const PaHostApiInfo* apiInfo;
            bool foundDevice = false;
            for (int i = 0; i < numDevices; i++) {
                deviceInfo = Pa_GetDeviceInfo(i);
                apiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
                if (deviceInfo->maxOutputChannels > 1) {
                    QString deviceName = QString::fromUtf8(deviceInfo->name);
                    QString apiName = QString::fromUtf8(apiInfo->name);
                    if (deviceName == AudioSettings::portAudioOutputDevice()
                        && apiName == AudioSettings::portAudioOutputApi()) {
                        choseDeviceIdx = i;
                        foundDevice = true;
                    }
                }
            }
            if (!foundDevice) {
                sgct::Log::Info("OmtLayer: Did not find desired audio device. Sticking with default device.");
            }
        }
    }
    return choseDeviceIdx;
}

bool OmtLayer::ready() const {
    return m_isReady;
}

bool OmtLayer::hasTexture() const {
    return true;
}

void OmtLayer::start() {
    std::lock_guard<std::recursive_mutex> lock(m_audioMutex);
    std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
    if (!m_audioStreamStarted && isAudioEnabled() && m_audioStream && m_audioStreamOpen) {
        setVolume(m_volume);
        m_audioError = cplay::audio::paStartStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted = true;
        }
    }
}

void OmtLayer::stop() {
    std::lock_guard<std::recursive_mutex> lock(m_audioMutex);
    std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
    if (isAudioEnabled() && m_audioStream && m_audioStreamOpen && m_audioStreamStarted) {
        m_audioError = cplay::audio::paStopStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted = false;
        }
    }
}

bool OmtLayer::hasAudio() const {
    return (isAudioEnabled() || (isMaster() && AudioSettings::enableAudioOnNodes()));
}

bool OmtLayer::isAudioEnabled() const {
    return m_isAudioEnabled;
}

void OmtLayer::enableAudio(bool enabled) {
    m_isAudioEnabled = enabled;
}

void OmtLayer::updateAudioOutput() {
    if (isMaster()) {
        if (!m_isAudioEnabled && AudioSettings::enableAudioOnMaster()) {
            enableAudio(true);
        }
        else if (m_isAudioEnabled && !AudioSettings::enableAudioOnMaster()) {
            enableAudio(false);
        }
    }
    if (isAudioEnabled()) {
        std::lock_guard<std::recursive_mutex> lock(m_audioMutex);
        std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
        PaDeviceIndex currentDeviceIdx = m_audioOutputParameters.device;
        PaDeviceIndex newDeviceIdx = GetChosenApplicationAudioDevice();

        int channelCount = m_audioOutputChannels;
        if (audioMixingEnabled()) {
            const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(newDeviceIdx);
            if (devInfo) {
                channelCount = cplay::audio::configuredOutputChannels(devInfo->maxOutputChannels);
            }
        }
        else {
            channelCount = m_audioChannels;
        }

        bool restartStream = false;
        if (newDeviceIdx != currentDeviceIdx) {
            restartStream = true;
        }
        else if (m_audioOutputParameters.channelCount != channelCount) {
            restartStream = true;
        }
        else if (cplay::audio::outputRoutingChanged(m_audioStream, newDeviceIdx)) {
            restartStream = true; // to/from the shared output (ASIO or WASAPI exclusive)
        }

        if (restartStream) {
            bool wasStarted = false;
            bool wasOpen = false;
            if (m_audioStream) {
                if (m_audioStreamOpen) {
                    wasOpen = true;
                    if (m_audioStreamStarted) {
                        wasStarted = true;
                        stop();
                    }
                    if (!m_audioStreamStarted) {
                        m_audioError = cplay::audio::paCloseStream(m_audioStream);
                        if (m_audioError == paNoError) {
                            m_audioStreamOpen = false;
                        }
                    }
                }
            }
            if (!m_audioStreamOpen) {
                m_audioOutputParameters.device = newDeviceIdx;
                m_audioOutputChannels = channelCount;

                if (wasStarted && wasOpen) {
                    StartAudioStream();
                }
            }
        }
    }
}

void OmtLayer::setVolume(int v, bool storeLevel) {
    if (storeLevel) {
        m_volume = v;
    }

    m_volume_Dec = v;
    m_audioVolume = static_cast<float>(v) / 100.0f;

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

void OmtLayer::setVolumeMute(bool v) {
    if (m_volumeMute == v)
        return;

    m_volumeMute = v;
    m_volumeMute_Dec = v;

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

// Helper to convert grid index to cols/rows
void OmtLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    m_subdivision->encodeAlways(data);
}

void OmtLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    m_subdivision->decodeAlways(data, pos);
}

void OmtLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_volume_Dec);
    sgct::serializeObject(data, m_volumeMute_Dec);
    m_subdivision->encodeProperties(data);
}

void OmtLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_volume_Dec);
    sgct::deserializeObject(data, pos, m_volumeMute_Dec);
    m_subdivision->decodeProperties(data, pos);

    m_typePropertiesDecoded = true;
}

void OmtLayer::GenerateTexture(unsigned int& id, int width, int height) {
    glGenTextures(1, &id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
