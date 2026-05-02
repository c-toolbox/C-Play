/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ndilayer.h"
#include "audiosettings.h"
#include <sgct/sgct.h>
#include <chrono>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <climits>
#include <cmath>
#include <utils/qrcommandprocessor.h>
#include <utils/qroperationhandler.h>
#include <utils/qroperationconfig.h>
#include <utils/dividetexturehandler.h>
#include <layers/texturelayer.h>

// simple clamp for 16-bit samples
static inline int16_t clamp16(int32_t v) noexcept {
    if (v > INT16_MAX) return INT16_MAX;
    if (v < INT16_MIN) return INT16_MIN;
    return static_cast<int16_t>(v);
}

static inline void zeroOutput(int16_t* out, int outChannels, std::size_t frames) noexcept {
    if (!out) return;
    std::memset(out, 0, frames * static_cast<std::size_t>(outChannels) * sizeof(int16_t));
}

// Real-time safe upmix/downmix helper (no allocations).
// Supports only: 2.0, 2.1, 5.0, 5.1, 7.0, 7.1. Other mappings are ignored (zeros).
// If in and out are the same, simple copy applies
// in: pointer to input interleaved int16 samples (frames * inChannels)
// out: pointer to output interleaved int16 samples (frames * outChannels)
// frames: number of frames to process
static void mixToOutputChannels(const int16_t* in, int inChannels,
                       int16_t* out, int outChannels,
                       std::size_t frames) noexcept
{
    if (!in || !out || frames == 0 || inChannels <= 0 || outChannels <= 0)
        return;

    // identical layout -> fast memcpy
    if (inChannels == outChannels) {
        std::size_t samples = frames * static_cast<std::size_t>(inChannels);
        std::memcpy(out, in, samples * sizeof(int16_t));
        return;
    }

    // Allowed system channel counts
    const auto allowed = [](int c)->bool {
        return c == 2 || c == 3 || c == 5 || c == 6 || c == 7 || c == 8;
    };
    // If either in or out channel count is not one of the supported systems, ignore mapping -> zero output.
    if (!allowed(outChannels) || !allowed(inChannels)) {
        zeroOutput(out, outChannels, frames);
        return;
    }

    // Helper: stereo -> generic 5.1-like mapping (used as base for several targets)
    auto stereoToSurround = [&](int targetChannels, const int16_t* inF, int16_t* outF) noexcept {
        int32_t L = inF[0];
        int32_t R = inF[1];

        int32_t FL = L;
        int32_t FR = R;
        float center_f = 0.35355339f * static_cast<float>(L + R);
        int32_t C = static_cast<int32_t>(center_f > 0 ? center_f + 0.5f : center_f - 0.5f);
        int32_t LFE = (L + R) / 4;
        int32_t SL = static_cast<int32_t>(L * 0.5f);
        int32_t SR = static_cast<int32_t>(R * 0.5f);
        int32_t RL = static_cast<int32_t>(L * 0.25f);
        int32_t RR = static_cast<int32_t>(R * 0.25f);

        // map according to targetChannels:
        // 3 (2.1): FL, FR, LFE
        // 5 (5.0): FL, FR, C, SL, SR
        // 6 (5.1): FL, FR, C, LFE, SL, SR
        // 7 (7.0): FL, FR, C, SL, SR, RL, RR
        // 8 (7.1): FL, FR, C, LFE, SL, SR, RL, RR
        switch (targetChannels) {
            case 3:
                outF[0] = clamp16(FL);
                outF[1] = clamp16(FR);
                outF[2] = clamp16(LFE);
                break;
            case 5:
                outF[0] = clamp16(FL);
                outF[1] = clamp16(FR);
                outF[2] = clamp16(C);
                outF[3] = clamp16(SL);
                outF[4] = clamp16(SR);
                break;
            case 6:
                outF[0] = clamp16(FL);
                outF[1] = clamp16(FR);
                outF[2] = clamp16(C);
                outF[3] = clamp16(LFE);
                outF[4] = clamp16(SL);
                outF[5] = clamp16(SR);
                break;
            case 7:
                outF[0] = clamp16(FL);
                outF[1] = clamp16(FR);
                outF[2] = clamp16(C);
                outF[3] = clamp16(SL);
                outF[4] = clamp16(SR);
                outF[5] = clamp16(RL);
                outF[6] = clamp16(RR);
                break;
            case 8:
                outF[0] = clamp16(FL);
                outF[1] = clamp16(FR);
                outF[2] = clamp16(C);
                outF[3] = clamp16(LFE);
                outF[4] = clamp16(SL);
                outF[5] = clamp16(SR);
                outF[6] = clamp16(RL);
                outF[7] = clamp16(RR);
                break;
            default:
                // unsupported target inside allowed set (should not happen)
                break;
        }
    };

    // Special-case: stereo input -> supported surround outputs
    if (inChannels == 2 && (outChannels == 3 || outChannels == 5 || outChannels == 6 || outChannels == 7 || outChannels == 8)) {
        for (std::size_t f = 0; f < frames; ++f) {
            const int16_t* inF = in + f * 2;
            int16_t* outF = out + f * static_cast<std::size_t>(outChannels);
            stereoToSurround(outChannels, inF, outF);
        }
        return;
    }

    // Downmix to stereo (2.0) from supported multi-channel sources (3,5,6,7,8)
    if (outChannels == 2 && (inChannels == 3 || inChannels == 5 || inChannels == 6 || inChannels == 7 || inChannels == 8)) {
        for (std::size_t f = 0; f < frames; ++f) {
            const int16_t* inF = in + f * static_cast<std::size_t>(inChannels);
            int16_t* outF = out + f * 2;
            int32_t sumL = 0, sumR = 0;

            // assign based on common channel layouts:
            // for 3: FL, FR, LFE (map LFE evenly)
            // for 5: FL, FR, C, SL, SR  -> C to both, SL->L, SR->R
            // for 6: FL, FR, C, LFE, SL, SR
            // for 7/8 approximate by assigning FL->L, FR->R, surrounds alternating
            sumL += inF[0]; // FL
            sumR += (inChannels > 1) ? inF[1] : inF[0]; // FR or FL fallback

            if (inChannels >= 3) { // C or LFE depending on spec
                // treat channel 2 as center/LFE contribution to both
                sumL += inF[2];
                sumR += inF[2];
            }
            // remaining channels: distribute alternately to L/R
            for (int ic = 3; ic < inChannels; ++ic) {
                if ((ic & 1) == 1) sumL += inF[ic];
                else sumR += inF[ic];
            }

            // compute per-side divisor
            int32_t leftCount = (inChannels + 1) / 2;
            int32_t rightCount = inChannels / 2;
            if (rightCount == 0) rightCount = 1;

            outF[0] = clamp16(sumL / leftCount);
            outF[1] = clamp16(sumR / rightCount);
        }
        return;
    }

    // If both in/out are allowed but mapping not explicitly implemented, do a conservative replicate/distribute.
    if (inChannels < outChannels) {
        for (std::size_t f = 0; f < frames; ++f) {
            const int16_t* inF = in + f * static_cast<std::size_t>(inChannels);
            int16_t* outF = out + f * static_cast<std::size_t>(outChannels);
            for (int oc = 0; oc < outChannels; ++oc) {
                int ic = (oc < inChannels) ? oc : (oc % inChannels);
                outF[oc] = inF[ic];
            }
        }
        return;
    }

    // If we reach here mapping is not supported for the requested variant -> zero output.
    zeroOutput(out, outChannels, frames);
}

