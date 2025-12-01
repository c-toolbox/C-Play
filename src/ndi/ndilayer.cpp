/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ndilayer.h"
#include "audiosettings.h"
#include <sgct/sgct.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <climits>
#include <cmath>

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
        glDeleteBuffers(2, m_pbo);
    }

    if (renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId);
    }
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
    if (m_typePropertiesDecoded) {
        m_typePropertiesDecoded = false;
        setVolume(m_volume_Dec);
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
            NDIreceiver.RefreshSenders();
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

bool NdiLayer::ready() const {
    return m_isReady;
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

void NdiLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_volume_Dec);
}

void NdiLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_volume_Dec);
    m_typePropertiesDecoded = true;
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
            receviedImage = NDIreceiver.ReceiveImageOnlyFrameSync(width, height);
        }
        else {
            receviedImage = NDIreceiver.ReceiveImageOnly(width, height);
            if (isAudioEnabled()) {
                receviedAudio = NDIreceiver.ReceiveAudioOnly();
            }
        }
    }
    else if (isMaster()) { // Still want to run som passes here to start capturing of audio.
        // If updateRendering is Off, and audio stream already open we do not need to continue here
        if (!updateRendering && m_audioStreamOpen)
            return false;

        if (updateRendering) {
            //Receving both image and audio here
            receviedImage = NDIreceiver.ReceiveImageAndAudio(width, height);
            if (!receviedImage) {
                receviedAudio = NDIreceiver.IsAudioFrame();
            }
        }
        else if (isMaster()) {
            receviedAudio = NDIreceiver.ReceiveAudioOnly();
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
        return GetPixelData(renderData.texId, width, height);
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
            glGenBuffers(2, m_pbo);
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

// Get NDI pixel data from the video frame to ofTexture
bool NdiLayer::GetPixelData(GLuint TextureID, unsigned int width, unsigned int height) {
    // Get the video frame buffer pointer
    unsigned char *videoData = NDIreceiver.GetVideoData();
    if (!videoData) {
        // Ensure the video buffer is freed
        NDIreceiver.FreeVideoData();
        return false;
    }

    // Get the NDI video frame pixel data into the texture
    switch (NDIreceiver.GetVideoType()) {
        // Note : the receiver is set up to prefer BGRA format by default
        // If set to prefer NDIlib_recv_color_format_fastest, YUV data is received.
        // YCbCr - Load texture with YUV data by way of PBO
    case NDIlib_FourCC_type_UYVY: // YCbCr using 4:2:2
        printf("GetPixelData - UYVY format not supported\n");
        break;
    case NDIlib_FourCC_type_UYVA: // YCbCr using 4:2:2:4
        printf("GetPixelData - UYVA format not supported\n");
        break;
    case NDIlib_FourCC_type_P216: // YCbCr using 4:2:2 in 16bpp
        printf("GetPixelData - P216 format not supported\n");
        break;
    case NDIlib_FourCC_type_PA16: // YCbCr using 4:2:2:4 in 16bpp
        printf("GetPixelData - PA16 format not supported\n");
        break;
    case NDIlib_FourCC_type_RGBX: // RGBX
    case NDIlib_FourCC_type_RGBA: // RGBA
        LoadTexturePixels(TextureID, width, height, videoData, GL_RGBA);
        break;
    case NDIlib_FourCC_type_BGRX: // BGRX
    case NDIlib_FourCC_type_BGRA: // BGRA
    default:                      // BGRA
        LoadTexturePixels(TextureID, width, height, videoData, GL_BGRA);
        break;
    } // end switch received format

    // Free the NDI video buffer
    NDIreceiver.FreeVideoData();

    m_hasCapturedImage = true;

    m_isReady = true;

    return true;
}

// Streaming texture pixel load
// From : http://www.songho.ca/opengl/gl_pbo.html
// Approximately 20% faster than using glTexSubImage2D alone
// GLformat can be default GL_BGRA or GL_RGBA
bool NdiLayer::LoadTexturePixels(GLuint TextureID, unsigned int width, unsigned int height, unsigned char *data, int GLformat) {
    void *pboMemory = NULL;

    PboIndex = (PboIndex + 1) % 2;
    NextPboIndex = (PboIndex + 1) % 2;

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

    // Get the NDI video frame pixel data into the texture
    switch (NDIreceiver.GetVideoType()) {
        // Note : the receiver is set up to prefer BGRA format by default
        // If set to prefer NDIlib_recv_color_format_fastest, YUV data is received.
        // YCbCr - Load texture with YUV data by way of PBO
    case NDIlib_FourCC_type_UYVY: // YCbCr using 4:2:2
        printf("GetPixelData - UYVY format not supported\n");
        break;
    case NDIlib_FourCC_type_UYVA: // YCbCr using 4:2:2:4
        printf("GetPixelData - UYVA format not supported\n");
        break;
    case NDIlib_FourCC_type_P216: // YCbCr using 4:2:2 in 16bpp
        printf("GetPixelData - P216 format not supported\n");
        break;
    case NDIlib_FourCC_type_PA16: // YCbCr using 4:2:2:4 in 16bpp
        printf("GetPixelData - PA16 format not supported\n");
        break;
    case NDIlib_FourCC_type_RGBX: // RGBX
    case NDIlib_FourCC_type_RGBA: // RGBA
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        break;
    case NDIlib_FourCC_type_BGRX: // BGRX
    case NDIlib_FourCC_type_BGRA: // BGRA
    default:                      // BGRA
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        break;
    } // end switch received format

    // Disable mipmaps
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
