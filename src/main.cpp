/*
 * SPDX-FileCopyrightText:
 * 2021-2025 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sgct/opengl.h>
#include <sgct/sgct.h>
#define GLFW_INCLUDE_NONE
#include "application.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <glm/glm.hpp>
#include <layerrenderer.h>
#include <layers/baselayer.h>
#include <layers/imagelayer.h>
#include <layers/videolayer.h>
#include <layers/textlayer.h>
#include <layersmodel.h>
#include <mutex>
#include <slidesmodel.h>

#ifdef MDK_SUPPORT
#include <mdk/global.h>
#include <layers/adaptivevideolayer.h>
#endif

#ifdef NDI_SUPPORT
#include <ndi/ndilayer.h>
#endif
// #define SGCT_ONLY

namespace {

bool allowDirectRendering = false;

std::mutex logMutex;
std::ofstream logFile;
std::string logFilePath = "";
std::string logLevel = "";
std::string startupFile = "";

std::vector<std::shared_ptr<BaseLayer>> primaryLayers;
std::shared_ptr<ImageLayer> backgroundImageLayer;
std::shared_ptr<ImageLayer> foregroundImageLayer;
std::shared_ptr<ImageLayer> overlayImageLayer;
#ifdef MDK_SUPPORT
std::shared_ptr <AdaptiveVideoLayer> mainVideoLayer;
#else
std::shared_ptr<VideoLayer> mainVideoLayer;
#endif
std::shared_ptr<TextLayer> mainSubtitleLayer;


bool updateLayers = false;
bool preLoadLayers = false;
std::vector<std::shared_ptr<BaseLayer>> secondaryLayers;
std::vector<std::shared_ptr<BaseLayer>> secondaryLayersToKeep;

std::shared_ptr<LayerRenderer> layerRender;

} // namespace

using namespace sgct;

static void *get_proc_address_glfw_v1(void*, const char *name) {
    return reinterpret_cast<void *>(glfwGetProcAddress(name));
}
static void* get_proc_address_glfw_v2(const char* name, void*) {
    return reinterpret_cast<void*>(glfwGetProcAddress(name));
}

static void initOGL(GLFWwindow *) {
#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif

    // Create standard layers
    backgroundImageLayer = std::make_shared<ImageLayer>("background");
    primaryLayers.push_back(backgroundImageLayer);

#ifdef MDK_SUPPORT
    mainVideoLayer = std::make_shared<AdaptiveVideoLayer>(get_proc_address_glfw_v1, get_proc_address_glfw_v2, allowDirectRendering, !logFilePath.empty() || !logLevel.empty(), logLevel);
#else
    mainVideoLayer = std::make_shared<VideoLayer>(get_proc_address_glfw_v1, allowDirectRendering, !logFilePath.empty() || !logLevel.empty(), logLevel);
#endif
    
    mainVideoLayer->initializeAndLoad(SyncHelper::instance().variables.loadedFile);
    primaryLayers.push_back(mainVideoLayer);

    overlayImageLayer = std::make_shared<ImageLayer>("overlay");
    primaryLayers.push_back(overlayImageLayer);

    foregroundImageLayer = std::make_shared<ImageLayer>("foreground");
    primaryLayers.push_back(foregroundImageLayer);

    mainSubtitleLayer = std::make_shared<TextLayer>();

    layerRender = std::make_shared<LayerRenderer>();
    layerRender->initializeGL(SyncHelper::instance().variables.radius, SyncHelper::instance().variables.fov);

    // Set up backface culling
    glCullFace(GL_BACK);
    // our polygon winding is clockwise since we are inside of the dome
    glFrontFace(GL_CW);
}

static void preSync() {
}

static std::vector<std::byte> encode() {
    std::vector<std::byte> data;
    serializeObject(data, SyncHelper::instance().variables.syncOn);
    serializeObject(data, SyncHelper::instance().variables.alpha);
    serializeObject(data, SyncHelper::instance().variables.alphaBg);
    serializeObject(data, SyncHelper::instance().variables.alphaFg);

    if (SyncHelper::instance().variables.syncOn) {
        serializeObject(data, SyncHelper::instance().variables.paused);
        serializeObject(data, SyncHelper::instance().variables.timePosition);
        serializeObject(data, SyncHelper::instance().variables.timeThreshold);
        serializeObject(data, SyncHelper::instance().variables.timeThresholdEnabled);
        serializeObject(data, SyncHelper::instance().variables.timeThresholdOnLoopOnly);
        serializeObject(data, SyncHelper::instance().variables.timeDirty);
        serializeObject(data, SyncHelper::instance().variables.gridToMapOn);
        serializeObject(data, SyncHelper::instance().variables.gridToMapOnBg);
        serializeObject(data, SyncHelper::instance().variables.gridToMapOnFg);
        serializeObject(data, SyncHelper::instance().variables.stereoscopicMode);
        serializeObject(data, SyncHelper::instance().variables.stereoscopicModeBg);
        serializeObject(data, SyncHelper::instance().variables.stereoscopicModeFg);
        serializeObject(data, SyncHelper::instance().variables.eofMode);
        serializeObject(data, SyncHelper::instance().variables.viewMode);
        serializeObject(data, SyncHelper::instance().variables.radius);
        serializeObject(data, SyncHelper::instance().variables.fov);
        serializeObject(data, SyncHelper::instance().variables.angle);
        serializeObject(data, SyncHelper::instance().variables.rotateX);
        serializeObject(data, SyncHelper::instance().variables.rotateY);
        serializeObject(data, SyncHelper::instance().variables.rotateZ);
        serializeObject(data, SyncHelper::instance().variables.translateX);
        serializeObject(data, SyncHelper::instance().variables.translateY);
        serializeObject(data, SyncHelper::instance().variables.translateZ);
        serializeObject(data, SyncHelper::instance().variables.planeWidth);
        serializeObject(data, SyncHelper::instance().variables.planeHeight);
        serializeObject(data, SyncHelper::instance().variables.planeElevation);
        serializeObject(data, SyncHelper::instance().variables.planeDistance);
        serializeObject(data, SyncHelper::instance().variables.planeConsiderAspectRatio);

        // Eq
        serializeObject(data, SyncHelper::instance().variables.eqDirty);
        if (SyncHelper::instance().variables.eqDirty) {
            serializeObject(data, SyncHelper::instance().variables.eqContrast);
            serializeObject(data, SyncHelper::instance().variables.eqBrightness);
            serializeObject(data, SyncHelper::instance().variables.eqGamma);
            serializeObject(data, SyncHelper::instance().variables.eqSaturation);
        }

        // Looptime
        serializeObject(data, SyncHelper::instance().variables.loopTimeDirty);
        if (SyncHelper::instance().variables.loopTimeDirty) {
            serializeObject(data, SyncHelper::instance().variables.loopTimeEnabled);
            serializeObject(data, SyncHelper::instance().variables.loopTimeA);
            serializeObject(data, SyncHelper::instance().variables.loopTimeB);
        }

        // Window features
        serializeObject(data, SyncHelper::instance().variables.windowOnTop);
        serializeObject(data, SyncHelper::instance().variables.windowOpacity);

        // Speed
        serializeObject(data, SyncHelper::instance().variables.speedDirty);
        if (SyncHelper::instance().variables.speedDirty) {
            serializeObject(data, SyncHelper::instance().variables.playbackSpeed);
        }

        // As strings can be quiet long.
        // Saving connection load, to only send one URL at a time.
        if (SyncHelper::instance().variables.loadFile) { // ID: 0 = mpv media file
            serializeObject(data, 0);
            serializeObject(data, SyncHelper::instance().variables.loadFile);
            serializeObject(data, SyncHelper::instance().variables.loadedFile);
            SyncHelper::instance().variables.loadFile = false;
        } else if (SyncHelper::instance().variables.overlayFileDirty) { // ID: 1 = overlay image file
            serializeObject(data, 1);
            serializeObject(data, SyncHelper::instance().variables.overlayFileDirty);
            serializeObject(data, SyncHelper::instance().variables.overlayFile);
            SyncHelper::instance().variables.overlayFileDirty = false;
        } else if (SyncHelper::instance().variables.bgImageFileDirty) { // ID: 2 = background image file
            serializeObject(data, 2);
            serializeObject(data, SyncHelper::instance().variables.bgImageFileDirty);
            serializeObject(data, SyncHelper::instance().variables.bgImageFile);
            SyncHelper::instance().variables.bgImageFileDirty = false;
        } else if (SyncHelper::instance().variables.fgImageFileDirty) { // ID: 3 = foreground image file
            serializeObject(data, 3);
            serializeObject(data, SyncHelper::instance().variables.fgImageFileDirty);
            serializeObject(data, SyncHelper::instance().variables.fgImageFile);
            SyncHelper::instance().variables.fgImageFileDirty = false;
        } else { // Sending no URL
            serializeObject(data, -1);
        }

        // Subtitle sync
        if (SyncHelper::instance().variables.subtitleText) {
            serializeObject(data, true);
            if (SyncHelper::instance().variables.subtitleText->needSync()) {
                serializeObject(data, true); // Full sync
                SyncHelper::instance().variables.subtitleText->encodeFull(data);
                SyncHelper::instance().variables.subtitleText->setHasSynced();
            }
            else {
                serializeObject(data, false); // Just always
                SyncHelper::instance().variables.subtitleText->encodeAlways(data);
            }
        }
        else { // No subtitle layer to sync
            serializeObject(data, false);
        }

        // Always syncing master slide, selected slide and previous slide so fade-down can occur.
        // Ideally, should most likely sync slide after selected as well
        // Currently syncing master slide and selected slide
        // Need figure out other scheme
        std::vector<int> slideIdxToSync;

        // Sync all slides and layers for now... if not layer set as "existOnMasterOnly"
        for (int i = Application::instance().slidesModel()->numberOfSlides() - 1; i >= -1; i--) {
            slideIdxToSync.push_back(i);
        }

        // Check if model says sync needed
        int totalLayersToSync = 0;
        bool needLayerSync = Application::instance().slidesModel()->needsSync();
        for (auto s : slideIdxToSync) {
            if (s < Application::instance().slidesModel()->numberOfSlides()) {
                LayersModel* slide = Application::instance().slidesModel()->slide(s);
                int numLayers = slide->numberOfLayers();
                if (slide->needsSync()) {
                    needLayerSync = true;
                }
                for (int l = 0; l < numLayers; l++) {
                    BaseLayer* layer = slide->layer(l);
                    if (!layer->existOnMasterOnly()) {
                        totalLayersToSync++;
                        if (layer->needSync()) {
                            needLayerSync = true;
                        }
                    }
                }
            }
        }

        // Sync if layer sync is needed
        serializeObject(data, needLayerSync);
        if (needLayerSync) {
            serializeObject(data, Application::instance().slidesModel()->preLoadLayers());
            // Layers sync...
            // Orders is top to bottom in the list = (first to last in the vector)
            // Sync only complete layer information when needed
            serializeObject(data, totalLayersToSync);
            for (auto s : slideIdxToSync) {
                if (s < Application::instance().slidesModel()->numberOfSlides()) {
                    int numLayers = Application::instance().slidesModel()->slide(s)->numberOfLayers();
                    for (int l = 0; l < numLayers; l++) {
                        BaseLayer *nextLayer = Application::instance().slidesModel()->slide(s)->layer(l);
                        if(!nextLayer->existOnMasterOnly()) {
                            serializeObject(data, nextLayer->identifier()); // ID
                            bool needSync = nextLayer->needSync();
                            serializeObject(data, needSync);   // Check needs sync
                            serializeObject(data, static_cast<int>(nextLayer->type())); // Type
                            if (needSync) {
                                nextLayer->encodeFull(data);
                                nextLayer->setHasSynced();
                            }
                            else {
                                nextLayer->encodeAlways(data);
                            }
                        }
                    }
                    Application::instance().slidesModel()->slide(s)->setHasSynced();
                }
            }
            Application::instance().slidesModel()->setHasSynced();
        }

        // Reset flags every frame cycle
        SyncHelper::instance().variables.timeDirty = false;
        SyncHelper::instance().variables.eqDirty = false;
        SyncHelper::instance().variables.loopTimeDirty = false;
        SyncHelper::instance().variables.speedDirty = false;
    }

    return data;
}

static void decode(const std::vector<std::byte> &data) {
    unsigned int pos = 0;
    deserializeObject(data, pos, SyncHelper::instance().variables.syncOn);
    deserializeObject(data, pos, SyncHelper::instance().variables.alpha);
    deserializeObject(data, pos, SyncHelper::instance().variables.alphaBg);
    deserializeObject(data, pos, SyncHelper::instance().variables.alphaFg);
    if (SyncHelper::instance().variables.syncOn) {
        deserializeObject(data, pos, SyncHelper::instance().variables.paused);
        deserializeObject(data, pos, SyncHelper::instance().variables.timePosition);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeThreshold);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeThresholdEnabled);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeThresholdOnLoopOnly);
        deserializeObject(data, pos, SyncHelper::instance().variables.timeDirty);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOn);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOnBg);
        deserializeObject(data, pos, SyncHelper::instance().variables.gridToMapOnFg);
        deserializeObject(data, pos, SyncHelper::instance().variables.stereoscopicMode);
        deserializeObject(data, pos, SyncHelper::instance().variables.stereoscopicModeBg);
        deserializeObject(data, pos, SyncHelper::instance().variables.stereoscopicModeFg);
        deserializeObject(data, pos, SyncHelper::instance().variables.eofMode);
        deserializeObject(data, pos, SyncHelper::instance().variables.viewMode);
        deserializeObject(data, pos, SyncHelper::instance().variables.radius);
        deserializeObject(data, pos, SyncHelper::instance().variables.fov);
        deserializeObject(data, pos, SyncHelper::instance().variables.angle);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateX);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateY);
        deserializeObject(data, pos, SyncHelper::instance().variables.rotateZ);
        deserializeObject(data, pos, SyncHelper::instance().variables.translateX);
        deserializeObject(data, pos, SyncHelper::instance().variables.translateY);
        deserializeObject(data, pos, SyncHelper::instance().variables.translateZ);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeWidth);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeHeight);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeElevation);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeDistance);
        deserializeObject(data, pos, SyncHelper::instance().variables.planeConsiderAspectRatio);

        // Eq
        deserializeObject(data, pos, SyncHelper::instance().variables.eqDirty);
        if (SyncHelper::instance().variables.eqDirty) {
            deserializeObject(data, pos, SyncHelper::instance().variables.eqContrast);
            deserializeObject(data, pos, SyncHelper::instance().variables.eqBrightness);
            deserializeObject(data, pos, SyncHelper::instance().variables.eqGamma);
            deserializeObject(data, pos, SyncHelper::instance().variables.eqSaturation);
        }

        // Looptime
        deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeDirty);
        if (SyncHelper::instance().variables.loopTimeDirty) {
            deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeEnabled);
            deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeA);
            deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeB);
        }

        // Window features
        deserializeObject(data, pos, SyncHelper::instance().variables.windowOnTop);
        deserializeObject(data, pos, SyncHelper::instance().variables.windowOpacity);

        // Speed
        deserializeObject(data, pos, SyncHelper::instance().variables.speedDirty);
        if (SyncHelper::instance().variables.speedDirty) {
            deserializeObject(data, pos, SyncHelper::instance().variables.playbackSpeed);
        }

        // Strings
        int transferedImageId = -1;
        deserializeObject(data, pos, transferedImageId);

        if (transferedImageId == 0) {
            deserializeObject(data, pos, SyncHelper::instance().variables.loadFile);
            deserializeObject(data, pos, SyncHelper::instance().variables.loadedFile);
        } else if (transferedImageId == 1) {
            deserializeObject(data, pos, SyncHelper::instance().variables.overlayFileDirty);
            deserializeObject(data, pos, SyncHelper::instance().variables.overlayFile);
        } else if (transferedImageId == 2) {
            deserializeObject(data, pos, SyncHelper::instance().variables.bgImageFileDirty);
            deserializeObject(data, pos, SyncHelper::instance().variables.bgImageFile);
        } else if (transferedImageId == 3) {
            deserializeObject(data, pos, SyncHelper::instance().variables.fgImageFileDirty);
            deserializeObject(data, pos, SyncHelper::instance().variables.fgImageFile);
        }

        // Subtitle sync
        bool subTitleIsSyncing = false;
        deserializeObject(data, pos, subTitleIsSyncing);
        if (subTitleIsSyncing && mainSubtitleLayer) {
            bool subTitleFullSync = false;
            deserializeObject(data, pos, subTitleFullSync);
            if (subTitleFullSync) {
                mainSubtitleLayer->decodeFull(data, pos);
            }
            else {
                mainSubtitleLayer->decodeAlways(data, pos);
            }
        }

        // Layers
        bool layerSync = false;
        deserializeObject(data, pos, layerSync);
        if (layerSync) {
            deserializeObject(data, pos, preLoadLayers);
            int numLayers = -1;
            deserializeObject(data, pos, numLayers);
            uint32_t id;
            updateLayers = true;
            for (int i = 0; i < numLayers; i++) {
                deserializeObject(data, pos, id);
                deserializeObject(data, pos, layerSync);
                int layerType = -1;
                deserializeObject(data, pos, layerType);
                // Check if already updated this layer before a draw has been made
                auto it_up = find_if(secondaryLayersToKeep.begin(), secondaryLayersToKeep.end(),
                                     [&id](const std::shared_ptr<BaseLayer>&t1) { return (t1 ? t1->identifier() == id : false); });
                if (it_up == secondaryLayersToKeep.end()) {
                    // Find if layer exists in all previously created layers
                    auto it = find_if(secondaryLayers.begin(), secondaryLayers.end(),
                                      [&id](const std::shared_ptr<BaseLayer>&t1) { return (t1 ? t1->identifier() == id : false); });
                    if (it != secondaryLayers.end()) { // If exist, add to new pos and remove from old container
                        // If exist, sync if needed
                        if (layerSync) {
                            (*it)->decodeFull(data, pos);
                        } else {
                            (*it)->decodeAlways(data, pos);
                        }
                        secondaryLayersToKeep.push_back(*it);
                    } else if (layerSync) { // Did not exist. Let's create it
                        BaseLayer *newLayer = BaseLayer::createLayer(false, layerType, get_proc_address_glfw_v1, get_proc_address_glfw_v2, std::to_string(id), id);
                        if (newLayer) {
                            if (layerSync) {
                                newLayer->decodeFull(data, pos);
                            } else {
                                newLayer->decodeAlways(data, pos);
                            }
                            secondaryLayersToKeep.push_back(std::shared_ptr<BaseLayer>(newLayer));
                        }
                    }
                }
                else {
                    if (layerSync) {
                        (*it_up)->decodeFull(data, pos);
                    }
                    else {
                        (*it_up)->decodeAlways(data, pos);
                    }
                }
            }
        }
    }
}

static void postSyncPreDraw() {
#ifndef SGCT_ONLY
    // Apply synced commands
    if (!Engine::instance().isMaster()) {

        // Delete layers left in old container, and update to new
        // Needs to be done in this function, not in the deserialization.
        if (updateLayers) {
            auto it = secondaryLayers.begin();
            for (; it != secondaryLayers.end();) {
                if (std::find(secondaryLayersToKeep.begin(), secondaryLayersToKeep.end(), (*it)) == secondaryLayersToKeep.end()) {
                    it = secondaryLayers.erase(it);
                } else {
                    std::shared_ptr<BaseLayer> layer = (*it);
                    if (layer->needSync()) {
                        if (layer->gridMode() == BaseLayer::GridMode::Plane) {
                            layer->updatePlane();
                        }
                        layer->setHasSynced();
                    }
                    ++it;
                }
            }
            secondaryLayers = secondaryLayersToKeep;
            secondaryLayersToKeep.clear();
            updateLayers = false;
        }

        glm::vec3 rotXYZ = glm::vec3(float(SyncHelper::instance().variables.rotateX),
                                     float(SyncHelper::instance().variables.rotateY),
                                     float(SyncHelper::instance().variables.rotateZ));

        glm::vec3 translateXYZ = glm::vec3(float(SyncHelper::instance().variables.translateX) / 100.f,
                                           float(SyncHelper::instance().variables.translateY) / 100.f,
                                           float(SyncHelper::instance().variables.translateZ) / 100.f);

        bool newImage = false;

        // Process background image loading
        newImage = backgroundImageLayer->processImageUpload(SyncHelper::instance().variables.bgImageFile, SyncHelper::instance().variables.bgImageFileDirty);
        if (newImage) {
            backgroundImageLayer->setRotate(rotXYZ);
            backgroundImageLayer->setTranslate(translateXYZ);
        }
        SyncHelper::instance().variables.bgImageFileDirty = false;

        // Process foreground image loading
        newImage = foregroundImageLayer->processImageUpload(SyncHelper::instance().variables.fgImageFile, SyncHelper::instance().variables.fgImageFileDirty);
        if (newImage) {
            foregroundImageLayer->setRotate(rotXYZ);
            foregroundImageLayer->setTranslate(translateXYZ);
        }
        SyncHelper::instance().variables.fgImageFileDirty = false;

        // Process overlay image loading
        newImage = overlayImageLayer->processImageUpload(SyncHelper::instance().variables.overlayFile, SyncHelper::instance().variables.overlayFileDirty);
        SyncHelper::instance().variables.overlayFileDirty = false;

        if (!SyncHelper::instance().variables.loadedFile.empty()) {
            // Load new MPV file
            mainVideoLayer->loadFile(SyncHelper::instance().variables.loadedFile, SyncHelper::instance().variables.loadFile);
            SyncHelper::instance().variables.loadFile = false;
        }

        layerRender->clearLayers();

        // Background image layer
        if ((!mainVideoLayer->renderingIsOn() || !mainVideoLayer->ready() ||
             SyncHelper::instance().variables.alpha < 1.f || SyncHelper::instance().variables.gridToMapOn == 1) &&
            backgroundImageLayer->ready() && SyncHelper::instance().variables.alphaBg > 0.f) {
            backgroundImageLayer->setAlpha(SyncHelper::instance().variables.alphaBg);
            backgroundImageLayer->setGridMode(static_cast<uint8_t>(SyncHelper::instance().variables.gridToMapOnBg));
            backgroundImageLayer->setStereoMode(static_cast<uint8_t>(SyncHelper::instance().variables.stereoscopicModeBg));
            layerRender->addLayer(backgroundImageLayer);
        }

        // Custom layers with hierarchy BACK
        // Should stop if mainVideoLayer is full visible
        // Rendered top to bottom, so need to add them the other way around...
        for (auto it = secondaryLayers.rbegin(); it != secondaryLayers.rend(); ++it) {
            std::shared_ptr<BaseLayer> layer = (*it);
            if (layer->hierarchy() == BaseLayer::LayerHierarchy::BACK) {
                if (layer->shouldUpdate()) {
                    if (!layer->hasInitialized()) {
                        layer->initialize();
                    }
                    if (!layer->ready()) {
                        layer->update(false);
                    }
                    else if (layer->ready() && (layer->alpha() > 0.f) && SyncHelper::instance().variables.alpha < 1.f) {
                        layer->update();
                        layer->setTranslate(translateXYZ);
                        layerRender->addLayer(layer);
                    }
                }
                else if ((preLoadLayers || layer->shouldPreLoad()) && !layer->ready()) {
                    if (!layer->hasInitialized()) {
                        layer->initialize();
                    }
                    layer->update(false);
                }
            }
        }

        // Main video/media layer
        if (mainVideoLayer->renderingIsOn()) {
            if (mainVideoLayer->ready() && SyncHelper::instance().variables.alpha > 0.f) {                
                mainVideoLayer->setAlpha(SyncHelper::instance().variables.alpha);
                mainVideoLayer->setGridMode(static_cast<uint8_t>(SyncHelper::instance().variables.gridToMapOn));
                mainVideoLayer->setStereoMode(static_cast<uint8_t>(SyncHelper::instance().variables.stereoscopicMode));
                mainVideoLayer->setRotate(rotXYZ);
                mainVideoLayer->setTranslate(translateXYZ);
                layerRender->addLayer(mainVideoLayer);

                //Add subtitle if exists
                if (mainSubtitleLayer->hasText()) {
                    layerRender->addLayer(mainSubtitleLayer);
                    mainSubtitleLayer->update(true);
                }
            }

            if (overlayImageLayer->ready() && SyncHelper::instance().variables.alpha > 0.f) {
                overlayImageLayer->setAlpha(SyncHelper::instance().variables.alpha);
                overlayImageLayer->setGridMode(static_cast<uint8_t>(SyncHelper::instance().variables.gridToMapOn));
                overlayImageLayer->setStereoMode(static_cast<uint8_t>(SyncHelper::instance().variables.stereoscopicMode));
                overlayImageLayer->setRotate(rotXYZ);
                overlayImageLayer->setTranslate(translateXYZ);
                layerRender->addLayer(overlayImageLayer);
            }

            // If we have 2D and 3D viewports defined, deside based on renderParams which to render
            // 1. Check what stereo mode we should choose
            // 2. For each window, check if there is a mix of viewports (2D and 3D). If not mix, skip step 3 (for that window).
            // 3. Enable/disable viewport based on defined stereo mode

            // Step 1
            bool show2Dcontent = false;
            bool show3Dcontent = false;
            bool has3Dplane = false;
            for (const auto &layer : layerRender->getLayers()) {
                if (layer->stereoMode() > 0) {
                    show3Dcontent = true;
                    if (layer->gridMode() == 1) {
                        has3Dplane = true;
                    }
                } else {
                    show2Dcontent = true;
                }
            }
            // If we have one 3D renderParam visible, it takes president
            if (show3Dcontent) {
                show2Dcontent = false;
            } else { // If not 2D or 3D, still enable 2D viewports
                show2Dcontent = true;
            }

            // If viewMode==1, that means the user has asked to force all content to 2D
            if (SyncHelper::instance().variables.viewMode == 1) {
                show2Dcontent = true;
                show3Dcontent = false;
            }
            
            for (const std::unique_ptr<Window> &win : Engine::instance().thisNode().windows()) {
                bool exist2Dviewports = false;
                bool exist3Dviewports = false;

                // Set window features
                if (glfwGetWindowOpacity(win->windowHandle()) != SyncHelper::instance().variables.windowOpacity) {
                    glfwSetWindowOpacity(win->windowHandle(), SyncHelper::instance().variables.windowOpacity);
                }
                int currentWinOnTop = glfwGetWindowAttrib(win->windowHandle(), GLFW_FLOATING);
                int newWinOnTop = (SyncHelper::instance().variables.windowOnTop ? 1 : 0);
                if (newWinOnTop != currentWinOnTop) {
                    glfwSetWindowAttrib(win->windowHandle(), GLFW_FLOATING, newWinOnTop);
                }

                // Step 2
                for (const std::unique_ptr<Viewport> &vp : win->viewports()) {
                    if (vp->eye() == FrustumMode::Mono) {
                        exist2Dviewports = true;
                    } else if (vp->eye() == FrustumMode::StereoLeft || vp->eye() == FrustumMode::StereoRight) {
                        exist3Dviewports = true;
                    }
                }
                // Step 3
                if (exist2Dviewports && exist3Dviewports) {
                    for (const std::unique_ptr<Viewport> &vp : win->viewports()) {
                        if (show2Dcontent && (vp->eye() == FrustumMode::Mono)) {
                            vp->setEnabled(true);
                        } else if (show3Dcontent && (vp->eye() == FrustumMode::StereoLeft || vp->eye() == FrustumMode::StereoRight)) {
                            vp->setEnabled(true);
                        } else {
                            vp->setEnabled(false);
                        }
                    }
                }
            }
        }

        // Custom layers with hierarchy FRONT
        // Rendered top to bottom, so need to add them the other way around...
        for (auto it = secondaryLayers.rbegin(); it != secondaryLayers.rend(); ++it) {
            std::shared_ptr<BaseLayer> layer = (*it);
            if (layer->hierarchy() == BaseLayer::LayerHierarchy::FRONT) {
                if (layer->shouldUpdate()) {
                    if (!layer->hasInitialized()) {
                        layer->initialize();
                    }
                    if (!layer->ready()) {
                        layer->update(false);
                    }
                    else if (layer->ready() && (layer->alpha() > 0.f)) {
                        layer->update();
                        layer->setTranslate(translateXYZ);
                        layerRender->addLayer(layer);
                    }
                }
                else if ((preLoadLayers || layer->shouldPreLoad()) && !layer->ready()) {
                    if (!layer->hasInitialized()) {
                        layer->initialize();
                    }
                    layer->update(false);
                }
            }
        }

        // Foreground image layer
        if (foregroundImageLayer->ready() && SyncHelper::instance().variables.alphaFg > 0.f) {
            foregroundImageLayer->setAlpha(SyncHelper::instance().variables.alphaFg);
            foregroundImageLayer->setGridMode(static_cast<uint8_t>(SyncHelper::instance().variables.gridToMapOnFg));
            foregroundImageLayer->setStereoMode(static_cast<uint8_t>(SyncHelper::instance().variables.stereoscopicModeFg));
            layerRender->addLayer(foregroundImageLayer);
        }

        // Set properties of main mpv layer
        mainVideoLayer->setTimePause(SyncHelper::instance().variables.paused);
        mainVideoLayer->setEOFMode(SyncHelper::instance().variables.eofMode);
        mainVideoLayer->setTimePosition(
            SyncHelper::instance().variables.timePosition,
            SyncHelper::instance().variables.timeDirty);
        if (SyncHelper::instance().variables.loopTimeDirty) {
            mainVideoLayer->setLoopTime(
                SyncHelper::instance().variables.loopTimeA,
                SyncHelper::instance().variables.loopTimeB,
                SyncHelper::instance().variables.loopTimeEnabled);
        }
        if (SyncHelper::instance().variables.eqDirty) {
            mainVideoLayer->setValue("contrast", SyncHelper::instance().variables.eqContrast);
            mainVideoLayer->setValue("brightness", SyncHelper::instance().variables.eqBrightness);
            mainVideoLayer->setValue("gamma", SyncHelper::instance().variables.eqGamma);
            mainVideoLayer->setValue("saturation", SyncHelper::instance().variables.eqSaturation);
        }
        if (SyncHelper::instance().variables.speedDirty) {
            mainVideoLayer->setValue("speed", SyncHelper::instance().variables.playbackSpeed);
        }

        // Set latest plane details for all primary layers
        glm::vec2 planeSize = glm::vec2(float(SyncHelper::instance().variables.planeWidth), float(SyncHelper::instance().variables.planeHeight));
        for (auto &layer : primaryLayers) {
            layer->setPlaneDistance(SyncHelper::instance().variables.planeDistance);
            layer->setPlaneElevation(SyncHelper::instance().variables.planeElevation);
            layer->setPlaneSize(planeSize, static_cast<uint8_t>(SyncHelper::instance().variables.planeConsiderAspectRatio));
        }
        // Only plane consideration for subtitle layer (as other plane parameters are set individually)
        mainSubtitleLayer->setPlaneSize(planeSize, static_cast<uint8_t>(SyncHelper::instance().variables.planeConsiderAspectRatio));

        // Set new general dome/sphere details
        layerRender->updateMeshes(SyncHelper::instance().variables.radius, SyncHelper::instance().variables.fov);
    }

    if (Engine::instance().isMaster()) {
        return;
    }
#endif

    // Update/render the frame from MPV pipeline
    mainVideoLayer->updateFrame();
}

static void draw(const RenderData &data) {
#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif
    glDisable(GL_DEPTH_TEST);
    glDepthMask(false);

    glDisable(GL_BLEND);

    // Render layers
    layerRender->renderLayers(data,
                              SyncHelper::instance().variables.viewMode,
                              float(SyncHelper::instance().variables.angle));

    glDisable(GL_BLEND);
}

static void cleanup() {
    if (!logFilePath.empty()) {
        logFile.close();
    }

#ifdef NDI_SUPPORT
    NdiFinder::destroy();
#endif

#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif

    secondaryLayers.clear();
    secondaryLayersToKeep.clear();
    backgroundImageLayer = nullptr;
    foregroundImageLayer = nullptr;
    overlayImageLayer = nullptr;
    mainVideoLayer = nullptr;
    mainSubtitleLayer = nullptr;
    layerRender = nullptr;
    primaryLayers.clear();
}

static void logging(Log::Level, std::string_view message) {
    std::lock_guard<std::mutex> lock(logMutex);
    logFile << message << std::endl;
}

int main(int argc, char *argv[]) {
    std::vector<std::string> arg(argv + 1, argv + argc);
    Configuration config = parseArguments(arg);
    config::Cluster cluster = loadCluster(config.configFilename);
    if (!cluster.success) {
        return -1;
    }

    // Look for C-Play command line specific things
    size_t i = 0;
    while (i < arg.size()) {
        if (arg[i] == "--mpvconf") {
            std::string mpvConfFolder = arg[i + 1]; // for instance, either "decoding_cpu" or "decoding_cpu"
            SyncHelper::instance().configuration.confAll = "./data/mpv-conf/" + mpvConfFolder + "/all.json";
            SyncHelper::instance().configuration.confMasterOnly = "./data/mpv-conf/" + mpvConfFolder + "/master-only.json";
            SyncHelper::instance().configuration.confNodesOnly = "./data/mpv-conf/" + mpvConfFolder + "/nodes-only.json";
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        } else if (arg[i] == "--allowDirectRendering") {
            allowDirectRendering = true;
            arg.erase(arg.begin() + i);
        } else if (arg[i] == "--loglevel") {
            // Valid log levels: error warn info debug
            std::string level = arg[i + 1];
            if (level == "error") {
                Log::instance().setNotifyLevel(Log::Level::Error);
                logLevel = level;
            } else if (level == "warn") {
                Log::instance().setNotifyLevel(Log::Level::Warning);
                logLevel = level;
            } else if (level == "info") {
                Log::instance().setNotifyLevel(Log::Level::Info);
                logLevel = level;
            } else if (level == "debug") {
                Log::instance().setNotifyLevel(Log::Level::Debug);
                logLevel = level;
            }
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        } else if (arg[i] == "--logfile") {
            std::string logFileName = arg[i + 1]; // for instance, either "log_master.txt" or "log_client.txt"
            logFilePath = "./data/log/" + logFileName;
            logFile.open(logFilePath, std::ofstream::out | std::ofstream::trunc);
            if (logLevel.empty()) { // Set log level to info if we specfied a log file
                Log::instance().setNotifyLevel(Log::Level::Info);
                logLevel = "info";
            }
            Log::instance().setShowLogLevel(true);
            Log::instance().setShowTime(true);
            Log::instance().setLogCallback(logging);
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        } else if (arg[i] == "--loadfile") {
            startupFile = arg[i + 1];
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        } else {
            i++;
        }
    }

    Engine::Callbacks callbacks;
    callbacks.initOpenGL = initOGL;
    callbacks.preSync = preSync;
    callbacks.encode = encode;
    callbacks.decode = decode;
    callbacks.postSyncPreDraw = postSyncPreDraw;
    callbacks.draw = draw;
    callbacks.cleanup = cleanup;
    try {
        Engine::create(cluster, callbacks, config);
    } catch (const std::runtime_error &e) {
        Log::Error(e.what());
        Engine::destroy();
        return EXIT_FAILURE;
    }

#ifdef MDK_SUPPORT
    //Unique for C-Play
    mdk::SetGlobalOption("MDK_KEY", "12BAA3C8BD1AEF74F0FAD1DFDE693AA49BCEB95A7E518F74D43C6A5A4D225882D44A101B825B82DA6F43EFCCD6D0B12148AC64B0DF4CCF37AF7720E1D743520FED455C3742E5108B0F052E202196C55B9BCEBF195301E315AD5789DF2AC4E297D3E645BAA12123255B87840EAC02FE103A3CFADFDCFFCE24BAE3B935C543520F");
#endif

#ifndef SGCT_ONLY
    if (Engine::instance().isMaster()) {
        if (!ClusterManager::instance().ignoreSync() || ClusterManager::instance().numberOfNodes() > 1) {
            if (!NetworkManager::instance().areAllNodesConnected()) {
                Engine::destroy();
                return EXIT_FAILURE;
            }
        }

        Log::Info("Start Master");

        // Hide window (as we are not using it on master)
        Engine::instance().thisNode().windows().at(0)->setRenderWhileHidden(true);
        Engine::instance().thisNode().windows().at(0)->setVisible(false);

        // Consider arguments not processed by C-Play or SGCT to be QApplication related
        std::vector<char*> qapp_argv;
        std::string app_path(argv[0]);
        qapp_argv.reserve(arg.size() + app_path.size());
        qapp_argv.push_back(argv[0]);
        size_t q = 0;
        while (q < arg.size()) {
            qapp_argv.push_back(const_cast<char*>(arg[q].c_str()));
            arg.erase(arg.begin() + q);
        }

        // Launch master application (which calls Engine::render from thread)
        int qapp_argv_size = static_cast<int>(qapp_argv.size());
        Application::create(qapp_argv_size, &qapp_argv[0], QStringLiteral("C-Play"));
        Application::instance().setStartupFile(startupFile);
        return Application::instance().run();
    } else {
#endif
        Log::Info("Start Client");

        Engine::instance().exec();
        Engine::destroy();
        return EXIT_SUCCESS;
#ifndef SGCT_ONLY
    }
#endif
}