/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int paNDIAudioCallback(const void*, void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo*,
    PaStreamCallbackFlags,
    void* userData)
{
    ofxNDIreceive* reciever = (ofxNDIreceive*)userData;
    if(reciever) {
        if (reciever->shouldReceiveAudio()) {
            void* receviedFrames = reciever->ReceiveAudioOnlyFrameSync((int)framesPerBuffer);
            if (receviedFrames) {
                int inChannels = reciever->GetAudioChannels();
                int outChannels = reciever->GetAudioTargetOutputChannels();
                if (inChannels == outChannels) {
                    memcpy(outputBuffer, receviedFrames, ((size_t)framesPerBuffer * (size_t)inChannels * sizeof(int16_t)));
                }
                else {
                    mixToOutputChannels(reinterpret_cast<const int16_t*>(receviedFrames),
                        inChannels,
                        reinterpret_cast<int16_t*>(outputBuffer),
                        outChannels,
                        static_cast<std::size_t>(framesPerBuffer));
                }
            }
        }
    }

    return paContinue;
}

NdiFinder* NdiFinder::_instance = nullptr;

NdiFinder::NdiFinder() {
    m_NDIreceiver.CreateFinder();
}

NdiFinder::~NdiFinder() {
    m_NDIreceiver.ReleaseFinder();
}

