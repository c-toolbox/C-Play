/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "streamsettings.h"
#include "ndilayer.h"
#include "audiosettings.h"
#include <sgct/sgct.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <climits>
#include <cmath>
#include <utils/texturesubdivision.h>
#include <layers/texturelayer.h>
#include <audio/portaudiolock.h>

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
    NdiLayer* layer = (NdiLayer*)userData;
    ofxNDIreceive* reciever = layer ? layer->receiver() : nullptr;
    if(reciever) {
        if (reciever->shouldReceiveAudio()) {
            void* receviedFrames = reciever->ReceiveAudioOnlyFrameSync((int)framesPerBuffer);
            if (receviedFrames) {
                int inChannels = reciever->GetAudioChannels();
                int outChannels = reciever->GetAudioTargetOutputChannels();
                // Standard up/down mix or spatial mapping (shared with the other PortAudio layers).
                layer->audioMixer().process(reinterpret_cast<const int16_t*>(receviedFrames),
                    inChannels, static_cast<std::size_t>(inChannels), 1,
                    reinterpret_cast<int16_t*>(outputBuffer),
                    outChannels,
                    static_cast<std::size_t>(framesPerBuffer));

                // Report the peak of this frame for the audio level meter in the LayerView.
                if (layer && layer->audioLevelsEnabled() && outChannels > 0) {
                    int maxAbs = 0;
                    const int16_t* outSamples = reinterpret_cast<const int16_t*>(outputBuffer);
                    const std::size_t totalSamples = static_cast<std::size_t>(framesPerBuffer) * static_cast<std::size_t>(outChannels);
                    for (std::size_t i = 0; i < totalSamples; ++i) {
                        int v = outSamples[i];
                        if (v < 0) v = -v; // widen to int so INT16_MIN does not overflow
                        if (v > maxAbs) maxAbs = v;
                    }
                    layer->reportAudioLevel(static_cast<float>(maxAbs) / 32768.f);
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

    // Applied when the render context first opens the receiver.
    m_gpuConversionEnabled = StreamSettings::ndiReceiveGpuConversion();

    enableTextureSubdivision();
}

NdiLayer::~NdiLayer() {
    cleanup();

}

void NdiLayer::cleanup() {
    std::lock_guard<std::mutex> lock(m_updateMutex);

    NDIreceiver.SetAudio(false);

    std::unique_lock<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
    if (m_audioStream) {
        if (m_audioStreamStarted) {
            m_audioError = cplay::audio::paStopStream(m_audioStream);
            if (m_audioError == paNoError || m_audioError == paStreamIsStopped)
                m_audioStreamStarted = false;
            else
                cplay::audio::paAbortStream(m_audioStream);
        }

        if (m_audioStreamOpen) {
            m_audioError = cplay::audio::paCloseStream(m_audioStream);
            if (m_audioError == paNoError)
                m_audioStreamOpen = false;
        }

        m_audioStream = nullptr;
        m_audioStreamStarted = false;
        m_audioStreamOpen = false;
    }

    if (m_portAudioInitialized) {
        Pa_Terminate();
        m_portAudioInitialized = false;
    }
    paLock.unlock();

    NDIreceiver.ReleaseReceiver();
    m_gpuConversion.cleanup();
    m_receiveFormatConfigured = false;

    if (m_pbo[0]) {
        glDeleteBuffers(3, m_pbo);
        m_pbo[0] = m_pbo[1] = m_pbo[2] = 0;
    }

    if (renderData.texId > 0) {
        glDeleteTextures(1, &renderData.texId);
        renderData.texId = 0;
    }

    // Free conversion buffer
    if (m_conversionBuffer) {
        free(m_conversionBuffer);
        m_conversionBuffer = nullptr;
    }
}

void NdiLayer::initialize() {
    m_hasInitialized = true;
    NDIreceiver.SetSenderName(filepath());

    // Only recevie audio on master
    if (isAudioEnabled()) {
        std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
        m_audioError = Pa_Initialize();
        if (m_audioError != paNoError) {
            NDIreceiver.SetAudio(false);
        }
        else {
            m_portAudioInitialized = true;
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
        setVolumeMute(m_volumeMute_Dec);
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
        // Keep the per-layer finder inside NDIreceiver alive and let OpenReceiver()
        // poll it continuously (1 ms timeout). Do not destroy/recreate it here as a
        // "refresh": that would reset its discovery state and make this machine
        // detect new senders much slower than one that keeps the finder running.
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
    std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
    if (!m_audioStreamStarted && isAudioEnabled() && m_audioStream && m_audioStreamOpen) {
        setVolume(m_volume);
        m_audioError = cplay::audio::paStartStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted = true;
        }     
    }
}

void NdiLayer::stop() {
    std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
    if (isAudioEnabled() && m_audioStream && m_audioStreamOpen && m_audioStreamStarted) {
        m_audioError = cplay::audio::paStopStream(m_audioStream);
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
    // Serialize with update()/ReceiveData() on the render thread: both paths may call
    // StartAudioStream(), and PortAudio's ASIO host API (single global driver) is not
    // thread-safe. Concurrent Pa_OpenStream calls crash inside the ASIO driver.
    std::lock_guard<std::mutex> lock(m_updateMutex);
    std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());

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
        if (audioMixingEnabled()) {
            channelCount = cplay::audio::configuredOutputChannels(Pa_GetDeviceInfo(newDeviceIdx)->maxOutputChannels);
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
        else if (cplay::audio::outputRoutingChanged(m_audioStream, newDeviceIdx)) {
            restartStream = true; // to/from the shared output (ASIO or WASAPI exclusive)
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
                        m_audioError = cplay::audio::paCloseStream(m_audioStream);
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
        // Keep the receiver muted while volume mute is active.
        NDIreceiver.SetAudioVolume(m_volumeMute ? 0.f : static_cast<float>(v) / 100.f);
    }

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

void NdiLayer::setVolumeMute(bool v) {
    if (m_volumeMute == v)
        return;

    m_volumeMute = v;
    m_volumeMute_Dec = v;

    if (isAudioEnabled()) {
        // Muting is done by zeroing the receiver's audio volume, which both the PortAudio
        // callback path and the Pa_WriteStream path pick up. Unmuting restores the level.
        NDIreceiver.SetAudioVolume(v ? 0.f : static_cast<float>(m_volume) / 100.f);
    }

    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

// Helper to convert grid index to cols/rows
// 0=1x1, 1=1x2, 2=2x1, 3=2x2, 4=2x3, 5=3x2, 6=3x3
void NdiLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    m_subdivision->encodeAlways(data);
}

void NdiLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    m_subdivision->decodeAlways(data, pos);
}

void NdiLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_volume_Dec);
    sgct::serializeObject(data, m_volumeMute_Dec);
    m_subdivision->encodeProperties(data);
}

void NdiLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_volume_Dec);
    sgct::deserializeObject(data, pos, m_volumeMute_Dec);
    m_subdivision->decodeProperties(data, pos);

    m_typePropertiesDecoded = true;
}

void NdiLayer::encodeNodeDefaultsForNodes(std::vector<std::byte>& data, bool full) {
    // Mirrors decodeTypeAlways()/decodeTypeProperties(): defaults for a layer receiving a
    // node stream, without layer operations.
    if (full) {
        const int volume = 100;
        const bool volumeMute = false;
        // decodeFull() reads the always section before the properties.
        TextureSubdivision::encodeDefaults(data, false);
        sgct::serializeObject(data, volume);
        sgct::serializeObject(data, volumeMute);
        TextureSubdivision::encodeDefaults(data, true);
        return;
    }
    TextureSubdivision::encodeDefaults(data, false);
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
    if (m_gpuConversionEnabled && updateRendering) {
        try {
            receviedImage = NDIreceiver.ReceiveImageOnlyFrameSync(width, height);
            // Bootstrap audio parameters before opening the PortAudio callback,
            // or service the blocking audio path independently of video capture.
            // Once the frame-synchronizer exists it owns the receiver's audio, so plain
            // recv_capture never returns audio frames anymore; pull from the frame-sync queue.
            if (isMaster() && isAudioEnabled() && (!m_audioStreamOpen || !m_recevieAudioThroughCallback)) {
                if (NDIreceiver.FrameSyncOn())
                    receviedAudio = NDIreceiver.ReceiveAudioOnlyFrameSync(0) != nullptr;
                else
                    receviedAudio = NDIreceiver.ReceiveAudioOnly() != nullptr;
            }
        }
        catch (const std::exception& e) {
            sgct::Log::Error(std::format("NdiLayer Error in GPU frame sync receive: {}", e.what()));
            if (receviedImage) NDIreceiver.FreeVideoData();
            return false;
        }
    }
    else if (updateRendering && (!isMaster() || (m_audioStreamOpen && m_recevieAudioThroughCallback))) {
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

    if (receviedAudio || (!isMaster() && isAudioEnabled())) {
        if (!m_audioStreamOpen) {
            StartAudioStream();
        }

        if (!m_recevieAudioThroughCallback && m_audioStreamOpen && m_audioStreamStarted) {
            const int16_t* interleaved = NDIreceiver.GetAudioInterleaved();
            // GetAudioSamples() is the frame count per channel, which is what Pa_WriteStream expects.
            const int framesPerChannel = NDIreceiver.GetAudioSamples();
            m_audioError = cplay::audio::paWriteStream(m_audioStream, interleaved, framesPerChannel);
            if (m_audioError != paNoError) {
                sgct::Log::Error("NdiLayer Error: audio stream error");
            }

            // Report the peak of this frame for the audio level meter in the LayerView.
            if (interleaved && audioLevelsEnabled()) {
                int maxAbs = 0;
                const int totalSamples = framesPerChannel * std::max(1, NDIreceiver.GetAudioChannels());
                for (int i = 0; i < totalSamples; ++i) {
                    int v = interleaved[i];
                    if (v < 0) v = -v; // widen to int so INT16_MIN does not overflow
                    if (v > maxAbs) maxAbs = v;
                }
                reportAudioLevel(static_cast<float>(maxAbs) / 32768.f);
            }
        }
    }

    if (receviedImage) {
        const auto frameFormat = NDIreceiver.GetVideoFrameFormat();
        if (frameFormat == NDIlib_frame_format_type_field_0 || frameFormat == NDIlib_frame_format_type_field_1)
            height *= 2; // GPU reconstructs individual fields at full display height.
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
            NDIreceiver.FreeVideoData();
            return false;
        }
    }

    return receviedAudio || (!isMaster() && isAudioEnabled());
}

// Create receiver if not initialized or a new sender has been selected
bool NdiLayer::OpenReceiver() {
    if (!m_receiveFormatConfigured) {
        m_gpuConversionEnabled = m_gpuConversionEnabled && m_gpuConversion.initialize();
        // Accept the decoder's native raw format, including alpha.
        NDIreceiver.SetFormat(m_gpuConversionEnabled ? NDIlib_recv_color_format_fastest
                                                     : NDIlib_recv_color_format_BGRX_BGRA);
        m_receiveFormatConfigured = true;
    }
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
    // Also guards against updateFrame() (which runs ReceiveData without m_updateMutex)
    // racing another thread into opening the same stream twice.
    std::lock_guard<std::recursive_mutex> paLock(cplay::audio::portAudioMutex());
    if (m_audioStreamOpen)
        return true;

    if (audioMixingEnabled()) {
        m_audioOutputParameters.channelCount = cplay::audio::configuredOutputChannels(Pa_GetDeviceInfo(m_audioOutputParameters.device)->maxOutputChannels);
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
        // The callback receives the layer as userData so it can report the audio level.
        m_audioError = cplay::audio::paOpenStream(&m_audioStream, NULL, &m_audioOutputParameters, NDIreceiver.GetAudioSampleRate(), NDIreceiver.GetAudioSamples(), paClipOff, paNDIAudioCallback, this);
    }
    else {
        m_audioError = cplay::audio::paOpenStream(&m_audioStream, NULL, &m_audioOutputParameters, NDIreceiver.GetAudioSampleRate(), NDIreceiver.GetAudioSamples(), paClipOff, NULL, NULL);
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

    // Get the current video format
    NDIlib_FourCC_video_type_e currentFormat = NDIreceiver.GetVideoType();
    unsigned int stride = NDIreceiver.GetVideoStride();
    
    const auto frameFormat = NDIreceiver.GetVideoFrameFormat();
    const auto field = frameFormat == NDIlib_frame_format_type_field_0 ? NdiGpuConversion::Field::Upper
        : frameFormat == NDIlib_frame_format_type_field_1 ? NdiGpuConversion::Field::Lower
        : NdiGpuConversion::Field::Progressive;
    const bool fielded = field != NdiGpuConversion::Field::Progressive;
    const unsigned int inputHeight = fielded ? height/2 : height;
    bool gpuUploaded = false;
    bool qrChecked = false;
    if (m_gpuConversionEnabled) {
        using Format = NdiGpuConversion::Format;
        Format format = Format::UYVY;
        bool supported = true;
        int qrFormat = GL_RED;
        int qrPixelStride = 1;
        unsigned char* qrPixels = videoData;
        switch (currentFormat) {
        case NDIlib_FourCC_type_UYVY: format = Format::UYVY; qrPixels++; qrPixelStride = 2; break;
        case NDIlib_FourCC_type_UYVA: format = Format::UYVA; qrPixels++; qrPixelStride = 2; break;
        case NDIlib_FourCC_type_NV12: format = Format::NV12; break;
        case NDIlib_FourCC_type_I420: format = Format::I420; break;
        case NDIlib_FourCC_type_YV12: format = Format::YV12; break;
        case NDIlib_FourCC_type_P216: format = Format::P216; qrPixels++; qrPixelStride = 2; break;
        case NDIlib_FourCC_type_PA16: format = Format::PA16; qrPixels++; qrPixelStride = 2; break;
        case NDIlib_FourCC_type_BGRA: format = Format::BGRA; qrFormat = GL_BGRA; qrPixelStride = 4; break;
        case NDIlib_FourCC_type_BGRX: format = Format::BGRX; qrFormat = GL_BGRA; qrPixelStride = 4; break;
        case NDIlib_FourCC_type_RGBA: format = Format::RGBA; qrFormat = GL_RGBA; qrPixelStride = 4; break;
        case NDIlib_FourCC_type_RGBX: format = Format::RGBX; qrFormat = GL_RGBA; qrPixelStride = 4; break;
        default: supported = false; break;
        }
        if (supported) {
            // Scan native luma before upload: QR control frames must never replace
            // the displayed texture or the texture used by queued QR operations.
            qrChecked = true;
            if (isQRCodeDetectionEnabled()) {
                std::vector<unsigned char> qrField;
                int qrStride = static_cast<int>(stride);
                if (fielded) {
                    // QR detection requires the full image aspect ratio. Expand
                    // native luminance only; no CPU YUV-to-RGB conversion/readback.
                    const int bytesPerPixel = qrFormat == GL_RED ? 1 : 4;
                    qrStride = static_cast<int>(width)*bytesPerPixel;
                    qrField.resize(static_cast<size_t>(qrStride)*height);
                    for (unsigned int y = 0; y < height; ++y)
                        for (unsigned int x = 0; x < width; ++x)
                            std::memcpy(qrField.data()+static_cast<size_t>(y)*qrStride+x*bytesPerPixel,
                                        qrPixels+static_cast<size_t>(y/2)*stride+x*qrPixelStride, bytesPerPixel);
                    qrPixels = qrField.data();
                    qrPixelStride = bytesPerPixel;
                }
                if (m_subdivision->isControlFrame(qrPixels, width, height, qrFormat, qrStride, qrPixelStride)) {
                    NDIreceiver.FreeVideoData();
                    return false;
                }
            }
            gpuUploaded = m_gpuConversion.upload(TextureID, static_cast<int>(width),
                                                 static_cast<int>(inputHeight), videoData, stride, format, field);
        }
    }

    if (fielded && !gpuUploaded) {
        // A field buffer is half-height. Never pass it to a full-frame CPU converter.
        NDIreceiver.FreeVideoData();
        return false;
    }

    // Calculate required buffer size for RGBA conversion
    size_t requiredBufferSize = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    
    // Check if we need to reallocate the conversion buffer
    // (size change or format change requiring conversion)
    bool needsConversion = (currentFormat != NDIlib_FourCC_type_BGRA && 
                           currentFormat != NDIlib_FourCC_type_BGRX &&
                           currentFormat != NDIlib_FourCC_type_RGBA &&
                           currentFormat != NDIlib_FourCC_type_RGBX);
    
    if (needsConversion && !gpuUploaded) {
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
    if (!gpuUploaded) switch (currentFormat) {
        // YUV 4:2:2 formats - 8-bit
        case NDIlib_FourCC_type_UYVY: // YCbCr 4:2:2
            ofxNDIutils::YUV422_to_RGBA(videoData, m_conversionBuffer, width, height, stride);
            pixelData = m_conversionBuffer;
            GLformat = GL_RGBA;
            break;
            
        case NDIlib_FourCC_type_UYVA: // YCbCr 4:2:2:4 with alpha
            ofxNDIutils::YUV422_to_RGBA(videoData, m_conversionBuffer, width, height, stride);
            // Preserve the separate alpha plane in the CPU resource-limit fallback too.
            for (unsigned int y = 0; y < height; ++y)
                for (unsigned int x = 0; x < width; ++x)
                    m_conversionBuffer[(static_cast<size_t>(y)*width+x)*4+3] =
                        videoData[static_cast<size_t>(stride)*height+static_cast<size_t>(y)*(stride/2)+x];
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
    if (isQRCodeDetectionEnabled() && !qrChecked) {
        foundCodes = m_subdivision->isControlFrame(pixelData, width, height, GLformat);
    }

    if (foundCodes) {
        NDIreceiver.FreeVideoData();
        return false;
    }

    // Load the texture with the converted or original pixel data
    bool success = gpuUploaded || LoadTexturePixels(TextureID, width, height, pixelData, GLformat,
                                                     needsConversion ? width * 4 : stride);

    // Free the NDI video buffer
    NDIreceiver.FreeVideoData();

    if (success) {
        m_hasCapturedImage = true;
        m_isReady = true;

        m_subdivision->updateSubLayers(renderData.texId, renderData.width, renderData.height);
    }

    return success;
}

// Fill an orphaned PBO before uploading. This remains valid across resolution
// changes and switches between GPU conversion and the QR/alpha RGB path.
bool NdiLayer::LoadTexturePixels(GLuint TextureID, unsigned int width, unsigned int height,
                                 unsigned char *data, int GLformat, unsigned int stride) {
    const size_t rowBytes = static_cast<size_t>(width) * 4;
    if (!data || stride < rowBytes) return false;
    GLint buffer, texture, alignment, rowLength, skipRows, skipPixels;
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
    PboIndex = (PboIndex + 1) % 3;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pbo[PboIndex]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, rowBytes * height, nullptr, GL_STREAM_DRAW);
    auto* pixels = static_cast<unsigned char*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
    bool success = false;
    if (pixels) {
        for (unsigned int y = 0; y < height; ++y)
            std::memcpy(pixels + static_cast<size_t>(height-1-y)*rowBytes,
                        data + static_cast<size_t>(y)*stride, rowBytes);
        success = glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER) == GL_TRUE;
        if (success) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glBindTexture(GL_TEXTURE_2D, TextureID);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GLformat, GL_UNSIGNED_BYTE, nullptr);
        }
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
    return success;
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
