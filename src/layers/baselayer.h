#ifndef BASELAYER_H
#define BASELAYER_H

#include <glm/glm.hpp>
#include <sgct/utils/plane.h>
#include <mutex>

class BaseLayer
{
public:
    struct RenderParams {
        unsigned int texId = 0;
        int width = -1;
        int height = -1;
        float alpha = 100.f;
        int gridMode = 0;
        int stereoMode = 0;
        glm::vec3 rotate = glm::vec3(0);
        glm::vec3 translate = glm::vec3(0);
    };

    struct PlaneParams {
        double elevation = 0.0;
        double distance = 0.0;
        glm::vec2 specifiedSize = glm::vec2(0);
        glm::vec2 actualSize = glm::vec2(0);
        int aspectRatioConsideration = 1;
        std::unique_ptr<sgct::utils::Plane> mesh = nullptr;
    };

    BaseLayer();
    ~BaseLayer();

    unsigned int textureId();
    int width();
    int height();

    float alpha();
    void setAlpha(float a);

    int gridMode();
    void setGridMode(int g);

    int stereoMode();
    void setStereoMode(int s);

    const glm::vec3& rotate();
    void setRotate(glm::vec3& r);

    const glm::vec3& translate();
    void setTranslate(glm::vec3& t);

    double planeElevation();
    void setPlaneElevation(double pE);

    double planeDistance();
    void setPlaneDistance(double pD);

    void setPlaneSize(glm::vec2 pS, int parc);

    void drawPlane();

protected:
    RenderParams renderData;
    PlaneParams planeData;

private:
    void updatePlane();
};

#endif // BASELAYER_H