void NdiFinder::destroy() {
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

NdiFinder& NdiFinder::instance() {
    if (!_instance) {
        _instance = new NdiFinder();
    }
    return *_instance;
}

int NdiFinder::findSenders() {
    return m_NDIreceiver.FindSenders();
}

std::vector<std::string> NdiFinder::getSendersList() {
    return m_NDIreceiver.GetSenderList();
}

bool NdiFinder::senderExists(std::string senderName) {
    int senderIdx;
    return m_NDIreceiver.GetSenderIndex(senderName, senderIdx);
}

std::string NdiFinder::getNDIVersionString() {
    return m_NDIreceiver.GetNDIversion();
}

NdiLayer::NdiLayer() {
    setType(BaseLayer::LayerType::NDI);
    NDIreceiver.ResetFps(30.0);

    // =======================================
    // Set to prefer BGRA
    NDIreceiver.SetFormat(NDIlib_recv_color_format_BGRX_BGRA);
    // NDI SDK says we should ask for the format we want in the end, which is BGR or RGB in this case
    // The SDK will most likely convert the video to the requested format using AXV2 instructions etc
    // So while NDIlib_recv_color_format_fastest or NDIlib_recv_color_format_best will work with implemented conversion
    // we better stick with the above formats for best performance.

    m_qrProcessor = new QRCommandProcessor();
    m_qrProcessor->setCommandCallback([this](const QRCommand& cmd) {
        onQRCommand(cmd);
    });

    m_qrOpHandler = new QROperationHandler();

    OpenReceiver();
}

NdiLayer::~NdiLayer() {
    if (isAudioEnabled() && m_recevieAudio) {
        if (m_audioStreamOpen && m_audioStreamStarted) {
            m_audioError = Pa_CloseStream(m_audioStream);
            if (m_audioError == paNoError) {
                m_audioStreamOpen = false;
            }
        }
        Pa_Terminate();
        NDIreceiver.SetAudio(false);
    }

    NDIreceiver.ReleaseReceiver();

    if (m_pbo[0]) {
        glDeleteBuffers(3, m_pbo);
    }

    if (renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId);
    }

    // Free conversion buffer
    if (m_conversionBuffer) {
        free(m_conversionBuffer);
        m_conversionBuffer = nullptr;
    }

    delete m_qrProcessor;
    delete m_qrOpHandler;
    delete m_divideTexHandler;
}

void NdiLayer::initialize() {
    m_hasInitialized = true;
    NDIreceiver.SetSenderName(filepath());

    // Only recevie audio on master
    if (isAudioEnabled()) {
        m_audioError = Pa_Initialize();
        if (m_audioError != paNoError) {
            NDIreceiver.SetAudio(false);
        }
        else {
            NDIreceiver.SetAudio(true, true);
            m_recevieAudio = true;
            m_audioOutputParameters.device = Pa_GetDefaultOutputDevice();
            if (m_audioOutputParameters.device == paNoDevice) {
                sgct::Log::Error("NdiLayer Error: No default audio output device.\n");
            }
        }
    }
}

void NdiLayer::update(bool updateRendering) {
    std::lock_guard<std::mutex> lock(m_updateMutex);

    if (m_typePropertiesDecoded) {
        m_typePropertiesDecoded = false;
        setVolume(m_volume_Dec);
        setQRCodeDetectionEnabled(m_qrCodeDetectionEnabled_Dec);
    }

    // Check sender amount
    int senders = NdiFinder::instance().findSenders();
    if (senders < 1) {
        m_isReady = false;
        return;
    }

    // Check if our sender exists
    m_isReady = NdiFinder::instance().senderExists(filepath());
    if (!m_isReady) {
        if (!isMaster()) {
            // Only refresh senders at most once every 2 seconds
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastRefreshTime).count() >= 2000) {
                NDIreceiver.RefreshSenders();
                m_lastRefreshTime = now;
            }
        }
        return;
    }

    NDIreceiver.SetSenderName(filepath());

    // Check for receiver creation
    // And find sender
    if (!OpenReceiver()) {
        m_isReady = false;
        return;
    }
    else {
        m_isReady = true;
    }

    // Let's recieve image or audio
    if (m_isReady) {
        ReceiveData(updateRendering);
    }
}

void NdiLayer::updateFrame() {
    // Let's recieve image or audio
    if (m_isReady) {
        ReceiveData(true);
    }
}

bool NdiLayer::ready() const {
    return m_isReady;
}

bool NdiLayer::hasTexture() const {
    return true;
}

void NdiLayer::start() {
    if (!m_audioStreamStarted && isAudioEnabled() && m_audioStream && m_audioStreamOpen) {
        setVolume(m_volume);
        m_audioError = Pa_StartStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted = true;
        }     
    }
}

void NdiLayer::stop() {
    if (isAudioEnabled() && m_audioStream && m_audioStreamOpen && m_audioStreamStarted) {
        m_audioError = Pa_StopStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted = false;
        }
    }
}

bool NdiLayer::hasAudio() const {
    return (isAudioEnabled() || (isMaster() && AudioSettings::enableAudioOnNodes()));
}

bool NdiLayer::isAudioEnabled() const {
    return m_isAudioEnabled;
}

void NdiLayer::enableAudio(bool enabled) {
    m_isAudioEnabled = enabled;
}

