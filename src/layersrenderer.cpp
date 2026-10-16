/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layersrenderer.h"
#include <animation/maskglsl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <sgct/opengl.h>

constexpr std::string_view VideoVert = R"(
  #version 460 core

  layout (location = 0) in vec2 in_position;
  layout (location = 1) in vec2 in_texCoord;

  uniform int eye;
  uniform int stereoscopicMode;
  uniform vec4 roi;
  uniform bool flipY;
  uniform mat4 quadMatrix;

  out vec2 tr_uv;
  out vec2 mask_uv;

  void main() {
    gl_Position = quadMatrix * vec4(in_position, 0.0, 1.0);
    mask_uv = in_texCoord;
    tr_uv = flipY ? vec2(in_texCoord.x, 1.0-in_texCoord.y) : in_texCoord;
    tr_uv = (tr_uv * roi.zw) + roi.xy;

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (tr_uv * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = tr_uv * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = tr_uv * vec2(1.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else { //Left Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = tr_uv * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (tr_uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (tr_uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
  }
)";

constexpr std::string_view MeshVert = R"(
  #version 460 core

  layout (location = 0) in vec2 in_texCoord;
  layout (location = 1) in vec3 in_normal;
  layout (location = 2) in vec3 in_position;

  uniform mat4 mvp;
  uniform int eye;
  uniform int stereoscopicMode;
  uniform vec4 roi;
  uniform bool flipY;

  out vec2 tr_uv;
  out vec3 tr_normals;
  out vec2 mask_uv;

  uniform mat4 circularModel;
uniform float circularHalfWidth;
uniform vec2 circularSourceBounds;

void main() {
    float circularX = (circularModel * vec4(in_position, 1.0)).x;
    gl_ClipDistance[0] = circularX + circularHalfWidth;
    gl_ClipDistance[1] = circularHalfWidth - circularX;
    gl_ClipDistance[2] = circularX - circularSourceBounds.x;
    gl_ClipDistance[3] = circularSourceBounds.y - circularX;
    gl_Position = mvp * vec4(in_position, 1.0);
    mask_uv = in_texCoord;
    tr_uv = flipY ? vec2(in_texCoord.x, 1.0-in_texCoord.y) : in_texCoord;
    tr_uv = (tr_uv * roi.zw) + roi.xy;
    tr_normals = in_normal;

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (tr_uv * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = tr_uv * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = tr_uv * vec2(1.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else { // Left Eye or Mono
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = tr_uv * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (tr_uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (tr_uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
  }
)";

// Fragment shaders are split around the cplayPost() definition (see cplay::anim::postGlsl()).
constexpr std::string_view VideoFragHead = R"(
  #version 460 core

  uniform sampler2D tex;
  uniform float alpha;
  uniform bool outside;

  in vec2 tr_uv;
  in vec3 tr_normals;
  in vec2 mask_uv;
  out vec4 out_color;
)" CPLAY_MASK_GLSL;

constexpr std::string_view VideoFragMain = R"(
  void main() {
    vec2 texCoods = tr_uv;
    if(outside){
        texCoods = vec2(1.0-tr_uv.x, tr_uv.y);
    }
   
    out_color = cplayPost(texCoods, mask_uv) * vec4(1.0, 1.0, 1.0, alpha * cplayMask(mask_uv));
  }
)";

constexpr std::string_view EACMeshVert = R"(
  #version 460 core

  layout (location = 0) in vec2 in_texCoord;
  layout (location = 1) in vec3 in_normal;
  layout (location = 2) in vec3 in_position;

  uniform mat4 mvp;
  
  uniform float scaleToUnitCube;
  uniform bool outside;

  out vec3 tr_position;
  out vec3 tr_normal;

  void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    tr_position = in_position * scaleToUnitCube;

    if(outside)
        tr_normal = -in_normal;
    else
        tr_normal = in_normal;
  }
)";

constexpr std::string_view EACVideoFragHead = R"(
  #version 460 core

  uniform sampler2D tex;
  uniform int eye;
  uniform int stereoscopicMode;
  uniform float alpha;
  uniform int videoWidth;
  uniform int videoHeight;
  uniform bool flipY;

  in vec3 tr_position;
  in vec3 tr_normal;
  out vec4 out_color;
)" CPLAY_MASK_GLSL;

constexpr std::string_view EACVideoFragMain = R"(
  const float M_PI_2 = 1.57079632679489661923;   // pi/2
  const float M_PI_4 = 0.785398163397448309616;  // pi/4
  const float M_1_PI = 0.318309886183790671538;  // 1/pi
  const float M_2_PI = 0.636619772367581343076;  // 2/pi
  const float M_PI = 3.14159265358979323846264338327950288;
  
  const int TOP_LEFT = 0;
  const int TOP_MIDDLE = 1;
  const int TOP_RIGHT = 2;
  const int BOTTOM_LEFT = 3;
  const int BOTTOM_MIDDLE = 4;
  const int BOTTOM_RIGHT = 5;

  const int RIGHT = 0; ///< Axis +X
  const int LEFT = 1; ///< Axis -X
  const int UP = 2; ///< Axis +Y
  const int DOWN = 3; ///< Axis -Y
  const int FRONT = 4; ///< Axis -Z
  const int BACK = 5; ///< Axis +Z

  const int ROT_0 = 0;
  const int ROT_90 = 1;
  const int ROT_180 = 2;
  const int ROT_270 = 3;

  vec2 rotate_cube_face(vec2 uv_in, int rotation)
  {
      vec2 uv_out;

      switch (rotation) {
          case ROT_0:
              uv_out = uv_in;
              break;
          case ROT_90:
              uv_out.x = -uv_in.y;
              uv_out.y =  uv_in.x;
              break;
          case ROT_180:
              uv_out.x = -uv_in.x;
              uv_out.y = -uv_in.y;
              break;
          case ROT_270:
              uv_out.x = uv_in.y;
              uv_out.y = -uv_in.x;
              break;
      }

      return uv_out;
  }

  /**
  * Calculate direction from corresponding 3D coordinates on sphere.
  * @param vec3 coordinated on sphere
  * @return direction direction of view
  */
  int xyz_to_direction(vec3 xyz)
  {
    int direction;
    float phi = atan(xyz.x, xyz.z);
    float theta = asin(xyz.y);
    float phi_norm, theta_threshold;
    int face;

    if (phi >= -M_PI_4 && phi < M_PI_4) {
        direction = FRONT;
        phi_norm = phi;
    } else if (phi >= -(M_PI_2 + M_PI_4) && phi < -M_PI_4) {
        direction = LEFT;
        phi_norm = phi + M_PI_2;
    } else if (phi >= M_PI_4 && phi < M_PI_2 + M_PI_4) {
        direction = RIGHT;
        phi_norm = phi - M_PI_2;
    } else {
        direction = BACK;
        phi_norm = phi + ((phi > 0.f) ? -M_PI : M_PI);
    }

    theta_threshold = atan(cos(phi_norm));
    if (theta > theta_threshold) {
        direction = DOWN;
    } else if (theta < -theta_threshold) {
        direction = UP;
    }

    return direction;
  }

  /**
   * Calculate frame position in equi-angular cubemap format for corresponding 3D coordinates on sphere.
   *
   * @param xyz coordinates on sphere
   * @param width frame width
   * @param height frame height
   * @return uv texture coordinate
   */
  vec2 xyz_to_eac(vec3 xyz, int width, int height)
  {
    // EAC has 2-pixel padding on faces except between faces on the same row
    float pixel_pad = 2;
    float u_pad = pixel_pad / width;
    float v_pad = pixel_pad / height;

    int in_cubemap_face_order[6];
    in_cubemap_face_order[RIGHT] = TOP_RIGHT;
    in_cubemap_face_order[LEFT]  = TOP_LEFT;
    in_cubemap_face_order[UP]    = BOTTOM_RIGHT;
    in_cubemap_face_order[DOWN]  = BOTTOM_LEFT;
    in_cubemap_face_order[FRONT] = TOP_MIDDLE;
    in_cubemap_face_order[BACK]  = BOTTOM_MIDDLE;

    int in_cubemap_face_rotation[6];
    in_cubemap_face_rotation[TOP_LEFT]      = ROT_0;
    in_cubemap_face_rotation[TOP_MIDDLE]    = ROT_0;
    in_cubemap_face_rotation[TOP_RIGHT]     = ROT_0;
    in_cubemap_face_rotation[BOTTOM_LEFT]   = ROT_270;
    in_cubemap_face_rotation[BOTTOM_MIDDLE] = ROT_90;
    in_cubemap_face_rotation[BOTTOM_RIGHT]  = ROT_270;

    int direction = xyz_to_direction(xyz);

    vec2 uv = vec2(0.0, 0.0);
    switch (direction) {
        case LEFT:
            uv.x = -xyz.z / xyz.x;
            uv.y =  xyz.y / xyz.x;
            break;
        case RIGHT:
            uv.x = -xyz.z  / xyz.x;
            uv.y = -xyz.y / xyz.x;
            break;
        case DOWN:
            uv.x = -xyz.x / xyz.y;
            uv.y = -xyz.z  / xyz.y;
            break;
        case UP:
            uv.x =  xyz.x / xyz.y;
            uv.y = -xyz.z  / xyz.y;
            break;
        case BACK:
            uv.x =  -xyz.x / xyz.z;
            uv.y =  -xyz.y / xyz.z;
            break;
        case FRONT:
            uv.x =  xyz.x / xyz.z;
            uv.y = -xyz.y / xyz.z;
            break;
    }

    int face = in_cubemap_face_order[direction];
    uv = rotate_cube_face(uv, in_cubemap_face_rotation[face]);

    int u_face = face % 3;
    int v_face = face / 3;

    uv = M_2_PI * atan(uv) + 0.5;

    uv.x = (uv.x + u_face) * (1.0 - 2.0 * u_pad) / 3.0 + u_pad;
    uv.y = uv.y * (0.5 - (2.0 * v_pad)) + v_pad + (0.5 * v_face);

    return uv;
  }

  void main() {
    vec2 uv = xyz_to_eac(normalize(tr_normal), videoWidth, videoHeight);

    if(flipY) {
        uv.y = 1.0 - uv.y;
    }

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            uv = (uv * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            uv = uv * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            uv = uv * vec2(1.0, 0.5);
            uv = vec2(1.0 - uv.y, uv.x);
        }
    }
    else { // Left Eye or Mono
        if(stereoscopicMode==1) { //Side-by-side
            uv = uv * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            uv = (uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            uv = (uv * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            uv = vec2(1.0 - uv.y, uv.x);
        }
    }
   
    out_color = cplayPost(uv, uv) * vec4(1.0, 1.0, 1.0, alpha);
    if (maskType != 0) {
        vec3 n = normalize(tr_normal);
        vec2 eqr = vec2(atan(n.x, n.z) / (2.0 * M_PI) + 0.5, asin(clamp(n.y, -1.0, 1.0)) / M_PI + 0.5);
        out_color.a *= cplayMask(eqr);
    }
  }
)";

LayersRenderer::LayersRenderer() : meshRadius(0),
                                 meshFov(0) {
}

LayersRenderer::~LayersRenderer() {
    layers2render.clear();
    domeMesh.reset();
    sphereMesh.reset();
}

void LayersRenderer::initializeGL(double radius, double fov) {
    // Create shaders: the plain variant keeps the original names, the post effects variant adds "_post".
    sgct::ShaderManager &shaders = sgct::ShaderManager::instance();
    for (const bool post : { false, true }) {
        const std::string suffix = post ? "_post" : "";
        const std::string postGlsl = cplay::anim::postGlsl(post);
        const std::string videoFrag = std::string(VideoFragHead) + postGlsl + std::string(VideoFragMain);
        const std::string eacFrag = std::string(EACVideoFragHead) + postGlsl + std::string(EACVideoFragMain);
        if (!shaders.shaderProgramExists("mesh" + suffix))
            shaders.addShaderProgram("mesh" + suffix, MeshVert, videoFrag);
        if (!shaders.shaderProgramExists("EAC" + suffix))
            shaders.addShaderProgram("EAC" + suffix, EACMeshVert, eacFrag);
        if (!shaders.shaderProgramExists("video" + suffix))
            shaders.addShaderProgram("video" + suffix, VideoVert, videoFrag);
    }

    // OBS: Need to create all shaders befor using any of them. Bug?
    for (const bool post : { false, true }) {
        sv = &variants[post ? 1 : 0];
        const std::string suffix = post ? "_post" : "";
        sv->meshPrg = &shaders.shaderProgram("mesh" + suffix);
        if (sv->meshPrg) {
            sv->meshPrg->bind();
            glUniform1i(glGetUniformLocation(sv->meshPrg->id(), "tex"), 0);
            sv->meshMatrixLoc = glGetUniformLocation(sv->meshPrg->id(), "mvp");
            sv->meshEyeModeLoc = glGetUniformLocation(sv->meshPrg->id(), "eye");
            sv->meshFlipYLoc = glGetUniformLocation(sv->meshPrg->id(), "flipY");
            sv->meshStereoscopicModeLoc = glGetUniformLocation(sv->meshPrg->id(), "stereoscopicMode");
            sv->meshRoi = glGetUniformLocation(sv->meshPrg->id(), "roi");
            sv->meshAlphaLoc = glGetUniformLocation(sv->meshPrg->id(), "alpha");
            sv->meshOutsideLoc = glGetUniformLocation(sv->meshPrg->id(), "outside");
            sv->meshMaskLocs.locate([this](const char *n) { return glGetUniformLocation(sv->meshPrg->id(), n); });
            sv->meshPrg->unbind();
        }

        sv->EACPrg = &shaders.shaderProgram("EAC" + suffix);
        if (sv->EACPrg) {
            sv->EACPrg->bind();
            glUniform1i(glGetUniformLocation(sv->EACPrg->id(), "tex"), 0);
            sv->EACMatrixLoc = glGetUniformLocation(sv->EACPrg->id(), "mvp");
            sv->EACEyeModeLoc = glGetUniformLocation(sv->EACPrg->id(), "eye");
            sv->EACFlipYLoc = glGetUniformLocation(sv->EACPrg->id(), "flipY");
            sv->EACStereoscopicModeLoc = glGetUniformLocation(sv->EACPrg->id(), "stereoscopicMode");
            sv->EACAlphaLoc = glGetUniformLocation(sv->EACPrg->id(), "alpha");
            sv->EACOutsideLoc = glGetUniformLocation(sv->EACPrg->id(), "outside");
            sv->EACScaleLoc = glGetUniformLocation(sv->EACPrg->id(), "scaleToUnitCube");
            sv->EACVideoWidthLoc = glGetUniformLocation(sv->EACPrg->id(), "videoWidth");
            sv->EACVideoHeightLoc = glGetUniformLocation(sv->EACPrg->id(), "videoHeight");
            sv->EACMaskLocs.locate([this](const char *n) { return glGetUniformLocation(sv->EACPrg->id(), n); });
            sv->EACPrg->unbind();
        }

        sv->videoPrg = &shaders.shaderProgram("video" + suffix);
        if (sv->videoPrg) {
            sv->videoPrg->bind();
            glUniform1i(glGetUniformLocation(sv->videoPrg->id(), "tex"), 0);
            sv->videoEyeModeLoc = glGetUniformLocation(sv->videoPrg->id(), "eye");
            sv->videoFlipYLoc = glGetUniformLocation(sv->videoPrg->id(), "flipY");
            sv->videoStereoscopicModeLoc = glGetUniformLocation(sv->videoPrg->id(), "stereoscopicMode");
            sv->videoRoi = glGetUniformLocation(sv->videoPrg->id(), "roi");
            sv->videoAlphaLoc = glGetUniformLocation(sv->videoPrg->id(), "alpha");
            sv->videoMaskLocs.locate([this](const char *n) { return glGetUniformLocation(sv->videoPrg->id(), n); });
            sv->videoPrg->unbind();
        }
    }
    sv = &variants[0];

    updateMeshes(radius, fov);
}

void LayersRenderer::updateMeshes(double radius, double fov) {
    // Set new general dome/sphere details
    if (meshRadius != radius || meshFov != fov) {
        meshRadius = radius;
        meshFov = fov;
        domeMesh = std::make_unique<DomeGrid>(float(meshRadius) / 100.f, float(meshFov), 256, 128);
        sphereMesh = std::make_unique<SphereGrid>(float(meshRadius) / 100.f, 256);
    }
}

void LayersRenderer::addLayer(std::shared_ptr<BaseLayer> layer) {
    layers2render.push_back(layer);
}

void LayersRenderer::clearLayers() {
    layers2render.clear();
}

const std::vector<std::shared_ptr<BaseLayer>> &LayersRenderer::getLayers() {
    return layers2render;
}

namespace {
struct GlUniformSetter {
    void set1i(int loc, int v) const { glUniform1i(loc, v); }
    void set1f(int loc, float v) const { glUniform1f(loc, v); }
    void set2f(int loc, float a, float b) const { glUniform2f(loc, a, b); }
    void set4fv(int loc, int count, const float *v) const { glUniform4fv(loc, count, v); }
};
}

void LayersRenderer::renderLayer(const sgct::RenderData& data, const BaseLayer* layer, sgct::FrustumMode currentEye, float angle,
                                 const BaseLayer* fxSource) {
    const float alpha = layer->renderAlpha();
    const glm::vec3 rot = layer->renderRotate();
    const glm::vec3 trans = layer->renderTranslate();
    const cplay::anim::LayerAnimState &anim = layer->animState();
    const cplay::anim::PostEffects &fx = (fxSource ? fxSource : layer)->postEffects();
    // Layers without post effects use the plain shaders; restore them for the draws that follow.
    struct VariantRestore {
        ShaderVariant *&current;
        ShaderVariant *plain;
        ~VariantRestore() { current = plain; }
    } variantRestore{sv, &variants[0]};
    sv = &variants[fx.active() ? 1 : 0];
    const float aspect = layer->renderAspectRatio();
    const GlUniformSetter setter;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, layer->textureId());
    glEnable(GL_BLEND);
    // Blend straight-alpha RGB while accumulating coverage separately. Using
    // SRC_ALPHA for alpha would square it and make an opaque background translucent
    // during layer fades. Set this explicitly rather than inheriting SGCT's state.
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                        GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    if (layer->gridMode() == 4) {
        glEnable(GL_CULL_FACE);

        sv->EACPrg->bind();

        glUniform1f(sv->EACAlphaLoc, alpha);
        sv->EACMaskLocs.apply(setter, anim, 2.f, fx);
        glUniform1i(sv->EACFlipYLoc, (layer->renderFlipY() ? 1 : 0));
        glUniform1i(sv->EACOutsideLoc, 0);
        glUniform1i(sv->EACVideoWidthLoc, layer->width());
        glUniform1i(sv->EACVideoHeightLoc, layer->height());

        if (layer->renderStereoMode() > 0) {
            glUniform1i(sv->EACEyeModeLoc, (GLint)currentEye);
            glUniform1i(sv->EACStereoscopicModeLoc, (GLint)layer->renderStereoMode());
        }
        else {
            glUniform1i(sv->EACEyeModeLoc, 0);
            glUniform1i(sv->EACStereoscopicModeLoc, 0);
        }

        const sgct::mat4 mvp = data.modelViewProjectionMatrix;
        glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values.data()), trans);

        glm::mat4 MVP_transformed_rot = MVP_transformed;
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(rot.z), glm::vec3(0.0f, 0.0f, 1.0f));        // roll
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(rot.x), glm::vec3(1.0f, 0.0f, 0.0f));        // pitch
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(rot.y), glm::vec3(0.0f, 1.0f, 0.0f));        // yaw
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(90.f), glm::vec3(0.0f, 0.0f, 1.0f));                     // roll
        glUniformMatrix4fv(sv->EACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

        sphereMesh->draw();

        // Set up frontface culling
        glCullFace(GL_FRONT);

        // Compensate for the angle of the dome
        glm::mat4 MVP_transformed_rot2 = MVP_transformed;
        MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - rot.z), glm::vec3(0.0f, 0.0f, 1.0f));         // roll
        MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - rot.x + angle), glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
        MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - rot.y), glm::vec3(0.0f, 1.0f, 0.0f));  // yaw
        MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - 90.f), glm::vec3(0.0f, 0.0f, 1.0f));                      // roll

        glUniformMatrix4fv(sv->EACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
        // render outside sphere
        glUniform1i(sv->EACOutsideLoc, 1);
        sphereMesh->draw();

        // Set up backface culling again
        glCullFace(GL_BACK);
        glUniform1i(sv->EACOutsideLoc, 0);

        sv->EACPrg->unbind();

        glDisable(GL_CULL_FACE);
    }
    else if (layer->gridMode() == 3) {
        glEnable(GL_CULL_FACE);

        const sgct::mat4 mvp = data.modelViewProjectionMatrix;
        glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values.data()), trans);

        glm::mat4 MVP_transformed_rot = MVP_transformed;
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(rot.z), glm::vec3(0.0f, 0.0f, 1.0f)); // roll
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(rot.x), glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(rot.y - 90.f), glm::vec3(0.0f, 1.0f, 0.0f)); // yaw

        sv->meshPrg->bind();

        if (layer->renderStereoMode() > 0) {
            glUniform1i(sv->meshEyeModeLoc, (GLint)currentEye);
            glUniform1i(sv->meshStereoscopicModeLoc, (GLint)layer->renderStereoMode());
        }
        else {
            glUniform1i(sv->meshEyeModeLoc, 0);
            glUniform1i(sv->meshStereoscopicModeLoc, 0);
        }

        if (layer->roiEnabled()) {
            glUniform4fv(sv->meshRoi, 1, glm::value_ptr(layer->roi()));
        }
        else {
            glUniform4f(sv->meshRoi, 0.f, 0.f, 1.f, 1.f);
        }

        glUniform1f(sv->meshAlphaLoc, alpha);
        sv->meshMaskLocs.apply(setter, anim, 2.f, fx);
        glUniform1i(sv->meshFlipYLoc, (layer->renderFlipY() ? 1 : 0));

        glUniformMatrix4fv(sv->meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

        // render inside sphere
        glUniform1i(sv->meshOutsideLoc, 0);
        sphereMesh->draw();

        // Set up frontface culling
        glCullFace(GL_FRONT);

        // Compensate for the angle of the dome
        glm::mat4 MVP_transformed_rot2 = MVP_transformed;
        MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - rot.z), glm::vec3(0.0f, 0.0f, 1.0f));         // roll
        MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - rot.x + angle), glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
        MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - rot.y + 90.f), glm::vec3(0.0f, 1.0f, 0.0f));         // yaw

        glUniformMatrix4fv(sv->meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
        // render outside sphere
        glUniform1i(sv->meshOutsideLoc, 1);
        sphereMesh->draw();

        // Set up backface culling again
        glCullFace(GL_BACK);
        glUniform1i(sv->meshOutsideLoc, 0);

        sv->meshPrg->unbind();

        glDisable(GL_CULL_FACE);
    }
    else if (layer->gridMode() == 2) {
        glEnable(GL_CULL_FACE);

        sv->meshPrg->bind();

        if (layer->renderStereoMode() > 0) {
            glUniform1i(sv->meshEyeModeLoc, (GLint)currentEye);
            glUniform1i(sv->meshStereoscopicModeLoc, (GLint)layer->renderStereoMode());
        }
        else {
            glUniform1i(sv->meshEyeModeLoc, 0);
            glUniform1i(sv->meshStereoscopicModeLoc, 0);
        }

        if (layer->roiEnabled()) {
            glUniform4fv(sv->meshRoi, 1, glm::value_ptr(layer->roi()));
        }
        else {
            glUniform4f(sv->meshRoi, 0.f, 0.f, 1.f, 1.f);
        }

        glUniform1f(sv->meshAlphaLoc, alpha);
        sv->meshMaskLocs.apply(setter, anim, 2.f, fx);
        glUniform1i(sv->meshFlipYLoc, (layer->renderFlipY() ? 1 : 0));

        const sgct::mat4 mvp = data.modelViewProjectionMatrix;
        glm::mat4 MVP_transformed_rot = glm::translate(glm::make_mat4(mvp.values.data()), trans);
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(rot.z), glm::vec3(0.0f, 0.0f, 1.0f));         // roll
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(rot.x - angle), glm::vec3(1.0f, 0.0f, 0.0f)); // pitch
        MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(rot.y), glm::vec3(0.0f, 1.0f, 0.0f));         // yaw
        glUniformMatrix4fv(sv->meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

        domeMesh->draw();

        sv->meshPrg->unbind();

        glDisable(GL_CULL_FACE);
    }
    else if (layer->gridMode() == 1) {
        glEnable(GL_CULL_FACE);
        // Set up frontface culling
        glCullFace(GL_FRONT);

        sv->meshPrg->bind();

        if (layer->renderStereoMode() > 0) {
            glUniform1i(sv->meshEyeModeLoc, (GLint)currentEye);
            glUniform1i(sv->meshStereoscopicModeLoc, (GLint)layer->renderStereoMode());
        }
        else {
            glUniform1i(sv->meshEyeModeLoc, 0);
            glUniform1i(sv->meshStereoscopicModeLoc, 0);
        }

        if (layer->roiEnabled()) {
            glUniform4fv(sv->meshRoi, 1, glm::value_ptr(layer->roi()));
        }
        else {
            glUniform4f(sv->meshRoi, 0.f, 0.f, 1.f, 1.f);
        }

        glUniform1f(sv->meshAlphaLoc, alpha);
        sv->meshMaskLocs.apply(setter, anim, aspect, fx);
        glUniform1i(sv->meshFlipYLoc, (layer->renderFlipY() ? 1 : 0));

        const sgct::mat4 mvp = data.projectionMatrix * data.viewMatrix;

        if (layer->circularPlaneActive()) {
            const_cast<BaseLayer*>(layer)->updatePlane();
            const auto mapping = layer->circularMapping();
            const auto copies = cplay::circular::copies(layer->circularPlaneModel(), layer->planeActualSize(), mapping.widthCm);
            const glm::mat4 tilt = glm::rotate(glm::mat4(1.f), glm::radians(-angle), glm::vec3(1, 0, 0));
            const glm::mat4 vp = glm::make_mat4(mvp.values.data());
            GLboolean previous[4];
            for (int i = 0; i < 4; ++i) { previous[i] = glIsEnabled(GL_CLIP_DISTANCE0 + i); glEnable(GL_CLIP_DISTANCE0 + i); }
            glUniform1f(glGetUniformLocation(sv->meshPrg->id(), "circularHalfWidth"), float(mapping.widthCm / 200.0));
            for (const auto &copy : copies) {
                const glm::mat4 matrix = vp * tilt * copy.model;
                glUniformMatrix4fv(sv->meshMatrixLoc, 1, GL_FALSE, &matrix[0][0]);
                glUniformMatrix4fv(glGetUniformLocation(sv->meshPrg->id(), "circularModel"), 1, GL_FALSE, &copy.model[0][0]);
                glUniform2fv(glGetUniformLocation(sv->meshPrg->id(), "circularSourceBounds"), 1, glm::value_ptr(copy.sourceBounds));
                layer->drawPlane();
            }
            for (int i = 0; i < 4; ++i) if (!previous[i]) glDisable(GL_CLIP_DISTANCE0 + i);
        } else {
            glm::mat4 planeTransform = glm::mat4(1.0f);

            //Respect the dome angle
            planeTransform = glm::rotate(planeTransform, glm::radians(-angle), glm::vec3(1.0f, 0.0f, 0.0f));

            //Specifc plane parameters
            planeTransform = glm::rotate(planeTransform, glm::radians(float(layer->renderPlaneAzimuth())), glm::vec3(0.0f, -1.0f, 0.0f));  // azimuth
            planeTransform = glm::rotate(planeTransform, glm::radians(float(layer->renderPlaneElevation())), glm::vec3(1.0f, 0.0f, 0.0f)); // elevation
            planeTransform = glm::rotate(planeTransform, glm::radians(float(layer->renderPlaneRoll())), glm::vec3(0.0f, 0.0f, 1.0f));      // roll
            planeTransform = glm::translate(planeTransform, glm::vec3(float(layer->renderPlaneHorizontal()) / 100.f, float(layer->renderPlaneVertical()) / 100.f, float(-layer->renderPlaneDistance()) / 100.f));
            if (layer->animActive() && anim.scale != 1.f)
                planeTransform = glm::scale(planeTransform, glm::vec3(anim.scale, anim.scale, 1.f));

            planeTransform = glm::make_mat4(mvp.values.data()) * planeTransform;
            glUniformMatrix4fv(sv->meshMatrixLoc, 1, GL_FALSE, &planeTransform[0][0]);

            layer->drawPlane();

        }
        sv->meshPrg->unbind();

        // Set up backface culling again
        glCullFace(GL_BACK);

        glDisable(GL_CULL_FACE);
    }
    else {
        sv->videoPrg->bind();

        if (layer->renderStereoMode() > 0) {
            glUniform1i(sv->videoEyeModeLoc, (GLint)currentEye);
            glUniform1i(sv->videoStereoscopicModeLoc, (GLint)layer->renderStereoMode());
        }
        else {
            glUniform1i(sv->videoEyeModeLoc, 0);
            glUniform1i(sv->videoStereoscopicModeLoc, 0);
        }

        if (layer->roiEnabled()) {
            glUniform4fv(sv->videoRoi, 1, glm::value_ptr(layer->roi()));
        }
        else {
            glUniform4f(sv->videoRoi, 0.f, 0.f, 1.f, 1.f);
        }

        glUniform1f(sv->videoAlphaLoc, alpha);
        sv->videoMaskLocs.apply(setter, anim, aspect, fx);
        GLint viewport[4] = {0, 0, 1, 1};
        if (layer->animActive() && anim.spin != 0.f)
            glGetIntegerv(GL_VIEWPORT, viewport);
        const glm::mat4 quad = cplay::anim::flatQuadMatrix(anim, layer->animActive(),
            viewport[3] > 0 ? static_cast<float>(viewport[2]) / static_cast<float>(viewport[3]) : 1.f,
            layer->screenRect());
        glUniformMatrix4fv(sv->videoMaskLocs.quadMatrix, 1, GL_FALSE, &quad[0][0]);
        glUniform1i(sv->videoFlipYLoc, (layer->renderFlipY() ? 1 : 0));

        data.window.renderScreenQuad();

        sv->videoPrg->unbind();
    }
}

void LayersRenderer::renderLayers(const sgct::RenderData &data, int viewMode, float angle) {
    sgct::FrustumMode currentEye = data.frustumMode;

    // Check if we force all viewports to 2D, meaning only show LeftEye if 3D
    if (viewMode == 1 && currentEye == sgct::FrustumMode::StereoRight) {
        currentEye = sgct::FrustumMode::StereoLeft;
    }

    const int currentEyeInt = static_cast<int>(currentEye);

    for (const auto &layer : layers2render) {
        if (layer->hasSubLayers()) {
            for (const auto& sublayer : layer->getSubLayers()) {
                if (sublayer->shouldRenderForEye(currentEyeInt))
                    renderLayer(data, sublayer.get(), currentEye, angle, layer.get());
            }
        }
        else if (!layer->isQRCodeDetectionEnabled() || layer->isQRCodeDetectionEnabled() && !layer->hasSubLayers()) {
            if (layer->shouldRenderForEye(currentEyeInt))
                renderLayer(data, layer.get(), currentEye, angle);
        }
    }
}
