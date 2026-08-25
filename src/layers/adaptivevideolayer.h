#ifndef ADAPTIVEVIDEOLAYER_H
#define ADAPTIVEVIDEOLAYER_H

#include <memory>
#include <layers/baselayer.h>

class VideoLayer;
class MdkLayer;

// Composite video layer that delegates to either an MPV-based (VideoLayer) or
// MDK-based (MdkLayer) sub-layer, chosen per file based on extension/codec.
// Used for slide layers on both master and nodes when MDK_SUPPORT is enabled.
// The main video layer always uses MPV directly (see main.cpp).
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

    ~AdaptiveVideoLayer() override;

    void initialize() override;
    void initializeGL() override;
    void cleanup() override;

    void initializeAndLoad(std::string filePath) override;

    void loadFile(std::string filePath, bool reload = false);

    // Lifecycle / rendering
    void update(bool updateRendering = true) override;
    void updateFrame() override;
    bool ready() const override;
    bool hasTexture() const override;
    bool renderingIsOn() const override;
    void reportSwap() override;

    // Playback control
    void start() override;
    void stop() override;
    bool pause() override;
    void setPause(bool paused) override;
    double position() override;
    void setPosition(double pos) override;
    double duration() override;
    double remaining() override;

    // Audio
    bool hasAudio() const override;
    int audioId() override;
    void setAudioId(int id) override;
    bool isAudioEnabled() const override;
    void enableAudio(bool enabled = true) override;
    std::vector<Track>* audioTracks() override;
    void updateAudioOutput() override;
    void setVolume(int v, bool storeLevel = true) override;
    void setVolumeMute(bool v) override;

    // Time / properties
    void setEOFMode(int eofMode) override;
    void setTimePause(bool paused, bool updateTime = true) override;
    void setTimePosition(double timePos, bool updateTime = true) override;
    void setLoopTime(double A, double B, bool enabled) override;
    void setValue(std::string param, int val) override;

    // EOF / loop-time accessors (forwarded to active sub-layer)
    int eofMode() const override;
    bool loopTimeEnabled() const override;
    double loopTimeA() const override;
    double loopTimeB() const override;

    // Texture info (forwarded to active sub-layer)
    unsigned int textureId() const override;
    int width() const override;
    int height() const override;

    // Serialization (time/pause sync - same format as MpvLayer/MdkLayer)
    void encodeTypeAlways(std::vector<std::byte>& data) override;
    void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) override;

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

    // The sub-layer currently in use (either mpvVideoLayer or mdkVideoLayer)
    BaseLayer* activeSubLayer() const;

    std::unique_ptr<VideoLayer> mpvVideoLayer;
    std::unique_ptr<MdkLayer> mdkVideoLayer;
};

#endif // ADAPTIVEVIDEOLAYER_H