void NdiLayer::updateAudioOutput() {
    if (isMaster()) {
        if (!m_isAudioEnabled && AudioSettings::enableAudioOnMaster()) {
            enableAudio(true);
        }
        else if (m_isAudioEnabled && !AudioSettings::enableAudioOnMaster()) {
            enableAudio(false);
        }
    }
    if (isAudioEnabled()) {
        //See if device has changed
        PaDeviceIndex currentDeviceIdx = m_audioOutputParameters.device;
        PaDeviceIndex newDeviceIdx = GetChosenApplicationAudioDevice();

        int channelCount = 0;
        if (AudioSettings::portAudioMixInputToOutput()) {
            channelCount = std::min(AudioSettings::portAudioOutputChannels(), Pa_GetDeviceInfo(newDeviceIdx)->maxOutputChannels);
        }
        else {
            channelCount = NDIreceiver.GetAudioChannels();
        }

        bool restartStream = false;
        if (newDeviceIdx != currentDeviceIdx) {
            restartStream = true;
        }
        else if(m_audioOutputParameters.channelCount != channelCount){
            restartStream = true;
        }

        //Change device index if we found a new one
        if (restartStream) {
            //Close stream to restart it
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
            //If stream is closed, let's switch device
            if (!m_audioStreamOpen) {
                m_audioOutputParameters.device = newDeviceIdx;

                //Let's start again if started
                if (wasStarted && wasOpen) {
                    StartAudioStream();
                }
            }
        }
    }
}

void NdiLayer::setVolume(int v, bool storeLevel) {
    if (storeLevel) {
        m_volume = v;
    }

    if (isAudioEnabled()) {
        m_volume_Dec = v;
        NDIreceiver.SetAudioVolume(static_cast<float>(v) / 100.f);
    }

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

// Helper to convert grid index to cols/rows
// 0=1x1, 1=1x2, 2=2x1, 3=2x2, 4=2x3, 5=3x2, 6=3x3
static void ndiGridIndexToColsRows(int grid, int& cols, int& rows) {
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

void NdiLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    // Always write whether division-mode sublayer data follows
    bool divisionActive = (m_textureDivisionMode == 2 && m_divideTexHandler);
    sgct::serializeObject(data, divisionActive);
    if (divisionActive) {
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
                    sub->encodeTypeAlways(data);
                }
            }
        }
    }
}

void NdiLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    bool divisionActive = false;
    sgct::deserializeObject(data, pos, divisionActive);
    if (divisionActive) {
        bool hasSubs = false;
        sgct::deserializeObject(data, pos, hasSubs);
        if (hasSubs) {
            int actualCount = 0;
            sgct::deserializeObject(data, pos, actualCount);

            if (m_divideTexHandler && m_divideTexHandler->hasSubLayers()) {
                auto& subs = m_divideTexHandler->subLayers();
                for (int i = 0; i < actualCount; ++i) {
                    if (i < static_cast<int>(subs.size()) && subs[i]) {
                        subs[i]->decodeBaseAlways(data, pos);
                        subs[i]->decodeTypeAlways(data, pos);
                    } else {
                        // Skip: shouldUpdate, shouldPreLoad, alpha
                        bool tmpBool; float tmpFloat;
                        sgct::deserializeObject(data, pos, tmpBool);
                        sgct::deserializeObject(data, pos, tmpBool);
                        sgct::deserializeObject(data, pos, tmpFloat);
                    }
                }
            } else {
                for (int i = 0; i < actualCount; ++i) {
                    bool tmpBool; float tmpFloat;
                    sgct::deserializeObject(data, pos, tmpBool);
                    sgct::deserializeObject(data, pos, tmpBool);
                    sgct::deserializeObject(data, pos, tmpFloat);
                }
            }
        }
    }
}

void NdiLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_volume_Dec);
    sgct::serializeObject(data, isQRCodeDetectionEnabled());
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
                    sub->encodeBaseProperties(data);
                    sub->encodeTypeProperties(data);
                }
            }
        }
    }
}

void NdiLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_volume_Dec);
    sgct::deserializeObject(data, pos, m_qrCodeDetectionEnabled_Dec);

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

            // Ensure DivideTextureHandler exists and has proper division set
            if (!m_divideTexHandler) {
                m_divideTexHandler = new DivideTextureHandler();
            }
            int cols = 1, rows = 1;
            ndiGridIndexToColsRows(m_textureDivisionGrid, cols, rows);
            m_divideTexHandler->setDivision(cols, rows, this);

            // If sublayers exist, decode into them; otherwise just advance pos
            if (m_divideTexHandler->hasSubLayers()) {
                auto& subs = m_divideTexHandler->subLayers();
                for (int i = 0; i < actualCount; ++i) {
                    if (i < static_cast<int>(subs.size()) && subs[i]) {
                        subs[i]->decodeBaseProperties(data, pos);
                        subs[i]->decodeTypeProperties(data, pos);
                    } else {
                        // Skip data for this sublayer
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
                // Sublayers don't exist yet on this node, skip their data
                for (int i = 0; i < actualCount; ++i) {
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

bool NdiLayer::isQRCodeDetectionEnabled() const {
    if (m_qrProcessor) {
        return m_qrProcessor->isEnabled();
    }
    return false;
}

void NdiLayer::setQRCodeDetectionEnabled(bool enabled) {
    if (m_qrProcessor) {
        m_qrProcessor->setEnabled(enabled);
    }
    if (!enabled && m_qrOpHandler) {
        m_qrOpHandler->clearAll();
    }

    if (isMaster())
        setNeedSync();
}

bool NdiLayer::loadQROperationConfig(const std::string& filePath) {
    if (m_qrOpHandler) {
        return m_qrOpHandler->loadConfig(filePath);
    }
    return false;
}

const QROperationConfig* NdiLayer::qrOperationConfig() const {
    if (m_qrOpHandler) {
        return m_qrOpHandler->config();
    }
    return nullptr;
}

std::string NdiLayer::activePlaneName() const {
    if (m_qrOpHandler) {
        return m_qrOpHandler->activePlaneName();
    }
    return std::string();
}

int NdiLayer::textureDivisionMode() const {
    return m_textureDivisionMode;
}

void NdiLayer::setTextureDivisionMode(int mode) {
    if (m_textureDivisionMode == mode)
        return;

    m_textureDivisionMode = mode;

    if (mode == 1) {
        setQRCodeDetectionEnabled(true);
        if (m_divideTexHandler) {
            m_divideTexHandler->clearAll();
        }
    } else if (mode == 2) {
        setQRCodeDetectionEnabled(false);
        if (!m_divideTexHandler) {
            m_divideTexHandler = new DivideTextureHandler();
        }
        int cols = 1, rows = 1;
        ndiGridIndexToColsRows(m_textureDivisionGrid, cols, rows);
        m_divideTexHandler->setDivision(cols, rows, this);
    } else {
        setQRCodeDetectionEnabled(false);
        if (m_divideTexHandler) {
            m_divideTexHandler->clearAll();
        }
    }

    if (isMaster())
        setNeedSync();
}

int NdiLayer::textureDivisionGrid() const {
    return m_textureDivisionGrid;
}

void NdiLayer::setTextureDivisionGrid(int grid) {
    if (m_textureDivisionGrid == grid)
        return;

    m_textureDivisionGrid = grid;

    if (m_textureDivisionMode == 2) {
        if (!m_divideTexHandler) {
            m_divideTexHandler = new DivideTextureHandler();
        }
        int cols = 1, rows = 1;
        ndiGridIndexToColsRows(grid, cols, rows);
        m_divideTexHandler->setDivision(cols, rows, this);
    }

    if (isMaster())
        setNeedSync();
}

// Receive ofTexture
bool NdiLayer::ReceiveData(bool updateRendering) {
    // Receive a pixel image first
    unsigned int width = (unsigned int)renderData.width;
    unsigned int height = (unsigned int)renderData.height;

    // If we have opened an audio stream already, let's only capture image here
    // Audio is captured in separate callback
    bool receviedAudio = false;
    bool receviedImage = false;
    if (updateRendering && (!isMaster() || (m_audioStreamOpen && m_recevieAudioThroughCallback))) {
        if (m_hasCapturedImage || NDIreceiver.FrameSyncOn()) { // We can start using frame sync now
            try {
                receviedImage = NDIreceiver.ReceiveImageOnlyFrameSync(width, height);
            }
            catch (const std::exception& e) {
                sgct::Log::Error(std::format("NdiLayer Error in ReceiveImageOnlyFrameSync: {}", e.what()));
                return false;
            }
        }
        else {
            try {
                receviedImage = NDIreceiver.ReceiveImageOnly(width, height);
            }
            catch (const std::exception& e) {
                sgct::Log::Error(std::format("NdiLayer Error in ReceiveImageOnly: {}", e.what()));
                return false;
            }
            if (isAudioEnabled()) {
                try {
                    receviedAudio = NDIreceiver.ReceiveAudioOnly();
                }
                catch (const std::exception& e) {
                    sgct::Log::Error(std::format("NdiLayer Error in ReceiveAudioOnly: {}", e.what()));
                    return false;
                }
            }
        }
    }
    else if (isMaster()) { // Still want to run som passes here to start capturing of audio.
        // If updateRendering is Off, and audio stream already open we do not need to continue here
        if (!updateRendering && m_audioStreamOpen)
            return false;

        if (updateRendering) {
            //Receving both image and audio here

            try {
                receviedImage = NDIreceiver.ReceiveImageAndAudio(width, height);
            }
            catch (const std::exception& e) {
                sgct::Log::Error(std::format("NdiLayer Error in ReceiveImageAndAudio: {}", e.what()));
                return false;
            }
            if (!receviedImage) {
                receviedAudio = NDIreceiver.IsAudioFrame();
            }
        }
        else if (isMaster()) {
            try {
                receviedAudio = NDIreceiver.ReceiveAudioOnly();
            }
            catch (const std::exception& e) {
                sgct::Log::Error(std::format("NdiLayer Error in ReceiveAudioOnly: {}", e.what()));
                return false;
            }
        }
    }

    if (receviedImage) {
        // Check for changed sender dimensions
        if (width != (unsigned int)renderData.width || height != (unsigned int)renderData.height) {
            if (renderData.width > 0)
                glDeleteTextures(1, &renderData.texId);

            GenerateTexture(renderData.texId, width, height);

            renderData.width = (int)width;
            renderData.height = (int)height;
        }
        // Get NDI pixel data from the video frame
        try {
            return GetPixelData(renderData.texId, width, height);
        }
        catch (const std::exception& e) {
            sgct::Log::Error(std::format("NdiLayer Error in GetPixelData: {}", e.what()));
            return false;
        }
    }

    if (receviedAudio || (!isMaster() && isAudioEnabled())) {
        if (!m_audioStreamOpen) {
            StartAudioStream();
        }

        if (!m_recevieAudioThroughCallback && m_audioStreamOpen && m_audioStreamStarted) {
            m_audioError = Pa_WriteStream(m_audioStream, NDIreceiver.GetAudioInterleaved(), NDIreceiver.GetAudioSamples());
            if (m_audioError != paNoError) {
                sgct::Log::Error("NdiLayer Error: audio stream error");
            }
        }

        return true;
    }

    return false;
}

// Create receiver if not initialized or a new sender has been selected
bool NdiLayer::OpenReceiver() {
    if (NDIreceiver.OpenReceiver()) {
        // Initialize pbos for asynchronous pixel load
        if (!m_pbo[0]) {
            glGenBuffers(3, m_pbo);
            PboIndex = NextPboIndex = 0;
        }
        return true;
    }
    return false;
}

bool NdiLayer::StartAudioStream() {
    if (AudioSettings::portAudioMixInputToOutput()) {
        m_audioOutputParameters.channelCount = std::min(AudioSettings::portAudioOutputChannels(), Pa_GetDeviceInfo(m_audioOutputParameters.device)->maxOutputChannels);
        NDIreceiver.SetAudioTargetOutputChannels(m_audioOutputParameters.channelCount);
    }
    else {
        m_audioOutputParameters.channelCount = NDIreceiver.GetAudioChannels();
        NDIreceiver.SetAudioTargetOutputChannels(-1);
    }
    
    m_audioOutputParameters.sampleFormat = paInt16; // 16 bit integer point output
    m_audioOutputParameters.suggestedLatency = Pa_GetDeviceInfo(m_audioOutputParameters.device)->defaultLowOutputLatency;
    m_audioOutputParameters.hostApiSpecificStreamInfo = NULL;

    if (m_recevieAudioThroughCallback) {
        m_audioError = Pa_OpenStream(&m_audioStream, NULL, &m_audioOutputParameters, NDIreceiver.GetAudioSampleRate(), NDIreceiver.GetAudioSamples(), paClipOff, paNDIAudioCallback, &NDIreceiver);
    }
    else {
        m_audioError = Pa_OpenStream(&m_audioStream, NULL, &m_audioOutputParameters, NDIreceiver.GetAudioSampleRate(), NDIreceiver.GetAudioSamples(), paClipOff, NULL, NULL);
    }

    if (m_audioError == paNoError) {
        m_audioStreamOpen = true;
        start();
        return true;
    }

    return false;
}

PaDeviceIndex NdiLayer::GetChosenApplicationAudioDevice() {
    PaDeviceIndex choseDeviceIdx = Pa_GetDefaultOutputDevice(); /* default output device */
    if (choseDeviceIdx == paNoDevice) {
        sgct::Log::Error("NdiLayer Error: No default audio output device.\n");
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
            sgct::Log::Info(std::format("NdiLayer: Trying to find audio device named \"{}\" using the \"{}\" api.",
                AudioSettings::portAudioOutputDevice().toStdString(), AudioSettings::portAudioOutputApi().toStdString()));
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
                        sgct::Log::Info(std::format("NdiLayer: Found desired audio device."));
                    }
                }
            }
            if (!foundDevice) {
                sgct::Log::Info(std::format("NdiLayer: Did not find desired audio device. Sticking with default device."));
            }
        }
    }
    return choseDeviceIdx;
}

