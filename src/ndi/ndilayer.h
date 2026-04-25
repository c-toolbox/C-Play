/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef NDILAYER_H
#define NDILAYER_H

#include <layers/baselayer.h>
#include <sgct/opengl.h>
#include <ndi/ofxNDI/ofxNDIreceive.h>
#include <portaudio.h>

class ofxNDIreceive;
class QRCommandProcessor;
class QROperationHandler;
class QROperationConfig;
struct QRPlaneDefinition;

class NdiFinder {
public:
    NdiFinder();
    ~NdiFinder();
    static void destroy();
    static NdiFinder& instance();

    int findSenders();
    std::vector<std::string> getSendersList();
    bool senderExists(std::string senderName);

    std::string getNDIVersionString();

private:
    ofxNDIreceive m_NDIreceiver;
    static NdiFinder* _instance;
};

class NdiLayer : public BaseLayer {
public:
    NdiLayer();
    ~NdiLayer();

    void initialize();
    void update(bool updateRendering = true);
    void updateFrame();
    bool ready() const;
    bool hasTexture() const override;

    void start();
    void stop();

    bool hasAudio() const;
    bool isAudioEnabled() const;
    void enableAudio(bool enabled = true);
    void updateAudioOutput();
    void setVolume(int v, bool storeLevel = true);

    void encodeTypeProperties(std::vector<std::byte>& data);
    void decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos);

    bool isQRCodeDetectionEnabled() const override;
    void setQRCodeDetectionEnabled(bool enabled) override;

    // Load QR operation plane configuration from a JSON file.
    bool loadQROperationConfig(const std::string& filePath);

    // Access the QR operation configuration.
    const QROperationConfig* qrOperationConfig() const;

    // Get the name of the currently active (live) plane, or empty if none.
    std::string activePlaneName() const;

    bool hasSubLayers() const override;
    std::vector<std::shared_ptr<BaseLayer>>& getSubLayers() const override;

private:
    bool ReceiveData(bool updateRendering);
    bool OpenReceiver();
    bool StartAudioStream();
    PaDeviceIndex GetChosenApplicationAudioDevice();

    bool FindCodes(unsigned char* data, unsigned int width, unsigned int height, int GLformat);
    bool GetPixelData(GLuint TextureID, unsigned int width, unsigned int height);
    bool LoadTexturePixels(GLuint TextureID, unsigned int width, unsigned int height, unsigned char *data, int GLformat);
    void GenerateTexture(unsigned int &id, int width, int height);

    void onQRCommand(const struct QRCommand& command);

    ofxNDIreceive NDIreceiver;

    PaStreamParameters m_audioOutputParameters;
    PaStream* m_audioStream;
    PaError m_audioError;
    bool m_audioStreamOpen = false;
    bool m_audioStreamStarted = false;
    bool m_recevieAudio = false;
    bool m_recevieAudioThroughCallback = true;

    GLuint m_pbo[3] = {0, 0, 0}; // PBOs used for asynchronous pixel load
    int PboIndex = 0;         // Index used for asynchronous pixel load
    int NextPboIndex = 0;
    bool m_hasCapturedImage = false;
    bool m_isReady = false;
    bool m_isAudioEnabled = false;
    bool m_typePropertiesDecoded = false;
    int m_volume_Dec = 100;
    bool m_qrCodeDetectionEnabled_Dec = false;

    // Conversion buffer for YUV formats
    unsigned char* m_conversionBuffer = nullptr;
    size_t m_conversionBufferSize = 0;
    NDIlib_FourCC_video_type_e m_lastVideoFormat = NDIlib_FourCC_type_BGRA;

    // QR command processing
    QRCommandProcessor* m_qrProcessor = nullptr;

    // QR operation handler (sublayers, config, SetActive/Clear/Freeze)
    QROperationHandler* m_qrOpHandler = nullptr;
};

#endif // NDILAYER_H