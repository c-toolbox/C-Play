/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "omtlayer.h"
#include "audiosettings.h"
#include <sgct/sgct.h>
#include <utils/dividetexturehandler.h>
#include <cstring>
#include <cmath>
#include <algorithm>

// ============================================================
// OmtFinder
// ============================================================
OmtFinder* OmtFinder::_instance = nullptr;

OmtFinder::OmtFinder() {
}

OmtFinder::~OmtFinder() {
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
}

OmtLayer::~OmtLayer() {
    if (isAudioEnabled() && m_receiveAudio) {
        if (m_audioStreamOpen && m_audioStreamStarted) {
            m_audioError = Pa_CloseStream(m_audioStream);
            if (m_audioError == paNoError) {
                m_audioStreamOpen = false;
            }
        }
        Pa_Terminate();
    }

    if (m_receiver) {
        omt_receive_destroy(m_receiver);
        m_receiver = nullptr;
    }

    if (renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId);
    }

    delete m_divideTexHandler;
}

void OmtLayer::initialize() {
    m_hasInitialized = true;

    // Only receive audio on master
    if (isAudioEnabled()) {
        m_audioError = Pa_Initialize();
        if (m_audioError != paNoError) {
            sgct::Log::Error("OmtLayer Error: PortAudio initialization failed.\n");
        }
        else {
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

    // Create receiver if needed — request both video and audio if audio is enabled
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

    // Upload pixel data to texture
    glBindTexture(GL_TEXTURE_2D, renderData.texId);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height,
        GL_BGRA, GL_UNSIGNED_BYTE, frame->Data);

    // Update divide texture sublayers if division mode is active
    if (m_textureDivisionMode == 2 && m_divideTexHandler && m_divideTexHandler->isActive()
        && renderData.texId > 0 && renderData.width > 0 && renderData.height > 0) {
        m_divideTexHandler->updateSubLayers(this, renderData.texId, renderData.width, renderData.height);
    }
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
    // We interleave and apply volume — no format conversion needed.
    const float* srcData = static_cast<const float*>(frame->Data);
    float vol = m_audioVolume;

    size_t totalSamples = static_cast<size_t>(samplesPerChannel) * static_cast<size_t>(outChannels);
    if (m_interleavedAudioBuf.size() < totalSamples) {
        m_interleavedAudioBuf.resize(totalSamples);
    }

    for (int s = 0; s < samplesPerChannel; ++s) {
        for (int oc = 0; oc < outChannels; ++oc) {
            // Map output channel to input channel (simple wrap for down/upmix)
            int ic = (oc < channels) ? oc : (oc % channels);
            float sample = srcData[static_cast<size_t>(ic) * static_cast<size_t>(samplesPerChannel) + static_cast<size_t>(s)];
            m_interleavedAudioBuf[static_cast<size_t>(s) * static_cast<size_t>(outChannels) + static_cast<size_t>(oc)] = sample * vol;
        }
    }

    // Write directly to the PortAudio stream (blocking)
    m_audioError = Pa_WriteStream(m_audioStream, m_interleavedAudioBuf.data(), static_cast<unsigned long>(samplesPerChannel));
    if (m_audioError != paNoError && m_audioError != paOutputUnderflowed) {
        sgct::Log::Error("OmtLayer Error: Pa_WriteStream failed.\n");
    }
}

bool OmtLayer::StartAudioStream() {
    if (!m_receiveAudio)
        return false;

    // Determine output channel count
    if (AudioSettings::portAudioMixInputToOutput()) {
        const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(m_audioOutputParameters.device);
        if (devInfo) {
            m_audioOutputChannels = std::min(AudioSettings::portAudioOutputChannels(), devInfo->maxOutputChannels);
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

    // Open stream without callback — we use Pa_WriteStream instead
    m_audioError = Pa_OpenStream(
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
    if (AudioSettings::portAudioCustomOutput()) {
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
    if (!m_audioStreamStarted && isAudioEnabled() && m_audioStream && m_audioStreamOpen) {
        setVolume(m_volume);
        m_audioError = Pa_StartStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted = true;
        }
    }
}

void OmtLayer::stop() {
    if (isAudioEnabled() && m_audioStream && m_audioStreamOpen && m_audioStreamStarted) {
        m_audioError = Pa_StopStream(m_audioStream);
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
        PaDeviceIndex currentDeviceIdx = m_audioOutputParameters.device;
        PaDeviceIndex newDeviceIdx = GetChosenApplicationAudioDevice();

        int channelCount = m_audioOutputChannels;
        if (AudioSettings::portAudioMixInputToOutput()) {
            const PaDeviceInfo* devInfo = Pa_GetDeviceInfo(newDeviceIdx);
            if (devInfo) {
                channelCount = std::min(AudioSettings::portAudioOutputChannels(), devInfo->maxOutputChannels);
            }
        }

        bool restartStream = false;
        if (newDeviceIdx != currentDeviceIdx) {
            restartStream = true;
        }
        else if (m_audioOutputParameters.channelCount != channelCount) {
            restartStream = true;
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
                        m_audioError = Pa_CloseStream(m_audioStream);
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

// Helper to convert grid index to cols/rows
static void omtGridIndexToColsRows(int grid, int& cols, int& rows) {
    switch (grid) {
    case 1: cols = 1; rows = 2; break;
    case 2: cols = 2; rows = 1; break;
    case 3: cols = 2; rows = 2; break;
    case 4: cols = 2; rows = 3; break;
    case 5: cols = 3; rows = 2; break;
    case 6: cols = 3; rows = 3; break;
    default: cols = 1; rows = 1; break;
    }
}

void OmtLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_volume_Dec);
    sgct::serializeObject(data, m_textureDivisionMode);
    sgct::serializeObject(data, m_textureDivisionGrid);

    // Serialize sublayer properties for division mode
    if (m_textureDivisionMode == 2 && m_divideTexHandler) {
        int numSubs = m_divideTexHandler->cellCount();
        sgct::serializeObject(data, numSubs);
        bool hasSubs = m_divideTexHandler->hasSubLayers();
        sgct::serializeObject(data, hasSubs);
        if (hasSubs) {
            auto& subs = m_divideTexHandler->subLayers();
            int actualCount = static_cast<int>(subs.size());
            sgct::serializeObject(data, actualCount);
            for (int i = 0; i < actualCount; ++i) {
                auto& sub = subs[i];
                if (sub) {
                    sub->encodeBaseAlways(data);
                    sub->encodeBaseProperties(data);
                }
            }
        }
    }
}

void OmtLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_volume_Dec);

    int divMode = 0;
    sgct::deserializeObject(data, pos, divMode);
    int divGrid = 0;
    sgct::deserializeObject(data, pos, divGrid);

    m_textureDivisionMode = divMode;
    m_textureDivisionGrid = divGrid;

    // Deserialize sublayer properties for division mode
    if (m_textureDivisionMode == 2) {
        int numSubs = 0;
        sgct::deserializeObject(data, pos, numSubs);
        bool hasSubs = false;
        sgct::deserializeObject(data, pos, hasSubs);
        if (hasSubs) {
            int actualCount = 0;
            sgct::deserializeObject(data, pos, actualCount);

            if (!m_divideTexHandler) {
                m_divideTexHandler = new DivideTextureHandler();
            }
            int cols = 1, rows = 1;
            omtGridIndexToColsRows(m_textureDivisionGrid, cols, rows);
            m_divideTexHandler->setDivision(cols, rows, this);

            if (m_divideTexHandler->hasSubLayers()) {
                auto& subs = m_divideTexHandler->subLayers();
                for (int i = 0; i < actualCount; ++i) {
                    if (i < static_cast<int>(subs.size()) && subs[i]) {
                        subs[i]->decodeBaseAlways(data, pos);
                        subs[i]->decodeBaseProperties(data, pos);
                    } else {
                        // Skip data for this sublayer
                        bool tmpBool; float tmpFloat;
                        sgct::deserializeObject(data, pos, tmpBool);
                        sgct::deserializeObject(data, pos, tmpBool);
                        sgct::deserializeObject(data, pos, tmpFloat);
                        uint8_t gm, sm;
                        sgct::deserializeObject(data, pos, gm);
                        sgct::deserializeObject(data, pos, sm);
                        bool roiEn;
                        sgct::deserializeObject(data, pos, roiEn);
                        if (roiEn) {
                            float rx, ry, rz, rw;
                            sgct::deserializeObject(data, pos, rx);
                            sgct::deserializeObject(data, pos, ry);
                            sgct::deserializeObject(data, pos, rz);
                            sgct::deserializeObject(data, pos, rw);
                        }
                        if (gm == BaseLayer::GridMode::Plane) {
                            double d; float f; uint8_t u;
                            sgct::deserializeObject(data, pos, d);
                            sgct::deserializeObject(data, pos, d);
                            sgct::deserializeObject(data, pos, d);
                            sgct::deserializeObject(data, pos, d);
                            sgct::deserializeObject(data, pos, d);
                            sgct::deserializeObject(data, pos, d);
                            sgct::deserializeObject(data, pos, f);
                            sgct::deserializeObject(data, pos, f);
                            sgct::deserializeObject(data, pos, u);
                        } else {
                            float rx, ry, rz;
                            sgct::deserializeObject(data, pos, rx);
                            sgct::deserializeObject(data, pos, ry);
                            sgct::deserializeObject(data, pos, rz);
                        }
                    }
                }
            } else {
                for (int i = 0; i < actualCount; ++i) {
                    bool tmpBool; float tmpFloat;
                    sgct::deserializeObject(data, pos, tmpBool);
                    sgct::deserializeObject(data, pos, tmpBool);
                    sgct::deserializeObject(data, pos, tmpFloat);
                    uint8_t gm, sm;
                    sgct::deserializeObject(data, pos, gm);
                    sgct::deserializeObject(data, pos, sm);
                    bool roiEn;
                    sgct::deserializeObject(data, pos, roiEn);
                    if (roiEn) {
                        float rx, ry, rz, rw;
                        sgct::deserializeObject(data, pos, rx);
                        sgct::deserializeObject(data, pos, ry);
                        sgct::deserializeObject(data, pos, rz);
                        sgct::deserializeObject(data, pos, rw);
                    }
                    if (gm == BaseLayer::GridMode::Plane) {
                        double d; float f; uint8_t u;
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, d);
                        sgct::deserializeObject(data, pos, f);
                        sgct::deserializeObject(data, pos, f);
                        sgct::deserializeObject(data, pos, u);
                    } else {
                        float rx, ry, rz;
                        sgct::deserializeObject(data, pos, rx);
                        sgct::deserializeObject(data, pos, ry);
                        sgct::deserializeObject(data, pos, rz);
                    }
                }
            }
        }
    }

    m_typePropertiesDecoded = true;
}

int OmtLayer::textureDivisionMode() const {
    return m_textureDivisionMode;
}

void OmtLayer::setTextureDivisionMode(int mode) {
    if (m_textureDivisionMode == mode)
        return;

    m_textureDivisionMode = mode;

    if (mode == 2) {
        if (!m_divideTexHandler) {
            m_divideTexHandler = new DivideTextureHandler();
        }
        int cols = 1, rows = 1;
        omtGridIndexToColsRows(m_textureDivisionGrid, cols, rows);
        m_divideTexHandler->setDivision(cols, rows, this);
    } else {
        if (m_divideTexHandler) {
            m_divideTexHandler->clearAll();
        }
    }

    if (isMaster())
        setNeedSync();
}

int OmtLayer::textureDivisionGrid() const {
    return m_textureDivisionGrid;
}

void OmtLayer::setTextureDivisionGrid(int grid) {
    if (m_textureDivisionGrid == grid)
        return;

    m_textureDivisionGrid = grid;

    if (m_textureDivisionMode == 2) {
        if (!m_divideTexHandler) {
            m_divideTexHandler = new DivideTextureHandler();
        }
        int cols = 1, rows = 1;
        omtGridIndexToColsRows(grid, cols, rows);
        m_divideTexHandler->setDivision(cols, rows, this);
    }

    if (isMaster())
        setNeedSync();
}

bool OmtLayer::hasSubLayers() const {
    if (m_textureDivisionMode == 2 && m_divideTexHandler)
        return m_divideTexHandler->hasSubLayers();
    return false;
}

std::vector<std::shared_ptr<BaseLayer>>& OmtLayer::getSubLayers() const {
    if (m_textureDivisionMode == 2 && m_divideTexHandler && m_divideTexHandler->hasSubLayers())
        return m_divideTexHandler->subLayers();
    static std::vector<std::shared_ptr<BaseLayer>> empty;
    return empty;
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