//Find barcodes in the received frame data using two-phase QR command scheme
bool NdiLayer::FindCodes(unsigned char* data, unsigned int width, unsigned int height, int GLformat) {
    if (!m_qrProcessor || !m_qrProcessor->isEnabled()) {
        return false;
    }
    return m_qrProcessor->processFrame(data, width, height, GLformat);
}

void NdiLayer::onQRCommand(const QRCommand& command) {
    if (m_qrOpHandler) {
        m_qrOpHandler->handleCommand(command, this, renderData.texId, renderData.width, renderData.height);
    }
}

bool NdiLayer::hasSubLayers() const {
    if (m_textureDivisionMode == 2 && m_divideTexHandler)
        return m_divideTexHandler->hasSubLayers();
    return m_qrOpHandler ? m_qrOpHandler->hasSubLayers() : false;
}

std::vector<std::shared_ptr<BaseLayer>>& NdiLayer::getSubLayers() const {
    if (m_textureDivisionMode == 2 && m_divideTexHandler && m_divideTexHandler->hasSubLayers())
        return m_divideTexHandler->subLayers();
    if (m_qrOpHandler) {
        return m_qrOpHandler->subLayers();
    }
    static std::vector<std::shared_ptr<BaseLayer>> empty;
    return empty;
}
    
