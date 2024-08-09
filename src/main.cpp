/*
 * SPDX-FileCopyrightText: 
 * 2021-2024 Erik Sundén <eriksunden85@gmail.com> 
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sgct/sgct.h>
#include <sgct/opengl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <layers/baselayer.h>
#include <layers/imagelayer.h>
#include <layers/mpvlayer.h>
#include <layerrenderer.h>
#include "application.h"
#include <fstream>   
#include <mutex>

//#define SGCT_ONLY

namespace {

bool allowDirectRendering = false;

std::mutex logMutex;
std::ofstream logFile;
std::string logFilePath = "";
std::string logLevel = "";
std::string startupFile = "";

std::vector<BaseLayer*> allLayers;
ImageLayer* backgroundImageLayer;
ImageLayer* foregroundImageLayer;
ImageLayer* overlayImageLayer;
MpvLayer* mainMpvLayer;

LayerRenderer* layerRender;

} // namespace

using namespace sgct;

void initOGL(GLFWwindow*) {
#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif

    // Create standard layers
    backgroundImageLayer = new ImageLayer("background");
    allLayers.push_back(backgroundImageLayer);

    mainMpvLayer = new MpvLayer(allowDirectRendering, !logFilePath.empty() || !logLevel.empty(), logLevel);
    mainMpvLayer->initialize();
    mainMpvLayer->loadFile(SyncHelper::instance().variables.loadedFile);
    allLayers.push_back(mainMpvLayer);

    overlayImageLayer = new ImageLayer("overlay");
    allLayers.push_back(overlayImageLayer);

    foregroundImageLayer = new ImageLayer("foreground");
    allLayers.push_back(foregroundImageLayer);

    layerRender = new LayerRenderer();
    layerRender->initialize(SyncHelper::instance().variables.radius, SyncHelper::instance().variables.fov);

    // Set up backface culling
    glCullFace(GL_BACK);
    // our polygon winding is clockwise since we are inside of the dome
    glFrontFace(GL_CW);
}

void preSync() {

}

std::vector<std::byte> encode() {
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

        //Eq
        serializeObject(data, SyncHelper::instance().variables.eqDirty);
        if (SyncHelper::instance().variables.eqDirty) {
            serializeObject(data, SyncHelper::instance().variables.eqContrast);
            serializeObject(data, SyncHelper::instance().variables.eqBrightness);
            serializeObject(data, SyncHelper::instance().variables.eqGamma);
            serializeObject(data, SyncHelper::instance().variables.eqSaturation);
        }

        //Looptime
        serializeObject(data, SyncHelper::instance().variables.loopTimeDirty);
        if (SyncHelper::instance().variables.loopTimeDirty) {
            serializeObject(data, SyncHelper::instance().variables.loopTimeEnabled);
            serializeObject(data, SyncHelper::instance().variables.loopTimeA);
            serializeObject(data, SyncHelper::instance().variables.loopTimeB);
        }

        // As strings can be quiet long.
        // Saving connection load, to only send one URL at a time.
        if (SyncHelper::instance().variables.loadFile) { // ID: 0 = mpv media file
            serializeObject(data, 0);
            serializeObject(data, SyncHelper::instance().variables.loadFile);
            serializeObject(data, SyncHelper::instance().variables.loadedFile);
            SyncHelper::instance().variables.loadFile = false;
        }
        else if (SyncHelper::instance().variables.overlayFileDirty) { // ID: 1 = overlay image file
            serializeObject(data, 1);
            serializeObject(data, SyncHelper::instance().variables.overlayFileDirty);
            serializeObject(data, SyncHelper::instance().variables.overlayFile);
            SyncHelper::instance().variables.overlayFileDirty = false;
        }
        else if (SyncHelper::instance().variables.bgImageFileDirty) { // ID: 2 = background image file
            serializeObject(data, 2);
            serializeObject(data, SyncHelper::instance().variables.bgImageFileDirty);
            serializeObject(data, SyncHelper::instance().variables.bgImageFile);
            SyncHelper::instance().variables.bgImageFileDirty = false;
        }
        else if (SyncHelper::instance().variables.fgImageFileDirty) { // ID: 3 = foreground image file
            serializeObject(data, 3);
            serializeObject(data, SyncHelper::instance().variables.fgImageFileDirty);
            serializeObject(data, SyncHelper::instance().variables.fgImageFile);
            SyncHelper::instance().variables.fgImageFileDirty = false;
        }
        else { // Sending no URL
            serializeObject(data, -1);
        }

        //Reset flags every frame cycle
        SyncHelper::instance().variables.timeDirty = false;
        SyncHelper::instance().variables.eqDirty = false;
        SyncHelper::instance().variables.loopTimeDirty = false;
    }

    return data;
}

void decode(const std::vector<std::byte>& data) {
    unsigned pos = 0;
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

        //Eq
        deserializeObject(data, pos, SyncHelper::instance().variables.eqDirty);
        if (SyncHelper::instance().variables.eqDirty) {
            deserializeObject(data, pos, SyncHelper::instance().variables.eqContrast);
            deserializeObject(data, pos, SyncHelper::instance().variables.eqBrightness);
            deserializeObject(data, pos, SyncHelper::instance().variables.eqGamma);
            deserializeObject(data, pos, SyncHelper::instance().variables.eqSaturation);
        }

        //Looptime
        deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeDirty);
        if (SyncHelper::instance().variables.loopTimeDirty) {
            deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeEnabled);
            deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeA);
            deserializeObject(data, pos, SyncHelper::instance().variables.loopTimeB);
        }

        //Strings
        int transferedImageId = -1;
        deserializeObject(data, pos, transferedImageId);

        if (transferedImageId == 0) {
            deserializeObject(data, pos, SyncHelper::instance().variables.loadFile);
            deserializeObject(data, pos, SyncHelper::instance().variables.loadedFile);
        }
        else if (transferedImageId == 1) {
            deserializeObject(data, pos, SyncHelper::instance().variables.overlayFileDirty);
            deserializeObject(data, pos, SyncHelper::instance().variables.overlayFile);
        }
        else if (transferedImageId == 2) {
            deserializeObject(data, pos, SyncHelper::instance().variables.bgImageFileDirty);
            deserializeObject(data, pos, SyncHelper::instance().variables.bgImageFile);
        }
        else if (transferedImageId == 3) {
            deserializeObject(data, pos, SyncHelper::instance().variables.fgImageFileDirty);
            deserializeObject(data, pos, SyncHelper::instance().variables.fgImageFile);
        }
    }
}

void postSyncPreDraw() {
#ifndef SGCT_ONLY
    //Apply synced commands
    if (!Engine::instance().isMaster()) {

        glm::vec3 rotXYZ = glm::vec3(float(SyncHelper::instance().variables.rotateX),
            float(SyncHelper::instance().variables.rotateY),
            float(SyncHelper::instance().variables.rotateZ));

        glm::vec3 translateXYZ = glm::vec3(float(SyncHelper::instance().variables.translateX) / 100.f,
            float(SyncHelper::instance().variables.translateY) / 100.f,
            float(SyncHelper::instance().variables.translateZ) / 100.f);

        bool newImage = false;

        //Process background image loading
        newImage = backgroundImageLayer->processImageUpload(SyncHelper::instance().variables.bgImageFile, SyncHelper::instance().variables.bgImageFileDirty);
        if (newImage) {
            backgroundImageLayer->setRotate(rotXYZ);
            backgroundImageLayer->setTranslate(translateXYZ);
        }
        SyncHelper::instance().variables.bgImageFileDirty = false;

        //Process foreground image loading
        newImage = foregroundImageLayer->processImageUpload(SyncHelper::instance().variables.fgImageFile, SyncHelper::instance().variables.fgImageFileDirty);
        if (newImage) {
            foregroundImageLayer->setRotate(rotXYZ);
            foregroundImageLayer->setTranslate(translateXYZ);
        }
        SyncHelper::instance().variables.fgImageFileDirty = false;

        //Process overlay image loading
        newImage = overlayImageLayer->processImageUpload(SyncHelper::instance().variables.overlayFile, SyncHelper::instance().variables.overlayFileDirty);
        SyncHelper::instance().variables.overlayFileDirty = false;

        if (!SyncHelper::instance().variables.loadedFile.empty()) {            
            //Load new MPV file
            mainMpvLayer->loadFile(SyncHelper::instance().variables.loadedFile, SyncHelper::instance().variables.loadFile);
            SyncHelper::instance().variables.loadFile = false;
        }

        layerRender->clearLayers();

        if ((!mainMpvLayer->renderingIsOn() || mainMpvLayer->hasLoadedFile() ||
            (SyncHelper::instance().variables.alpha < 1.f || SyncHelper::instance().variables.gridToMapOn == 1))
            && !backgroundImageLayer->hasLoadedFile() && SyncHelper::instance().variables.alphaBg > 0.f) {
            backgroundImageLayer->setAlpha(SyncHelper::instance().variables.alphaBg);
            backgroundImageLayer->setGridMode(SyncHelper::instance().variables.gridToMapOnBg);
            backgroundImageLayer->setStereoMode(SyncHelper::instance().variables.stereoscopicModeBg);
            layerRender->addLayer(backgroundImageLayer);
        }

        if (mainMpvLayer->renderingIsOn()) {
            if (!mainMpvLayer->hasLoadedFile() && SyncHelper::instance().variables.alpha > 0.f) {
                mainMpvLayer->setAlpha(SyncHelper::instance().variables.alpha);
                mainMpvLayer->setGridMode(SyncHelper::instance().variables.gridToMapOn);
                mainMpvLayer->setStereoMode(SyncHelper::instance().variables.stereoscopicMode);
                mainMpvLayer->setRotate(rotXYZ);
                mainMpvLayer->setTranslate(translateXYZ);
                layerRender->addLayer(mainMpvLayer);
            }

            if (!overlayImageLayer->hasLoadedFile() && SyncHelper::instance().variables.alpha > 0.f) {
                overlayImageLayer->setAlpha(SyncHelper::instance().variables.alpha);
                overlayImageLayer->setGridMode(SyncHelper::instance().variables.gridToMapOn);
                overlayImageLayer->setStereoMode(SyncHelper::instance().variables.stereoscopicMode);
                overlayImageLayer->setRotate(rotXYZ);
                overlayImageLayer->setTranslate(translateXYZ);
                layerRender->addLayer(overlayImageLayer);
            }

            //If we have 2D and 3D viewports defined, deside based on renderParams which to render
            //1. Check what stereo mode we should choose
            //2. For each window, check if there is a mix of viewports (2D and 3D). If not mix, skip step 3 (for that window).
            //3. Enable/disable viewport based on defined stereo mode

            //Step 1
            bool show2Dcontent = false;
            bool show3Dcontent = false;
            bool has3Dplane = false;
            for (const auto& layer : layerRender->getLayers()) {
                if (layer->stereoMode() > 0) {
                    show3Dcontent = true;
                    if (layer->gridMode() == 1) {
                        has3Dplane = true;
                    }
                }
                else {
                    show2Dcontent = true;
                }
            }
            //If we have one 3D renderParam visible, it takes president
            if (show3Dcontent) {
                show2Dcontent = false;
            }
            else { //If not 2D or 3D, still enable 2D viewports
                show2Dcontent = true;
            }

            //If viewMode==1, that means the user has asked to force all content to 2D
            if (SyncHelper::instance().variables.viewMode == 1) {
                show2Dcontent = true;
                show3Dcontent = false;
            }

            for (const std::unique_ptr<Window>& win : Engine::instance().thisNode().windows()) {
                bool exist2Dviewports = false;
                bool exist3Dviewports = false;
                // Step 2
                for (const std::unique_ptr<Viewport>& vp : win->viewports()) {
                    if (vp->eye() == Frustum::Mode::MonoEye) {
                        exist2Dviewports = true;
                    }
                    else if (vp->eye() == Frustum::Mode::StereoLeftEye || vp->eye() == Frustum::Mode::StereoRightEye) {
                        exist3Dviewports = true;
                    }
                }
                // Step 3
                if (exist2Dviewports && exist3Dviewports) {
                    for (const std::unique_ptr<Viewport>& vp : win->viewports()) {
                        if (show2Dcontent && (vp->eye() == Frustum::Mode::MonoEye)) {
                            vp->setEnabled(true);
                        }
                        else if (show3Dcontent && (vp->eye() == Frustum::Mode::StereoLeftEye || vp->eye() == Frustum::Mode::StereoRightEye)) {
                            vp->setEnabled(true);
                        }
                        else {
                            vp->setEnabled(false);
                        }
                    }
                }
            }
            
        }

        if (!foregroundImageLayer->hasLoadedFile() && SyncHelper::instance().variables.alphaFg > 0.f) {
            foregroundImageLayer->setAlpha(SyncHelper::instance().variables.alphaFg);
            foregroundImageLayer->setGridMode(SyncHelper::instance().variables.gridToMapOnFg);
            foregroundImageLayer->setStereoMode(SyncHelper::instance().variables.stereoscopicModeFg);
            layerRender->addLayer(foregroundImageLayer);
        }
        
        // Set properties of main mpv layer
        mainMpvLayer->setPause(SyncHelper::instance().variables.paused);
        mainMpvLayer->setEOFMode(SyncHelper::instance().variables.eofMode);
        mainMpvLayer->setTimePosition(
            SyncHelper::instance().variables.timePosition, 
            SyncHelper::instance().variables.timeDirty);
        if (SyncHelper::instance().variables.loopTimeDirty) {
            mainMpvLayer->setLoopTime(
                SyncHelper::instance().variables.loopTimeA, 
                SyncHelper::instance().variables.loopTimeB, 
                SyncHelper::instance().variables.loopTimeEnabled);
        }
        if (SyncHelper::instance().variables.eqDirty) {
            mainMpvLayer->setValue("contrast", SyncHelper::instance().variables.eqContrast);
            mainMpvLayer->setValue("brightness", SyncHelper::instance().variables.eqBrightness);
            mainMpvLayer->setValue("gamma", SyncHelper::instance().variables.eqGamma);
            mainMpvLayer->setValue("saturation", SyncHelper::instance().variables.eqSaturation);
        }

        // Set latest plane details for all layers
        glm::vec2 planeSize = glm::vec2(float(SyncHelper::instance().variables.planeWidth), float(SyncHelper::instance().variables.planeHeight));
        for (const auto& layer : allLayers) {
            layer->setPlaneDistance(SyncHelper::instance().variables.planeDistance);
            layer->setPlaneElevation(SyncHelper::instance().variables.planeElevation);
            layer->setPlaneSize(planeSize, SyncHelper::instance().variables.planeConsiderAspectRatio);
        }

        // Set new general dome/sphere details
        layerRender->updateMeshes(SyncHelper::instance().variables.radius, SyncHelper::instance().variables.fov);
    }

    if (Engine::instance().isMaster()) {
        return;
    }
#endif

    // Update/render the frame from MPV pipeline
    mainMpvLayer->updateFrame();
}

void draw(const RenderData& data) {
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

void cleanup() {
    if (!logFilePath.empty()) {
        logFile.close();
    }

#ifndef SGCT_ONLY
    if (Engine::instance().isMaster())
        return;
#endif

    //Cleanup mainMpvLayer
    mainMpvLayer->cleanup();
    delete mainMpvLayer;
    mainMpvLayer = nullptr;

    delete backgroundImageLayer;
    delete foregroundImageLayer;
    delete overlayImageLayer;

    delete layerRender;
}

void logging(Log::Level, std::string_view message) {
    std::lock_guard<std::mutex> lock(logMutex);
    logFile << message << std::endl;
}

int main(int argc, char *argv[])
{
    std::vector<std::string> arg(argv + 1, argv + argc);
    Configuration config = parseArguments(arg);
    config::Cluster cluster = loadCluster(config.configFilename);
    if (!cluster.success) {
        return -1;
    }

    //Look for C-Play command line specific things
    size_t i = 0;
    while (i < arg.size()) {
        if (arg[i] == "--mpvconf") {
            std::string mpvConfFolder = arg[i + 1]; // for instance, either "decoding_cpu" or "decoding_cpu"
            SyncHelper::instance().configuration.confAll = "./data/mpv-conf/" + mpvConfFolder + "/all.json";
            SyncHelper::instance().configuration.confMasterOnly = "./data/mpv-conf/" + mpvConfFolder + "/master-only.json";
            SyncHelper::instance().configuration.confNodesOnly = "./data/mpv-conf/" + mpvConfFolder + "/nodes-only.json";
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        }
        else if (arg[i] == "--allowDirectRendering") {
            allowDirectRendering = true;
            arg.erase(arg.begin() + i);
        }
        else if (arg[i] == "--loglevel") {
            //Valid log levels: error warn info debug
            std::string level = arg[i + 1];
            if(level == "error") {
                Log::instance().setNotifyLevel(Log::Level::Error);
                logLevel = level;
            }
            else if (level == "warn") {
                Log::instance().setNotifyLevel(Log::Level::Warning);
                logLevel = level;
            }
            else if (level == "info") {
                Log::instance().setNotifyLevel(Log::Level::Info);
                logLevel = level;
            }
            else if (level == "debug") {
                Log::instance().setNotifyLevel(Log::Level::Debug);
                logLevel = level;
            }
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        }
        else if (arg[i] == "--logfile") {
            std::string logFileName = arg[i + 1]; // for instance, either "log_master.txt" or "log_client.txt"
            logFilePath = "./data/log/" + logFileName;
            logFile.open(logFilePath, std::ofstream::out | std::ofstream::trunc);
            if (logLevel.empty()) { //Set log level to info if we specfied a log file
                Log::instance().setNotifyLevel(Log::Level::Info);
                logLevel = "info";
            }
            Log::instance().setShowLogLevel(true);
            Log::instance().setShowTime(true);
            Log::instance().setLogCallback(logging);
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        }
        else if (arg[i] == "--loadfile") {
            startupFile = arg[i + 1];
            arg.erase(arg.begin() + i, arg.begin() + i + 2);
        }
        else {
            // Ignore unknown commands
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
    }
    catch (const std::runtime_error& e) {
        Log::Error(e.what());
        Engine::destroy();
        return EXIT_FAILURE;
    }

#ifndef SGCT_ONLY
    if (Engine::instance().isMaster()) {
        if(!ClusterManager::instance().ignoreSync() || ClusterManager::instance().numberOfNodes() > 1) {
            if(!NetworkManager::instance().areAllNodesConnected()) {
                Engine::destroy();
                return EXIT_FAILURE;
            }
        }

        Log::Info("Start Master");

        //Hide window (as we are not using it on master)
        Engine::instance().thisNode().windows().at(0)->setRenderWhileHidden(true);
        Engine::instance().thisNode().windows().at(0)->setVisible(false);

        //Do not support arguments to QApp, only SGCT
        std::vector<char*> cargv;
        cargv.push_back(argv[0]);
        int cargv_size = static_cast<int>(cargv.size());

        //Launch master application (which calls Engine::render from thread)
        Application::create(cargv_size, &cargv[0], QStringLiteral("C-Play"));
        Application::instance().setStartupFile(startupFile);
        return Application::instance().run();
    }
    else{
#endif
        Log::Info("Start Client");

        Engine::instance().exec();
        Engine::destroy();
        return EXIT_SUCCESS;
#ifndef SGCT_ONLY
    }
#endif

}

