#ifndef ADAPTIVEVIDEOLAYER_H
#define ADAPTIVEVIDEOLAYER_H

#include <layers/baselayer.h>

class VideoLayer;
class MdkLayer;

class AdaptiveVideoLayer : public BaseLayer {
public:
    enum MediaPlayerLibrary {
        MPV,
        MDK
    };

    enum AdaptiveMethod {
        USE_EXTENSION,
        USE_CODEC
    };

    enum CodecChecker {
        USE_CURRENT_LIB,
        USE_MPV,
        USE_MDK
    };

    AdaptiveVideoLayer(
        gl_adress_func_v1 opa1, 
        gl_adress_func_v2 opa2,
        bool allowDirectRendering = false,
        bool loggingOn = false,
        std::string logLevel = "info");

    ~AdaptiveVideoLayer();

    void initialize();
    void initializeGL();
    void cleanup();

    void initializeAndLoad(std::string filePath);

    void loadFile(std::string filePath, bool reload = false);

    BaseLayer* get();

    void updateUsedMediaLibrary(std::string codecName);

private:
    MediaPlayerLibrary m_mpl;
    MediaPlayerLibrary m_mpl_default;
    AdaptiveMethod m_am;
    CodecChecker m_cc;

    std::vector<std::string> extPrioMpv;
    std::vector<std::string> extPrioMdk;
    std::vector<std::string> codecPrioMpv;
    std::vector<std::string> codecPrioMdk;

    VideoLayer* mpvVideoLayer;
    MdkLayer* mdkVideoLayer;
};

#endif // ADAPTIVEVIDEOLAYER_H