// Get NDI pixel data from the video frame to ofTexture
bool NdiLayer::GetPixelData(GLuint TextureID, unsigned int width, unsigned int height) {
    // Get the video frame buffer pointer
    unsigned char *videoData = NDIreceiver.GetVideoData();
    if (!videoData) {
        // Ensure the video buffer is freed
        NDIreceiver.FreeVideoData();
        return false;
    }

    // Get the current video format
    NDIlib_FourCC_video_type_e currentFormat = NDIreceiver.GetVideoType();
    unsigned int stride = NDIreceiver.GetVideoStride();
    
    // Calculate required buffer size for RGBA conversion
    size_t requiredBufferSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    
    // Check if we need to reallocate the conversion buffer
    // (size change or format change requiring conversion)
    bool needsConversion = (currentFormat != NDIlib_FourCC_type_BGRA && 
                           currentFormat != NDIlib_FourCC_type_BGRX &&
                           currentFormat != NDIlib_FourCC_type_RGBA &&
                           currentFormat != NDIlib_FourCC_type_RGBX);
    
    if (needsConversion) {
        if (m_conversionBufferSize != requiredBufferSize || m_lastVideoFormat != currentFormat) {
            // Reallocate conversion buffer
            if (m_conversionBuffer) {
                free(m_conversionBuffer);
            }
            m_conversionBuffer = static_cast<unsigned char*>(malloc(requiredBufferSize));
            m_conversionBufferSize = requiredBufferSize;
            m_lastVideoFormat = currentFormat;
            
            if (!m_conversionBuffer) {
                sgct::Log::Error("NdiLayer: Failed to allocate conversion buffer");
                NDIreceiver.FreeVideoData();
                return false;
            }
        }
    }
    else {
        // No conversion needed, free conversion buffer if allocated
        if (m_conversionBuffer) {
            free(m_conversionBuffer);
            m_conversionBuffer = nullptr;
            m_conversionBufferSize = 0;
        }
        m_lastVideoFormat = currentFormat;
    }

    // Pointer to the data we'll upload to the texture
    unsigned char* pixelData = videoData;
    int GLformat = GL_BGRA; // Default format

    // Convert based on NDI format
    switch (currentFormat) {
        // YUV 4:2:2 formats - 8-bit
        case NDIlib_FourCC_type_UYVY: // YCbCr 4:2:2
            ofxNDIutils::YUV422_to_RGBA(videoData, m_conversionBuffer, width, height, stride);
            pixelData = m_conversionBuffer;
            GLformat = GL_RGBA;
            break;
            
        case NDIlib_FourCC_type_UYVA: // YCbCr 4:2:2:4 with alpha
            // Treat as UYVY for now (alpha not fully supported)
            ofxNDIutils::YUV422_to_RGBA(videoData, m_conversionBuffer, width, height, stride);
            pixelData = m_conversionBuffer;
            GLformat = GL_RGBA;
            break;
        
        // YUV 4:2:0 planar formats
        case NDIlib_FourCC_type_NV12: // YUV 4:2:0
            ofxNDIutils::NV12_to_RGBA(videoData, m_conversionBuffer, width, height, stride);
            pixelData = m_conversionBuffer;
            GLformat = GL_RGBA;
            break;
            
        case NDIlib_FourCC_type_I420: // YUV 4:2:0 planar
            ofxNDIutils::I420_to_RGBA(videoData, m_conversionBuffer, width, height, false);
            pixelData = m_conversionBuffer;
            GLformat = GL_RGBA;
            break;
            
        case NDIlib_FourCC_type_YV12: // YUV 4:2:0 planar (swapped UV)
            ofxNDIutils::I420_to_RGBA(videoData, m_conversionBuffer, width, height, true);
            pixelData = m_conversionBuffer;
            GLformat = GL_RGBA;
            break;
        
        // High bit-depth formats - 16-bit
        case NDIlib_FourCC_type_P216: // YCbCr 4:2:2 16-bit
            ofxNDIutils::P216_to_RGBA(videoData, m_conversionBuffer, width, height, stride);
            pixelData = m_conversionBuffer;
            GLformat = GL_RGBA;
            break;
            
        case NDIlib_FourCC_type_PA16: // YCbCr 4:2:2:4 16-bit with alpha
            ofxNDIutils::PA16_to_RGBA(videoData, m_conversionBuffer, width, height, stride);
            pixelData = m_conversionBuffer;
            GLformat = GL_RGBA;
            break;
        
        // RGB formats - direct use
        case NDIlib_FourCC_type_RGBA: // RGBA
        case NDIlib_FourCC_type_RGBX: // RGBX
            pixelData = videoData;
            GLformat = GL_RGBA;
            break;
            
        case NDIlib_FourCC_type_BGRA: // BGRA
        case NDIlib_FourCC_type_BGRX: // BGRX
        default:
            pixelData = videoData;
            GLformat = GL_BGRA;
            break;
    }

    // Check for QR commands before uploading texture (two-phase scheme).
    bool foundCodes = false;
    if (isQRCodeDetectionEnabled()) {
        foundCodes = FindCodes(pixelData, width, height, GLformat);
    }

    if (foundCodes) {
        NDIreceiver.FreeVideoData();
        return false;
    }

    // Load the texture with the converted or original pixel data
    bool success = LoadTexturePixels(TextureID, width, height, pixelData, GLformat);

    // Free the NDI video buffer
    NDIreceiver.FreeVideoData();

    if (success) {
        m_hasCapturedImage = true;
        m_isReady = true;

        // Update the active sublayer with the new frame
        if (isQRCodeDetectionEnabled() && m_qrOpHandler && m_qrOpHandler->isActive()) {
            m_qrOpHandler->updateActiveSubLayer(this, renderData.texId, renderData.width, renderData.height);
        }

        // Update divide texture sublayers if division mode is active
        if (m_textureDivisionMode == 2 && m_divideTexHandler && m_divideTexHandler->isActive()) {
            m_divideTexHandler->updateSubLayers(this, renderData.texId, renderData.width, renderData.height);
        }
    }

    return success;
}

