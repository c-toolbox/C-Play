#ifndef MPVLAYER_H
#define MPVLAYER_H

#include <layers/baselayer.h>
#include <client.h>
#include <render_gl.h>
#include <mutex>

class MpvLayer : public BaseLayer
{
public:

    struct mpvData {
        mpv_handle* handle;
        mpv_render_context* renderContext;
        std::unique_ptr<std::thread> trd;
        double videoDuration = 0.0;
        int fboWidth = 0;
        int fboHeight = 0;
        unsigned int fboId = 0;
        int reconfigs = 0;
        int reconfigsBeforeUpdate = 0;
        int advancedControl = 0;
        int eofMode = -1;
        std::atomic_bool threadRunning = false;
        std::atomic_bool mpvInitialized = false;
        std::atomic_bool threadDone = false;
        std::atomic_bool terminate = false;
        bool updateRendering = true;
        bool allowDirectRendering = false;
        bool loggingOn = false;
        bool videoIsPaused = false;
        std::string logLevel = "info";
        std::string loadedFile = "";
        int timePos = 0;
    };

    MpvLayer(opengl_func_adress_ptr opa, 
        bool allowDirectRendering = false, 
        bool loggingOn = false, 
        std::string logLevel = "info");
    ~MpvLayer();

    void update();
    bool ready();

    void initialize();
    void cleanup();
    void updateFrame();

    void loadFile(std::string filePath, bool reload = false);
    std::string loadedFile();

    void updateFbo();
    void skipRendering(bool skipRendering);
    bool renderingIsOn();

    void setPause(bool paused);
    void setEOFMode(int eofMode);
    void setTimePosition(double timePos, bool updateTime = true);
    void setLoopTime(double A, double B, bool enabled);
    void setValue(std::string param, int val);

private:
    void checkNeededMpvFboResize();
    void createMpvFBO(int width, int height);
    void generateTexture(unsigned int& id, int width, int height);

    mpvData videoData;
};

#endif // MPVLAYER_H