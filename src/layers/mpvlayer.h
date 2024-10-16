#ifndef MPVLAYER_H
#define MPVLAYER_H

#include <client.h>
#include <layers/baselayer.h>
#include <mutex>
#include <render_gl.h>

class MpvLayer : public BaseLayer {
public:
    struct mpvData {
        mpv_handle *handle;
        mpv_render_context *renderContext;
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
        std::atomic_bool mpvInitializedGL = false;
        std::atomic_bool threadDone = false;
        std::atomic_bool terminate = false;
        bool updateRendering = true;
        bool allowDirectRendering = false;
        bool isMaster = false;
        std::vector<Track> audioTracks;
        int audioId = -1;
        bool loggingOn = false;
        bool videoIsPaused = true;
        bool videoShouldPause = true;
        std::string logLevel = "info";
        std::string loadedFile = "";
        double timePos = 0;
        double timeToSet = 0;
        bool timeIsDirty = false;
    };

    MpvLayer(opengl_func_adress_ptr opa,
             bool allowDirectRendering = false,
             bool loggingOn = false,
             std::string logLevel = "info");
    ~MpvLayer();

    void initialize();
    void update(bool updateRendering = true);
    bool ready();

    void start();
    void stop();

    bool pause();
    void setPause(bool pause);

    double position();
    void setPosition(double pos);

    double duration();
    double remaining();

    bool hasAudio();
    int audioId();
    void setAudioId(int id);
    std::vector<Track>* audioTracks();
    void updateAudioOutput();
    void setVolume(int v);

    void encodeTypeAlways(std::vector<std::byte>& data);
    void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    void initializeMpv();
    void initializeGL();
    void cleanup();
    void updateFrame();

    void loadFile(std::string filePath, bool reload = false);
    std::string loadedFile();

    void updateFbo();
    void skipRendering(bool skipRendering);
    bool renderingIsOn();

    void setEOFMode(int eofMode);
    void setTimePause(bool paused, bool updateTime = true);
    void setTimePosition(double timePos, bool updateTime = true);
    void setLoopTime(double A, double B, bool enabled);
    void setValue(std::string param, int val);

private:
    void checkNeededMpvFboResize();
    void createMpvFBO(int width, int height);
    void generateTexture(unsigned int &id, int width, int height);

    mpvData videoData;
};

#endif // MPVLAYER_H