// Streaming texture pixel load
// Approximately 20% faster than using glTexSubImage2D alone
// GLformat can be default GL_BGRA or GL_RGBA
bool NdiLayer::LoadTexturePixels(GLuint TextureID, unsigned int width, unsigned int height, unsigned char *data, int GLformat) {
    void *pboMemory = NULL;

    PboIndex = (PboIndex + 1) % 3;
    NextPboIndex = (PboIndex + 1) % 3;

    // Bind the texture and PBO
    glBindTexture(GL_TEXTURE_2D, TextureID);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[PboIndex]);

    // Copy pixels from PBO to the texture - use offset instead of pointer.
    // glTexSubImage2D redefines a contiguous subregion of an existing
    // two-dimensional texture image. NULL data pointer reserves space.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GLformat, GL_UNSIGNED_BYTE, 0);

    // Bind PBO to update the texture
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[NextPboIndex]);

    // Call glBufferData() with a NULL pointer to clear the PBO data and avoid a stall.
    glBufferData(GL_PIXEL_UNPACK_BUFFER, width * height * 4, 0, GL_STREAM_DRAW);

    // Map the buffer object into client's memory
    pboMemory = (void *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    // Update the mapped buffer directly
    if (pboMemory) {
        // RGBA pixel data
        // Use sse2 if the width is divisible by 16
        ofxNDIutils::CopyImage(data, (unsigned char *)pboMemory, width, height, true);
        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER); // release the mapped buffer
    } else {
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        return false;
    }

    // Release PBOs
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return true;
}

void NdiLayer::GenerateTexture(unsigned int &id, int width, int height) {
    glGenTextures(1, &id);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glBindTexture(GL_TEXTURE_2D, id);

    // All formats will be converted to RGBA8 for the texture
    // The GetPixelData function handles the conversion
    NDIlib_FourCC_video_type_e videoType = NDIreceiver.GetVideoType();
    
    switch (videoType) {
        // Formats that get converted to RGBA
        case NDIlib_FourCC_type_UYVY:
        case NDIlib_FourCC_type_UYVA:
        case NDIlib_FourCC_type_NV12:
        case NDIlib_FourCC_type_I420:
        case NDIlib_FourCC_type_YV12:
        case NDIlib_FourCC_type_P216:
        case NDIlib_FourCC_type_PA16:
        case NDIlib_FourCC_type_RGBX:
        case NDIlib_FourCC_type_RGBA:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            break;
            
        // BGRA formats (native from NDI)
        case NDIlib_FourCC_type_BGRX:
        case NDIlib_FourCC_type_BGRA:
        default:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
            break;
    }

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
