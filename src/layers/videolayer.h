#ifndef VIDEOLAYER_H
#define VIDEOLAYER_H

#include <layers/mpvlayer.h>

class VideoLayer : public MpvLayer {
public:
    VideoLayer(opengl_func_adress_ptr opa,
             bool allowDirectRendering = false,
             bool loggingOn = false,
             std::string logLevel = "info");
    ~VideoLayer();

    void initializeGL();
    void cleanup();
    void updateFrame();
    bool ready();

    void updateFbo();

private:
    void checkNeededMpvFboResize();
    void createMpvFBO(int width, int height);
    void generateTexture(unsigned int &id, int width, int height);
};

#endif // VIDEOLAYER_H