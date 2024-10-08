#ifndef BASELAYER_H
#define BASELAYER_H

#include <glm/glm.hpp>
#include <mutex>
#include <sgct/utils/plane.h>
#include <vector>

class BaseLayer {
public:
    enum LayerType {
        BASE,
        IMAGE,
#ifdef PDF_SUPPORT
        PDF,
#endif
        VIDEO,
#ifdef NDI_SUPPORT
        NDI,
#endif
        INVALID
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

    typedef void *(*opengl_func_adress_ptr)(void *ctx, const char *name);
    static std::string typeDescription(BaseLayer::LayerType e);
    static BaseLayer *createLayer(int layerType, opengl_func_adress_ptr opa, std::string strId = "", uint32_t numID = 0);

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
        double distance = 0.0;
        double roll = 0.0;
        uint8_t aspectRatioConsideration = 1;
        glm::vec2 specifiedSize = glm::vec2(0);
        glm::vec2 actualSize = glm::vec2(0);
        std::unique_ptr<sgct::utils::Plane> mesh = nullptr;
    };

    BaseLayer();
    ~BaseLayer();

    virtual void initialize();
    virtual void update();
    virtual bool ready();

    virtual void start();
    virtual void stop();

    bool hasInitialized();

    uint32_t identifier() const;
    void setIdentifier(uint32_t id);
    void updateIdentifierBasedOnCount();

    bool needSync() const;
    void setHasSynced();

    void encodeFull(std::vector<std::byte> &data);
    void encodeMinimal(std::vector<std::byte>& data);
    void encodeProperties(std::vector<std::byte>& data);

    void decodeFull(const std::vector<std::byte> &data, unsigned int &pos);
    void decodeMinimal(const std::vector<std::byte>& data, unsigned int& pos);
    void decodeProperties(const std::vector<std::byte>& data, unsigned int& pos);

    LayerType type() const;
    void setType(LayerType t);

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

    unsigned int textureId() const;
    int width() const;
    int height() const;

    float alpha() const;
    void setAlpha(float a);

    bool shouldUpdate() const;
    void setShouldUpdate(bool value);

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

    double planeDistance() const;
    void setPlaneDistance(double pD);

    double planeRoll() const;
    void setPlaneRoll(double pR);

    double planeWidth() const;
    void setPlaneWidth(double pW);

    double planeHeight() const;
    void setPlaneHeight(double pH);

    uint8_t planeAspectRatio() const;
    void setPlaneAspectRatio(uint8_t parc);

    void setPlaneSize(glm::vec2 pS, uint8_t parc);

    void drawPlane();
    void updatePlane();

protected:
    LayerType m_type;
    std::string m_title;
    std::string m_filepath;
    int m_page;
    int m_numPages;
    int m_keepVisibilityForNumSlides;
    bool m_shouldUpdate;
    bool m_hasInitialized;
    bool m_needSync;

    RenderParams renderData;
    PlaneParams planeData;

    opengl_func_adress_ptr m_openglProcAdr;
    uint32_t m_identifier;
    static std::atomic_uint32_t m_id_gen;
};

#endif // BASELAYER_H
