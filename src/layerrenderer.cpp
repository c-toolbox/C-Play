/*
 * SPDX-FileCopyrightText:
 * 2024 Erik Sundén <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "layerrenderer.h"
#include <sgct/opengl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

constexpr std::string_view VideoVert = R"(
  #version 410 core

  layout (location = 0) in vec2 in_position;
  layout (location = 1) in vec2 in_texCoord;

  uniform int eye;
  uniform int stereoscopicMode;

  out vec2 tr_uv;

  void main() {
    gl_Position = vec4(in_position, 0.0, 1.0);
    tr_uv = in_texCoord;

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (in_texCoord * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = in_texCoord * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = in_texCoord * vec2(1.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else { //Left Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = in_texCoord * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (in_texCoord * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (in_texCoord * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
  }
)";

constexpr std::string_view MeshVert = R"(
  #version 410 core

  layout (location = 0) in vec2 in_texCoord;
  layout (location = 1) in vec3 in_normal;
  layout (location = 2) in vec3 in_position;

  uniform mat4 mvp;
  uniform int eye;
  uniform int stereoscopicMode;

  out vec2 tr_uv;
  out vec3 tr_normals;

  void main() {
    gl_Position = mvp * vec4(in_position, 1.0);
    tr_uv = in_texCoord;
    tr_normals = in_normal;

    if(eye==2) { //Right Eye
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = (in_texCoord * vec2(0.5, 1.0)) + vec2(0.5, 0.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = in_texCoord * vec2(1.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = in_texCoord * vec2(1.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
    else { // Left Eye or Mono
        if(stereoscopicMode==1) { //Side-by-side
            tr_uv = in_texCoord * vec2(0.5, 1.0);
        }
        else if(stereoscopicMode==2) { //Top-bottom
            tr_uv = (in_texCoord * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
        }
        else if(stereoscopicMode==3) { //Top-bottom-flip
            tr_uv = (in_texCoord * vec2(1.0, 0.5)) + vec2(0.0, 0.5);
            tr_uv = vec2(1.0 - tr_uv.y, tr_uv.x);
        }
    }
  }
)";

constexpr std::string_view VideoFrag = R"(
  #version 410 core

  uniform sampler2D tex;
  uniform float alpha;
  uniform bool outside;

  in vec2 tr_uv;
  in vec3 tr_normals;
  out vec4 out_color;

  void main() {
    vec2 texCoods = tr_uv;
    if(outside){
        texCoods = vec2(1.0-tr_uv.x, tr_uv.y);
    }
   
    out_color = texture(tex, texCoods) * vec4(1.0, 1.0, 1.0, alpha);
  }
)";

constexpr std::string_view EACMeshVert = R"(
  #version 410 core

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

constexpr std::string_view EACVideoFrag = R"(
  #version 410 core

  uniform sampler2D tex;
  uniform int eye;
  uniform int stereoscopicMode;
  uniform float alpha;
  uniform int videoWidth;
  uniform int videoHeight;

  in vec3 tr_position;
  in vec3 tr_normal;
  out vec4 out_color;

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
   
    out_color = texture(tex, uv) * vec4(1.0, 1.0, 1.0, alpha);
  }
)";

LayerRenderer::LayerRenderer() :
    meshRadius(0),
    meshFov(0),
    videoAlphaLoc(-1),
    videoEyeModeLoc(-1),
    videoStereoscopicModeLoc(-1),
    meshAlphaLoc(-1),
    meshEyeModeLoc(-1),
    meshMatrixLoc(-1),
    meshOutsideLoc(-1),
    meshStereoscopicModeLoc(-1),
    EACAlphaLoc(-1),
    EACMatrixLoc(-1),
    EACScaleLoc(-1),
    EACOutsideLoc(-1),
    EACVideoWidthLoc(-1),
    EACVideoHeightLoc(-1),
    EACEyeModeLoc(-1),
    EACStereoscopicModeLoc(-1),
    videoPrg(nullptr),
    meshPrg(nullptr),
    EACPrg(nullptr) {

}

LayerRenderer::~LayerRenderer() {
    domeMesh = nullptr;
    sphereMesh = nullptr;
}

void LayerRenderer::initialize(double radius, double fov) {
    // Create shaders
    sgct::ShaderManager::instance().addShaderProgram("mesh", MeshVert, VideoFrag);
    sgct::ShaderManager::instance().addShaderProgram("EAC", EACMeshVert, EACVideoFrag);
    sgct::ShaderManager::instance().addShaderProgram("video", VideoVert, VideoFrag);

    //OBS: Need to create all shaders befor using any of them. Bug?
    meshPrg = &sgct::ShaderManager::instance().shaderProgram("mesh");
    meshPrg->bind();
    glUniform1i(glGetUniformLocation(meshPrg->id(), "tex"), 0);
    meshMatrixLoc = glGetUniformLocation(meshPrg->id(), "mvp");
    meshEyeModeLoc = glGetUniformLocation(meshPrg->id(), "eye");
    meshStereoscopicModeLoc = glGetUniformLocation(meshPrg->id(), "stereoscopicMode");
    meshAlphaLoc = glGetUniformLocation(meshPrg->id(), "alpha");
    meshOutsideLoc = glGetUniformLocation(meshPrg->id(), "outside");
    meshPrg->unbind();

    EACPrg = &sgct::ShaderManager::instance().shaderProgram("EAC");
    EACPrg->bind();
    glUniform1i(glGetUniformLocation(EACPrg->id(), "tex"), 0);
    EACMatrixLoc = glGetUniformLocation(EACPrg->id(), "mvp");
    EACEyeModeLoc = glGetUniformLocation(EACPrg->id(), "eye");
    EACStereoscopicModeLoc = glGetUniformLocation(EACPrg->id(), "stereoscopicMode");
    EACAlphaLoc = glGetUniformLocation(EACPrg->id(), "alpha");
    EACOutsideLoc = glGetUniformLocation(meshPrg->id(), "outside");
    EACScaleLoc = glGetUniformLocation(EACPrg->id(), "scaleToUnitCube");
    EACVideoWidthLoc = glGetUniformLocation(EACPrg->id(), "videoWidth");
    EACVideoHeightLoc = glGetUniformLocation(EACPrg->id(), "videoHeight");
    EACPrg->unbind();

    videoPrg = &sgct::ShaderManager::instance().shaderProgram("video");
    videoPrg->bind();
    glUniform1i(glGetUniformLocation(videoPrg->id(), "tex"), 0);
    videoEyeModeLoc = glGetUniformLocation(videoPrg->id(), "eye");
    videoStereoscopicModeLoc = glGetUniformLocation(videoPrg->id(), "stereoscopicMode");
    videoAlphaLoc = glGetUniformLocation(videoPrg->id(), "alpha");
    videoPrg->unbind();

    updateMeshes(radius, fov);
}

void LayerRenderer::updateMeshes(double radius, double fov) {
    // Set new general dome/sphere details
    if (meshRadius != radius || meshFov != fov) {
        domeMesh = nullptr;
        sphereMesh = nullptr;
        meshRadius = radius;
        meshFov = fov;
        domeMesh = std::make_unique<sgct::utils::Dome>(float(meshRadius) / 100.f, float(meshFov), 256, 128);
        sphereMesh = std::make_unique<sgct::utils::Sphere>(float(meshRadius) / 100.f, 256);
    }
}

void LayerRenderer::addLayer(BaseLayer* layer) {
    layers2render.push_back(layer);
}

void LayerRenderer::clearLayers() {
    layers2render.clear();
}

const std::vector<BaseLayer*>& LayerRenderer::getLayers() {
    return layers2render;
}

void LayerRenderer::renderLayers(const sgct::RenderData& data, int viewMode, float angle) {
    sgct::Frustum::Mode currentEye = data.frustumMode;

    //Check if we force all viewports to 2D, meaning only show LeftEye if 3D
    if (viewMode == 1 && currentEye == sgct::Frustum::Mode::StereoRightEye) {
        currentEye = sgct::Frustum::Mode::StereoLeftEye;
    }

    for (const auto& layer : layers2render) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, layer->textureId());
        glEnable(GL_BLEND);

        if (layer->gridMode() == 4) {
            glEnable(GL_CULL_FACE);

            EACPrg->bind();

            glUniform1f(EACAlphaLoc, layer->alpha());
            glUniform1i(EACOutsideLoc, 0);
            glUniform1i(EACVideoWidthLoc, layer->width());
            glUniform1i(EACVideoHeightLoc, layer->height());

            if (layer->stereoMode() > 0) {
                glUniform1i(EACEyeModeLoc, (GLint)currentEye);
                glUniform1i(EACStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(EACEyeModeLoc, 0);
                glUniform1i(EACStereoscopicModeLoc, 0);
            }

            const sgct::mat4 mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values), layer->translate());

            glm::mat4 MVP_transformed_rot = MVP_transformed;
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().x), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().y + 90.f), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(90.f), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            glUniformMatrix4fv(EACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            sphereMesh->draw();

            // Set up frontface culling
            glCullFace(GL_FRONT);

            // Compensate for the angle of the dome
            glm::mat4 MVP_transformed_rot2 = MVP_transformed;
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().x + angle), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().y - 90.f), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - 90.f), glm::vec3(0.0f, 0.0f, 1.0f)); //roll

            glUniformMatrix4fv(EACMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
            //render outside sphere
            glUniform1i(EACOutsideLoc, 1);
            sphereMesh->draw();

            // Set up backface culling again
            glCullFace(GL_BACK);
            glUniform1i(EACOutsideLoc, 0);

            EACPrg->unbind();

            glDisable(GL_CULL_FACE);
        }
        else if (layer->gridMode() == 3) {
            glEnable(GL_CULL_FACE);

            const sgct::mat4 mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed = glm::translate(glm::make_mat4(mvp.values), layer->translate());

            glm::mat4 MVP_transformed_rot = MVP_transformed;
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().x), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().y), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw

            meshPrg->bind();

            if (layer->stereoMode() > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)currentEye);
                glUniform1i(meshStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, layer->alpha());

            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            //render inside sphere
            glUniform1i(meshOutsideLoc, 0);
            sphereMesh->draw();

            // Set up frontface culling
            glCullFace(GL_FRONT);

            // Compensate for the angle of the dome
            glm::mat4 MVP_transformed_rot2 = MVP_transformed;
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().x + angle), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot2 = glm::rotate(MVP_transformed_rot2, glm::radians(360.f - layer->rotate().y), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw

            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot2[0][0]);
            //render outside sphere
            glUniform1i(meshOutsideLoc, 1);
            sphereMesh->draw();

            // Set up backface culling again
            glCullFace(GL_BACK);
            glUniform1i(meshOutsideLoc, 0);

            meshPrg->unbind();

            glDisable(GL_CULL_FACE);
        }
        else if (layer->gridMode() == 2) {
            glEnable(GL_CULL_FACE);

            meshPrg->bind();

            if (layer->stereoMode() > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)currentEye);
                glUniform1i(meshStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, layer->alpha());

            const sgct::mat4 mvp = data.modelViewProjectionMatrix;
            glm::mat4 MVP_transformed_rot = glm::translate(glm::make_mat4(mvp.values), layer->translate());
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().z), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().x - angle), glm::vec3(1.0f, 0.0f, 0.0f)); //pitch
            MVP_transformed_rot = glm::rotate(MVP_transformed_rot, glm::radians(layer->rotate().y), glm::vec3(0.0f, 1.0f, 0.0f)); //yaw
            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &MVP_transformed_rot[0][0]);

            domeMesh->draw();

            meshPrg->unbind();

            glDisable(GL_CULL_FACE);
        }
        else if (layer->gridMode() == 1) {
            glEnable(GL_CULL_FACE);
            // Set up frontface culling
            glCullFace(GL_FRONT);

            meshPrg->bind();

            if (layer->stereoMode() > 0) {
                glUniform1i(meshEyeModeLoc, (GLint)currentEye);
                glUniform1i(meshStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(meshEyeModeLoc, 0);
                glUniform1i(meshStereoscopicModeLoc, 0);
            }

            glUniform1f(meshAlphaLoc, layer->alpha());

            const sgct::mat4 mvp = data.projectionMatrix * data.viewMatrix;

            glm::mat4 planeTransform = glm::mat4(1.0f);
            planeTransform = glm::rotate(planeTransform, glm::radians(float(layer->planeAzimuth())), glm::vec3(0.0f, -1.0f, 0.0f)); //azimuth
            planeTransform = glm::rotate(planeTransform, glm::radians(float(layer->planeElevation())), glm::vec3(1.0f, 0.0f, 0.0f)); //elevation
            planeTransform = glm::rotate(planeTransform, glm::radians(float(layer->planeRoll())), glm::vec3(0.0f, 0.0f, 1.0f)); //roll
            planeTransform = glm::translate(planeTransform, glm::vec3(0.0f, 0.0f, float(-layer->planeDistance()) / 100.f)); //distance
            planeTransform = glm::make_mat4(mvp.values) * planeTransform;
            glUniformMatrix4fv(meshMatrixLoc, 1, GL_FALSE, &planeTransform[0][0]);

            layer->drawPlane();

            meshPrg->unbind();

            // Set up backface culling again
            glCullFace(GL_BACK);

            glDisable(GL_CULL_FACE);
        }
        else {
            videoPrg->bind();

            if (layer->stereoMode() > 0) {
                glUniform1i(videoEyeModeLoc, (GLint)currentEye);
                glUniform1i(videoStereoscopicModeLoc, (GLint)layer->stereoMode());
            }
            else {
                glUniform1i(videoEyeModeLoc, 0);
                glUniform1i(videoStereoscopicModeLoc, 0);
            }

            glUniform1f(videoAlphaLoc, layer->alpha());

            data.window.renderScreenQuad();

            videoPrg->unbind();
        }
    }
}
