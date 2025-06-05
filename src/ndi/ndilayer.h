/*
 * SPDX-FileCopyrightText:
 * 2025 Erik Sundén <eriksunden85@gmail.com>
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

class NdiFinder {
public:
    NdiFinder();
    ~NdiFinder();
    static NdiFinder& instance();

    int findSenders();
    std::vector<std::string> getSendersList();
    bool senderExists(std::string senderName);

    std::string getNDIVersionString();

private:
    ofxNDIreceive* m_NDIreceiver;
    static NdiFinder* _instance;
};

class NdiLayer : public BaseLayer {
public:
    NdiLayer();
    ~NdiLayer();

    void initialize();
    void update(bool updateRendering = true);
    bool ready() const;

    void start();
    void stop();

    bool hasAudio();
    void updateAudioOutput();
    void setVolume(int v, bool storeLevel = true);

private:
    bool ReceiveData(bool updateRendering);
    bool OpenReceiver();
    bool StartAudioStream();
    PaDeviceIndex GetChosenApplicationAudioDevice();

    bool GetPixelData(GLuint TextureID, unsigned int width, unsigned int height);
    bool LoadTexturePixels(GLuint TextureID, unsigned int width, unsigned int height, unsigned char *data, int GLformat);
    void GenerateTexture(unsigned int &id, int width, int height);

    ofxNDIreceive NDIreceiver;

    PaStreamParameters m_audioOutputParameters;
    PaStream* m_audioStream;
    PaError m_audioError;
    bool m_audioStreamOpen = false;
    bool m_audioStreamStarted = false;
    bool m_recevieAudio = false;
    bool m_recevieAudioThroughCallback = true;

    GLuint m_pbo[2] = {0, 0}; // PBOs used for asynchronous pixel load
    int PboIndex = 0;         // Index used for asynchronous pixel load
    int NextPboIndex = 0;
    bool m_hasCapturedImage = false;
    bool m_isReady = false;
};

#endif // NDILAYER_H