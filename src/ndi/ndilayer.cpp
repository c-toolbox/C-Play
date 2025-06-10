/*
 * SPDX-FileCopyrightText:
 * 2024-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "ndilayer.h"
#include "audiosettings.h"
#include <fmt/core.h>
#include <sgct/sgct.h>

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
                int channels = reciever->GetAudioChannels();
                memcpy(outputBuffer, receviedFrames, ((size_t)framesPerBuffer * (size_t)channels * sizeof(int16_t)));
            }
        }
    }

    return paContinue;
}

NdiFinder* NdiFinder::_instance = nullptr;

NdiFinder::NdiFinder() : m_NDIreceiver(new ofxNDIreceive()) {
    m_NDIreceiver->CreateFinder();
}

NdiFinder::~NdiFinder() {
    delete m_NDIreceiver;
    delete _instance;
    _instance = nullptr;
}

NdiFinder& NdiFinder::instance() {
    if (!_instance) {
        _instance = new NdiFinder();
    }
    return *_instance;
}

int NdiFinder::findSenders() {
    return m_NDIreceiver->FindSenders();
}

std::vector<std::string> NdiFinder::getSendersList() {
    return m_NDIreceiver->GetSenderList();
}

bool NdiFinder::senderExists(std::string senderName) {
    int senderIdx;
    return m_NDIreceiver->GetSenderIndex(senderName, senderIdx);
}

std::string NdiFinder::getNDIVersionString() {
    return m_NDIreceiver->GetNDIversion();
}

NdiLayer::NdiLayer() {
    setType(BaseLayer::LayerType::NDI);
    NDIreceiver.ResetFps(30.0);

    // Only recevie audio on master
    if (isMaster()) {
        NDIreceiver.SetAudio(true, true);
    }
    else {
        NDIreceiver.SetAudio(false);
    }

    // =======================================
    // Set to prefer BGRA
    NDIreceiver.SetFormat(NDIlib_recv_color_format_BGRX_BGRA);

    OpenReceiver();
}

NdiLayer::~NdiLayer() {
    if (isMaster() && m_recevieAudio) {
        NDIreceiver.SetAudio(false);
        if (m_audioStreamOpen) {
            m_audioError = Pa_CloseStream(m_audioStream);
            if (m_audioError == paNoError) {
                m_audioStreamOpen = false;
            }
        }
        Pa_Terminate();
    }

    if (!OpenReceiver()) {
        NDIreceiver.ReleaseReceiver();
    }

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
    if (isMaster()) {
        m_audioError = Pa_Initialize();
        if (m_audioError != paNoError) {
            NDIreceiver.SetAudio(false);
        }
        else {
            NDIreceiver.SetAudio(true, true);
            m_recevieAudio = true;
            m_audioOutputParameters.device = GetChosenApplicationAudioDevice();
            if (m_audioOutputParameters.device == paNoDevice) {
                sgct::Log::Error("NdiLayer Error: No default audio output device.\n");
            }
        }
    }
}

void NdiLayer::update(bool updateRendering) {
    // Check sender amount
    int senders = NdiFinder::instance().findSenders();
    if (senders < 1) {
        m_isReady = false;
        return;
    }

    // Check if our sender exists
    m_isReady = NdiFinder::instance().senderExists(filepath());
    if (!m_isReady) {
        NDIreceiver.RefreshSenders();
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
    if (isMaster() && m_audioStream && m_audioStreamOpen) {
        m_audioError = Pa_StartStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted = true;
        }     
    }
}

void NdiLayer::stop() {
    if (isMaster() && m_audioStream && m_audioStreamOpen && m_audioStreamStarted) {
        m_audioError = Pa_StopStream(m_audioStream);
        if (m_audioError == paNoError) {
            m_audioStreamStarted = false;
        }
    }
}

bool NdiLayer::hasAudio() {
    return isMaster();
}

void NdiLayer::updateAudioOutput() {
    if (isMaster()) {
        //See if device has changed
        PaDeviceIndex currentDeviceIdx = m_audioOutputParameters.device;
        PaDeviceIndex newDeviceIdx = GetChosenApplicationAudioDevice();

        //Change device index if we found a new one
        if (newDeviceIdx != currentDeviceIdx) {
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

    if (isMaster()) {
        NDIreceiver.SetAudioVolume(static_cast<float>(v) / 100.f);
    }
}

// Receive ofTexture
bool NdiLayer::ReceiveData(bool updateRendering) {
    // Receive a pixel image first
    unsigned int width = (unsigned int)renderData.width;
    unsigned int height = (unsigned int)renderData.height;

    // If we have opened an audio stream already, let's only capture image here
    // Audio is captured in separate callback
    // Or if we are a node, we do not need to capture audio anyway...
    if (updateRendering && (!isMaster() || (m_audioStreamOpen && m_recevieAudioThroughCallback))) {
        bool receviedImage = false;

        if (m_hasCapturedImage || NDIreceiver.FrameSyncOn()) { // We can start using frame sync now
            receviedImage = NDIreceiver.ReceiveImageOnlyFrameSync(width, height);
        }
        else {
            receviedImage = NDIreceiver.ReceiveImageOnly(width, height);
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
    }
    else if (isMaster()) { // Still want to run som passes here to start capturing of audio.
        // If updateRendering is Off, and audio stream already open we do not need to continue here
        if (!updateRendering && m_audioStreamOpen)
            return false;

        bool receviedImage = false;
        bool receviedAudio = false;

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
        else if (receviedAudio) {
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
    m_audioOutputParameters.channelCount = NDIreceiver.GetAudioChannels();
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
            && !AudioSettings::portAudioOutpuApi().isEmpty()) {
            int numDevices = Pa_GetDeviceCount();
            if (numDevices < 0) {
                return choseDeviceIdx;
            }
            const PaDeviceInfo* deviceInfo;
            const PaHostApiInfo* apiInfo;
            sgct::Log::Info(fmt::format("NdiLayer: Trying to find audio device named \"{}\" using the \"{}\" api.",
                AudioSettings::portAudioOutputDevice().toStdString(), AudioSettings::portAudioOutpuApi().toStdString()));
            bool foundDevice = false;
            for (int i = 0; i < numDevices; i++) {
                deviceInfo = Pa_GetDeviceInfo(i);
                apiInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
                if (deviceInfo->maxOutputChannels > 1) {
                    QString deviceName = QString::fromUtf8(deviceInfo->name);
                    QString apiName = QString::fromUtf8(apiInfo->name);
                    if (deviceName == AudioSettings::portAudioOutputDevice()
                        && apiName == AudioSettings::portAudioOutpuApi()) {
                        choseDeviceIdx = i;
                        foundDevice = true;
                        sgct::Log::Info(fmt::format("NdiLayer: Found desired audio device."));
                    }
                }
            }
            if (!foundDevice) {
                sgct::Log::Info(fmt::format("NdiLayer: Did not find desired audio device. Sticking with default device."));
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
