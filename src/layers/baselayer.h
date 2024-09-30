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
        int gridMode = 0;
        int stereoMode = 0;
        glm::vec3 rotate = glm::vec3(0);
        glm::vec3 translate = glm::vec3(0);
        bool roiEnabled = false;
        glm::vec4 roi = glm::vec4(0.f, 0.f, 1.f, 1.f);
    };

    struct PlaneParams {
        double azimuth = 0.0;
        double elevation = 0.0;
        double distance = 0.0;
        double roll = 0.0;
        glm::vec2 specifiedSize = glm::vec2(0);
        glm::vec2 actualSize = glm::vec2(0);
        int aspectRatioConsideration = 1;
        std::unique_ptr<sgct::utils::Plane> mesh = nullptr;
    };

    BaseLayer();
    ~BaseLayer();

    virtual void update();
    virtual bool ready();

    virtual void start();
    virtual void stop();

    uint32_t identifier() const;
    void setIdentifier(uint32_t id);
    void updateIdentifierBasedOnCount();

    bool needSync() const;
    void setHasSynced();

    virtual void encode(std::vector<std::byte> &data);
    virtual void decode(const std::vector<std::byte> &data, unsigned int &pos);

    LayerType type() const;
    void setType(LayerType t);

    std::string typeName() const;

    std::string title() const;
    void setTitle(std::string t);

    std::string filepath() const;
    void setFilePath(std::string p);

    int keepVisibilityForNumSlides();
    void setKeepVisibilityForNumSlides(int k);

    unsigned int textureId() const;
    int width() const;
    int height() const;

    float alpha() const;
    void setAlpha(float a);

    int gridMode() const;
    void setGridMode(int g);

    int stereoMode() const;
    void setStereoMode(int s);

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

    int planeAspectRatio() const;
    void setPlaneAspectRatio(int parc);

    void setPlaneSize(glm::vec2 pS, int parc);

    void drawPlane();
    void updatePlane();

protected:
    std::string m_title;
    std::string m_filepath;
    LayerType m_type;
    int m_keepVisibilityForNumSlides;
    RenderParams renderData;
    PlaneParams planeData;

    opengl_func_adress_ptr m_openglProcAdr;
    uint32_t m_identifier;
    static std::atomic_uint32_t m_id_gen;

    bool m_needSync;
};

#endif // BASELAYER_H
