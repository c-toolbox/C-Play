#ifndef BASELAYER_H
#define BASELAYER_H

#include <glm/glm.hpp>
#include <mutex>
#include <sgct/utils/plane.h>
#include <vector>
#include "track.h"

class BaseLayer {
public:
    enum LayerType {
        BASE,
        IMAGE,
        VIDEO,
        AUDIO,
#ifdef PDF_SUPPORT
        PDF,
#endif
#ifdef NDI_SUPPORT
        NDI,
#endif
        STREAM,
#ifdef SPOUT_SUPPORT
        SPOUT,
#endif
        INVALID
    };

    enum LayerHierarchy {
        BACK,
        FRONT
    };

    enum StereoMode {
        No_2D,
        SBS_3D,
        TB_3D,
        TBF_3D
    };

    enum GridMode {
        None,
        Plane,
        Dome,
        Sphere_EQR,
        Sphere_EAC
    };

    typedef void* (*gl_adress_func_v1)(void* ctx, const char* name);
    typedef void* (*gl_adress_func_v2)(const char* name, void* ctx);
    static std::string typeDescription(BaseLayer::LayerType e);
    static BaseLayer *createLayer(bool isMaster, int layerType, gl_adress_func_v1 opa1, gl_adress_func_v2 opa2, std::string strId = "", uint32_t numID = 0);

    struct RenderParams {
        unsigned int texId = 0;
        int width = 0;
        int height = 0;
        float alpha = 100.f;
        bool flipY = false;
        uint8_t gridMode = 0;
        uint8_t stereoMode = 0;
        bool roiEnabled = false;
        glm::vec3 rotate = glm::vec3(0);
        glm::vec3 translate = glm::vec3(0);
        glm::vec4 roi = glm::vec4(0.f, 0.f, 1.f, 1.f);
    };

    struct PlaneParams {
        double azimuth = 0.0;
        double elevation = 0.0;
        double roll = 0.0;
        double distance = 0.0;
        double horizontal = 0.0;
        double vertical = 0.0;
        uint8_t aspectRatioConsideration = 1;
        glm::vec2 specifiedSize = glm::vec2(0);
        glm::vec2 actualSize = glm::vec2(0);
        std::unique_ptr<sgct::utils::Plane> mesh = nullptr;
    };

    BaseLayer();

    // Start of all virtual methods for derived classes

    virtual ~BaseLayer();
    virtual void cleanup();

    virtual void initialize();
    virtual void initializeGL();
    virtual void initializeAndLoad(std::string filePath);

    virtual void update(bool updateRendering = true);
    virtual void updateFrame();
    virtual bool renderingIsOn() const;
    virtual bool ready() const;

    virtual void start();
    virtual void stop();

    virtual bool pause();
    virtual void setPause(bool paused);

    virtual double position();
    virtual void setPosition(double pos);

    virtual double duration();
    virtual double remaining();

    virtual bool hasAudio();
    virtual int audioId();
    virtual void setAudioId(int id);
    virtual std::vector<Track>* audioTracks();
    virtual void updateAudioOutput();
    virtual void setVolume(int v, bool storeLevel = true);

    virtual void setEOFMode(int eofMode);
    virtual void setTimePause(bool paused, bool updateTime = true);
    virtual void setTimePosition(double timePos, bool updateTime = true);
    virtual void setLoopTime(double A, double B, bool enabled);
    virtual void setValue(std::string param, int val);

    virtual void encodeTypeCore(std::vector<std::byte>& data);
    virtual void decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos);

    virtual void encodeTypeAlways(std::vector<std::byte>& data);
    virtual void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    virtual void encodeTypeProperties(std::vector<std::byte>& data);
    virtual void decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos);

    // End virtual methods to use in derived classes

    void encodeBaseCore(std::vector<std::byte>& data);
    void decodeBaseCore(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeBaseAlways(std::vector<std::byte>& data);
    void decodeBaseAlways(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeBaseProperties(std::vector<std::byte>& data);
    void decodeBaseProperties(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeFull(std::vector<std::byte>& data);
    void decodeFull(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeAlways(std::vector<std::byte>& data);
    void decodeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    bool hasInitialized();

    bool isMaster() const;
    bool existOnMasterOnly() const;
    uint32_t identifier() const;

    bool needSync() const;
    void setHasSynced();

    LayerType type() const;
    void setType(LayerType t);

    LayerHierarchy hierarchy() const;
    void setHierarchy(LayerHierarchy h);

    std::string typeName() const;

    std::string title() const;
    void setTitle(std::string t);

    std::string filepath() const;
    void setFilePath(std::string p);

    int page() const;
    void setPage(int p);

    int numPages() const;
    void setNumPages(int np);

    int keepVisibilityForNumSlides();
    void setKeepVisibilityForNumSlides(int k);

    int volume() const;
    unsigned int textureId() const;
    int width() const;
    int height() const;

    float alpha() const;
    void setAlpha(float a);

    bool shouldUpdate() const;
    void setShouldUpdate(bool value);

    bool shouldPreLoad() const;
    void setShouldPreLoad(bool value);

    bool flipY() const;

    uint8_t gridMode() const;
    void setGridMode(uint8_t g);

    uint8_t stereoMode() const;
    void setStereoMode(uint8_t s);

    const glm::vec3 &rotate() const;
    void setRotate(glm::vec3 &r);

    const glm::vec3 &translate() const;
    void setTranslate(glm::vec3 &t);

    bool roiEnabled() const;
    void setRoiEnabled(bool value);

    const glm::vec4 &roi() const;
    void setRoi(glm::vec4 &r);
    void setRoi(float x, float y, float width, float height);

    double planeAzimuth() const;
    void setPlaneAzimuth(double pA);

    double planeElevation() const;
    void setPlaneElevation(double pE);

    double planeRoll() const;
    void setPlaneRoll(double pR);

    double planeDistance() const;
    void setPlaneDistance(double pD);

    double planeHorizontal() const;
    void setPlaneHorizontal(double pH);

    double planeVertical() const;
    void setPlaneVertical(double pV);

    double planeWidth() const;
    void setPlaneWidth(double pW);

    double planeHeight() const;
    void setPlaneHeight(double pH);

    uint8_t planeAspectRatio() const;
    void setPlaneAspectRatio(uint8_t parc);

    void setPlaneSize(glm::vec2 pS, uint8_t parc);

    void drawPlane();
    void updatePlane();

    void setIsMaster(bool value);
    void setIdentifier(uint32_t id);
    void updateIdentifierBasedOnCount();

protected:
    void setNeedSync();

    LayerType m_type;
    LayerHierarchy m_hierachy;
    std::string m_title;
    std::string m_filepath;
    int m_page;
    int m_numPages;
    int m_volume;
    int m_keepVisibilityForNumSlides;
    bool m_isMaster;
    bool m_existOnMasterOnly;
    bool m_shouldUpdate;
    bool m_shouldPreLoad;
    bool m_hasInitialized;
    bool m_needSync;
    int m_syncIteration;

    RenderParams renderData;
    PlaneParams planeData;

    uint32_t m_identifier;
    static std::atomic_uint32_t m_id_gen;
};

#endif // BASELAYER_H
