#ifndef NDILAYER_H
#define NDILAYER_H

#include <layers/baselayer.h>
#include <sgct/opengl.h>
#include <ndi/ofxNDI/ofxNDIreceive.h>

class ofxNDIreceive;

class NdiLayer : public BaseLayer {
public:
    NdiLayer();
    ~NdiLayer();

    void initialize();
    void update(bool updateRendering = true);
    bool ready();

    void start();
    void stop();

private:
    bool ReceiveImage();

    bool OpenReceiver();

    bool GetPixelData(GLuint TextureID, unsigned int width, unsigned int height);
    bool LoadTexturePixels(GLuint TextureID, unsigned int width, unsigned int height, unsigned char *data, int GLformat);
    void GenerateTexture(unsigned int &id, int width, int height);

    ofxNDIreceive NDIreceiver;
    GLuint m_pbo[2] = {0, 0}; // PBOs used for asynchronous pixel load
    int PboIndex = 0;         // Index used for asynchronous pixel load
    int NextPboIndex = 0;
    bool m_isReady = false;
};

#endif // NDILAYER_H