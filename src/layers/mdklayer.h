#ifndef MDKLAYER_H
#define MDKLAYER_H

#include <layers/baselayer.h>
#include <mdk/RenderAPI.h>
#include <functional>

namespace mdk {
    class Player;
}
#ifndef Q_MDK_API
#define Q_MDK_API Q_DECL_IMPORT
#endif

class MdkLayer : public BaseLayer {
public:

    typedef std::function<void(std::string codecName)> onFileLoadedCallback;
    struct mdkData {
        bool loggingOn = false;
        bool mdkInitializedGL = false;
        bool mediaIsPaused = true;
        bool mediaShouldPause = true;
        bool updateRendering = true;
        int eofMode = -1;
        std::string logLevel = "info";
        std::string loadedFile = "";
        int fboWidth = 0;
        int fboHeight = 0;
        bool fboCreated = false;
        unsigned int fboId = 0;
        double timePos = 0;
        double timeToSet = 0;
        bool timeIsDirty = false;
        onFileLoadedCallback fileLoadedCallback = nullptr;
    };

    MdkLayer(gl_adress_func_v2 opa,
             bool loggingOn = false,
             std::string logLevel = "info",
             onFileLoadedCallback flc = nullptr);

    ~MdkLayer();

    void initialize();
    virtual void initializeGL();
    virtual void cleanup();
    virtual void updateFrame();
    virtual bool ready() const;

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

    bool renderingIsOn() const;

    void setEOFMode(int eofMode);
    void setTimePause(bool paused, bool updateTime = true);
    void setTimePosition(double timePos, bool updateTime = true);
    void setLoopTime(double A, double B, bool enabled);
    void setValue(std::string param, int val);

    void updateFbo();

private:
    void checkNeededMdkFboResize();
    void createMdkFBO(int width, int height);
    void generateTexture(unsigned int& id, int width, int height);

    mdkData m_data;
    mdk::GLRenderAPI m_renderAPI;
    std::unique_ptr<mdk::Player> m_player;
    gl_adress_func_v2 m_openglProcAdr;
};

#endif // MDKLAYER_H