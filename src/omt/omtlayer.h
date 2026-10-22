/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sund�n <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef OMTLAYER_H
#define OMTLAYER_H

#include <layers/baselayer.h>
#include <sgct/opengl.h>
#include <libomt.h>
#include <portaudio.h>
#include <memory>
#include <mutex>
#include <vector>

class OmtFinder {
public:
    OmtFinder();
    ~OmtFinder();
    static OmtFinder& instance();
    static void destroy();
    // libomt loads libvmx dynamically for VMX1 video decoding. If it is missing, receivers
    // still connect and deliver audio but never return video frames. Logs an error when absent.
    static bool checkVideoDecoderAvailable();

    std::vector<std::string> getSendersList();
    bool senderExists(const std::string& senderName);

    std::string getOMTVersionString();

private:
    static OmtFinder* _instance;
};

class OmtLayer : public BaseLayer {
public:
    OmtLayer();
    ~OmtLayer();

    void initialize();
    void update(bool updateRendering = true);
    void updateFrame();
    bool ready() const;
    bool hasTexture() const override;

    void start();
    void stop();

    bool hasAudio() const;
    // The audio level is reported from ProcessAudioFrame(), so a meter in the LayerView
    // can show live levels while the image renders.
    bool hasAudioLevels() const override { return true; }
    bool isAudioEnabled() const;
    void enableAudio(bool enabled = true);
    void updateAudioOutput();
    void setVolume(int v, bool storeLevel = true);
    void setVolumeMute(bool v);

    void encodeTypeAlways(std::vector<std::byte>& data);
    void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeTypeProperties(std::vector<std::byte>& data);
    void decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos);

private:
    void GenerateTexture(unsigned int& id, int width, int height);
    bool StartAudioStream();
    PaDeviceIndex GetChosenApplicationAudioDevice();
    void ProcessAudioFrame(OMTMediaFrame* frame);

    omt_receive_t* m_receiver = nullptr;
    bool m_isReady = false;

    // Audio members
    PaStreamParameters m_audioOutputParameters;
    PaStream* m_audioStream = nullptr;
    PaError m_audioError = paNoError;
    bool m_audioStreamOpen = false;
    bool m_audioStreamStarted = false;
    bool m_receiveAudio = false;
    bool m_portAudioInitialized = false;
    bool m_isAudioEnabled = false;
    bool m_typePropertiesDecoded = false;
    int m_volume_Dec = 100;
    bool m_volumeMute = false;
    bool m_volumeMute_Dec = false;
    float m_audioVolume = 1.0f;
    int m_audioSampleRate = 48000;
    int m_audioChannels = 2;
    int m_audioOutputChannels = 2;

    // Temporary buffer for interleaving planar float audio
    std::vector<float> m_interleavedAudioBuf;

    // Guards all PortAudio stream state; update() (render thread) and
    // updateAudioOutput() (GUI thread) may otherwise open/close streams concurrently,
    // which crashes non-thread-safe drivers such as ASIO.
    std::recursive_mutex m_audioMutex;
};

#endif // OMTLAYER_H
