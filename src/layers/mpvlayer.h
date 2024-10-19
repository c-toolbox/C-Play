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
        bool loggingOn = false;
        bool mediaIsPaused = true;
        bool mediaShouldPause = true;
        std::string logLevel = "info";
        std::string loadedFile = "";
        bool updateRendering = true;
        bool allowDirectRendering = false;
        bool isMaster = false;
        bool supportVideo = true;
        std::vector<Track> audioTracks;
        int audioId = -1;
        int fboWidth = 0;
        int fboHeight = 0;
        bool fboCreated = false;
        unsigned int fboId = 0;
        int reconfigs = 0;
        int reconfigsBeforeUpdate = 0;
        int advancedControl = 0;
        int eofMode = -1;
        double mediaDuration = 0.0;
        double timePos = 0;
        double timeToSet = 0;
        bool timeIsDirty = false;
        std::atomic_bool threadRunning = false;
        std::atomic_bool mpvInitialized = false;
        std::atomic_bool mpvInitializedGL = false;
        std::atomic_bool threadDone = false;
        std::atomic_bool terminate = false;
    };

    MpvLayer(opengl_func_adress_ptr opa,
             bool allowDirectRendering = false,
             bool loggingOn = false,
             std::string logLevel = "info");

    virtual ~MpvLayer() = 0; //This is an abstract class

    void initialize();
    void initializeMpv();
    virtual void initializeGL();
    virtual void cleanup();
    virtual void updateFrame();
    virtual bool ready();

    void initializeAndLoad(std::string filePath);
    void update(bool updateRendering = true);

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
    void setVolume(int v, bool storeLevel = true);

    void encodeTypeAlways(std::vector<std::byte>& data);
    void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    void loadFile(std::string filePath, bool reload = false);
    std::string loadedFile();

    bool renderingIsOn();

    void setEOFMode(int eofMode);
    void setTimePause(bool paused, bool updateTime = true);
    void setTimePosition(double timePos, bool updateTime = true);
    void setLoopTime(double A, double B, bool enabled);
    void setValue(std::string param, int val);

protected:
    mpvData m_data;
};

#endif // MPVLAYER_H