#ifndef BASELAYER_H
#define BASELAYER_H

#include <glm/glm.hpp>

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

protected:
    RenderParams renderData;
};

#endif // BASELAYER_H
