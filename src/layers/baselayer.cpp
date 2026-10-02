/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "baselayer.h"
#include <layers/imagelayer.h>
#ifdef VIDEO_LAYER
#include <layers/videolayer.h>
#endif
#ifdef AUDIO_LAYER
#include <layers/audiolayer.h>
#endif
#include <sgct/opengl.h>
#include <sgct/shareddata.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#ifdef AUDIO_LAYER
#include "audiosettings.h"
#endif
#ifdef NETWORK_SYNC_SETTINGS
#include "presentationsettings.h"
#endif
#if defined(MDK_SUPPORT) && defined(VIDEO_LAYER)
#include <layers/adaptivevideolayer.h>
#endif
#if defined(NDI_LAYER)
#include <ndi/ndilayer.h>
#endif
#if defined(PDF_LAYER)
#include <layers/pdflayer.h>
#endif
#ifdef STREAM_LAYER
#include <layers/streamlayer.h>
#endif
#ifdef YOUTUBE_LAYER
#include <layers/youtubelayer.h>
#endif
#ifdef MULTI_VIDEO_LAYER
#include <layers/multivideolayer.h>
#endif
#if defined(SPOUT_LAYER)
#include <layers/spoutlayer.h>
#endif
#if defined(DIRECTSHOW_LAYER)
#include <layers/directshowlayer.h>
#endif
#if defined(OMT_LAYER)
#include <omt/omtlayer.h>
#endif
#ifdef TEXT_LAYER
#include <layers/textlayer.h>
#endif
#ifdef CONTROL_LAYER
#include <layers/controllayer.h>
#endif
#ifdef REST_LAYER
#include <layers/restlayer.h>
#endif
#ifdef WEBRTC_LAYER
#include <webrtc/webrtclayer.h>
#endif
// ndisender.h is always compiled, it degrades to a no-op implementation when
// the build has no NDI support. The complete type is needed here regardless,
// since BaseLayer holds a std::unique_ptr<NdiSender>.
#include <ndi/ndisender.h>
#ifdef NODE_STREAM_SUPPORT
#include <nodestream/nodestreamlayer.h>
#include <nodestream/nodestreamsender.h>
#include <nodestream/nodestreamsocket.h>
#include <sgct/clustermanager.h>
#endif

std::atomic_uint32_t BaseLayer::m_id_gen = 1;

std::string BaseLayer::typeDescription(BaseLayer::LayerType e) {
    switch (e) {
    case BASE:
        return "Base";
#ifdef IMAGE_LAYER
    case IMAGE:
        return "Image";
#endif
#ifdef VIDEO_LAYER
    case VIDEO:
        return "Video";
#endif
#ifdef MULTI_VIDEO_LAYER
    case MULTIVIDEO:
        return "MultiVideo";
#endif
#ifdef AUDIO_LAYER
    case AUDIO:
        return "Audio";
#endif
#ifdef PDF_LAYER
    case PDF:
        return "PDF";
#endif
#ifdef NDI_LAYER
    case NDI:
        return "NDI";
#endif
#ifdef OMT_LAYER
    case OMT:
        return "OMT";
#endif
#ifdef DIRECTSHOW_LAYER
    case DIRECTSHOW:
        return "DirectShow";
#endif
#ifdef SPOUT_LAYER
    case SPOUT:
        return "Spout";
#endif
#ifdef STREAM_LAYER
    case STREAM:
        return "Stream";
#endif
#ifdef YOUTUBE_LAYER
    case YOUTUBE:
        return "YouTube";
#endif
#ifdef WEBRTC_LAYER
    case WEBRTC:
        return "WebRTC";
#endif
#ifdef TEXT_LAYER
    case TEXT:
        return "Text";
#endif
#ifdef CONTROL_LAYER
    case CONTROL:
        return "Control";
#endif
#ifdef REST_LAYER
    case REST:
        return "REST";
#endif
#ifdef NODE_STREAM_SUPPORT
    case NODESTREAM:
        return "NodeStream";
#endif
    default:
        return "";
    }
}

#ifdef MDK_SUPPORT
#define FUNC_V2 gl_adress_func_v2 opa2
#else
#define FUNC_V2 gl_adress_func_v2
#endif

#ifdef VIDEO_LAYER
#define FUNC_V1 gl_adress_func_v1 opa1
#else
#define FUNC_V1 gl_adress_func_v1
#endif

BaseLayer *BaseLayer::createLayer(bool isMaster, int layerType, FUNC_V1, FUNC_V2, std::string strId, uint32_t numID) {
    BaseLayer *newLayer = nullptr;
    switch (layerType) {
#ifdef IMAGE_LAYER
    case static_cast<int>(BaseLayer::LayerType::IMAGE): {
        ImageLayer *newImg = new ImageLayer(strId);
        newLayer = newImg;
        break;
    }
#endif
#ifdef VIDEO_LAYER
    case static_cast<int>(BaseLayer::LayerType::VIDEO): {
#ifdef MDK_SUPPORT
        // AdaptiveVideoLayer is used for slide layers on both master and nodes.
        // The main video layer (main.cpp) always uses MPV directly.
        AdaptiveVideoLayer* newVideo = new AdaptiveVideoLayer(opa1, opa2);
        newLayer = newVideo;
#else
        VideoLayer* newVideo = new VideoLayer(opa1);
        newLayer = newVideo;
#endif
        if (newLayer) {
            newLayer->setEOFMode(2);
        }
        break;
    }
#endif
#ifdef MULTI_VIDEO_LAYER
    case static_cast<int>(BaseLayer::LayerType::MULTIVIDEO): {
        MultiVideoLayer* newMultiVideo = new MultiVideoLayer(opa1);
        newMultiVideo->setEOFMode(2);
        newLayer = newMultiVideo;
        break;
    }
#endif
#ifdef AUDIO_LAYER
    case static_cast<int>(BaseLayer::LayerType::AUDIO): {
        AudioLayer* newAudio = new AudioLayer(opa1);
        newAudio->setEOFMode(2);
        newLayer = newAudio;
        break;
    }
#endif
#ifdef PDF_LAYER
    case static_cast<int>(BaseLayer::LayerType::PDF): {
        PdfLayer* newPDF = new PdfLayer();
        newLayer = newPDF;
        break;
    }
#endif
#ifdef NDI_LAYER
    case static_cast<int>(BaseLayer::LayerType::NDI): {
        NdiLayer *newNDI = new NdiLayer();
        newLayer = newNDI;
        break;
    }
#endif
#ifdef OMT_LAYER
    case static_cast<int>(BaseLayer::LayerType::OMT): {
        OmtLayer* newOmt = new OmtLayer();
        newLayer = newOmt;
        break;
    }
#endif
#ifdef SPOUT_LAYER
    case static_cast<int>(BaseLayer::LayerType::SPOUT): {
        SpoutLayer* newSpout = new SpoutLayer();
        newLayer = newSpout;
        break;
    }
#endif
#ifdef STREAM_LAYER
    case static_cast<int>(BaseLayer::LayerType::STREAM): {
        StreamLayer* newStream = new StreamLayer(opa1);
        newLayer = newStream;
        break;
    }
#endif
#ifdef YOUTUBE_LAYER
    case static_cast<int>(BaseLayer::LayerType::YOUTUBE): {
        YoutubeLayer* newYoutube = new YoutubeLayer(opa1);
        newLayer = newYoutube;
        break;
    }
#endif
#ifdef TEXT_LAYER
    case static_cast<int>(BaseLayer::LayerType::TEXT): {
        TextLayer* newText = new TextLayer();
        newLayer = newText;
        break;
    }
#endif
#ifdef CONTROL_LAYER
    case static_cast<int>(BaseLayer::LayerType::CONTROL): {
        ControlLayer* newControl = new ControlLayer();
        newLayer = newControl;
        break;
    }
#endif
#ifdef REST_LAYER
    case static_cast<int>(BaseLayer::LayerType::REST): {
        RestLayer* newRest = new RestLayer();
        newLayer = newRest;
        break;
    }
#endif
#ifdef WEBRTC_LAYER
    case static_cast<int>(BaseLayer::LayerType::WEBRTC): {
        WebRTCLayer* newWebRtc = new WebRTCLayer();
        newLayer = newWebRtc;
        break;
    }
#endif
#ifdef DIRECTSHOW_LAYER
    case static_cast<int>(BaseLayer::LayerType::DIRECTSHOW): {
        DirectShowLayer* newDirectShow = new DirectShowLayer();
        newLayer = newDirectShow;
        break;
    }
#endif
#ifdef NODE_STREAM_LAYER
    case static_cast<int>(BaseLayer::LayerType::NODESTREAM): {
        newLayer = new NodeStreamLayer();
        break;
    }
#endif
    default:
        break;
    }

    if (newLayer) {
        newLayer->setIsMaster(isMaster);
#ifdef AUDIO_LAYER
        if (AudioSettings::enableAudioOnMaster() || AudioSettings::enableAudioOnNodes()) {
            newLayer->enableAudio(true);
        }
#endif
        if (numID != 0)
            newLayer->setIdentifier(numID);
        else
            newLayer->updateIdentifierBasedOnCount();
    }

    return newLayer;
}

BaseLayer::BaseLayer() {
    m_type = BASE;
    m_hierachy = FRONT;
    m_title = "";
    m_filepath = "";
    m_volume = 100;
    m_volumeScaling = 1.f;
    m_isLocked = false;
    m_isEnabled = true;
    m_isMaster = false;
    m_existOnMasterOnly = false;
    m_shouldUpdate = false;
    m_shouldUpdateFrame = false;
    m_hasInitialized = false;
    m_keepVisibilityForNumSlides = 0;
    m_identifier = 0;
    m_pendingStart = false;
    m_ndiOutputEnabled = false;
    m_ndiSenderName = "";
    m_loadStatusPath = "";
    m_loadError = "";
    setNeedSync();
}

BaseLayer::~BaseLayer() {
    planeData.mesh.reset();
#ifdef NDI_SUPPORT
    m_ndiSender.reset();
#endif
#ifdef NODE_STREAM_SUPPORT
    m_nodeStreamSender.reset();
#endif
}

void BaseLayer::cleanup() {
    // Overwrite in derived class, but always call cleanupNdiOutput()
    cleanupNdiOutput();
    cleanupNodeStreamOutput();
}

void BaseLayer::initialize() {
    // Overwrite in derived class
}

void BaseLayer::initializeGL() {
    // Overwrite in derived class
}

void BaseLayer::initializeAndLoad(std::string) {
    // Overwrite in derived class
}

void BaseLayer::update(bool) {
    // Overwrite in derived class
}

void BaseLayer::updateFrame() {
    // Overwrite in derived class
}

bool BaseLayer::renderingIsOn() const {
    // Overwrite in derived class
    return false;
}

void BaseLayer::reportSwap() {
    // Default no-op. Override in derived classes (e.g., MpvLayer) to report swap to the underlying renderer.
}

void BaseLayer::start() {
    // Overwrite in derived class
}

void BaseLayer::stop() {
    // Overwrite in derived class
}

void BaseLayer::requestStart() {
    // Start now when possible; otherwise remember the request so the render loop
    // can fulfil it once a slow load (e.g. a stream) finally completes. Without
    // this, a play signal fired before the load finished was simply dropped and
    // the layer stayed paused forever.
    if (isMaster() && !ready()) {
        m_pendingStart = true;
        return;
    }
    start();
}

bool BaseLayer::fulfillPendingStart() {
    if (m_pendingStart && isMaster() && shouldUpdate() && alpha() > 0.f && ready()) {
        m_pendingStart = false;
        start();
        return true;
    }
    return false;
}

bool BaseLayer::pause() {
    return true;
    // Overwrite in derived class
}

void BaseLayer::setPause(bool) {
    // Overwrite in derived class
}

double BaseLayer::position() {
    return 0.0;
    // Overwrite in derived class
}

void BaseLayer::setPosition(double) {
    // Overwrite in derived class
}

double BaseLayer::duration() {
    return 0.0;
    // Overwrite in derived class
}

double BaseLayer::remaining() {
    return 0.0;
    // Overwrite in derived class
}

bool BaseLayer::hasAudio() const {
    return false;
}

namespace {
// Time constant for the exponential decay of the reported audio level. A value around
// 100 ms reads like a typical VU meter: fast enough to follow speech, slow enough that
// individual frames do not flicker.
constexpr int64_t kAudioLevelDecayMs = 100;
// If no new samples have been reported for this long the level is considered stale and
// falls back to zero (stream stopped, audio disabled, sender silent).
constexpr int64_t kAudioLevelStaleMs = 250;

int64_t steadyClockMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
} // namespace

float BaseLayer::audioLevel() const {
    if (!m_audioLevelsEnabled.load(std::memory_order_acquire)) {
        return 0.f; // meter disabled -> no level computation at all
    }

    const int64_t lastReport = m_audioPeakTimeMs.load(std::memory_order_relaxed);
    if (lastReport == 0) {
        return 0.f;
    }

    const int64_t ageMs = steadyClockMs() - lastReport;
    if (ageMs > kAudioLevelStaleMs) {
        return 0.f; // no new samples for a while -> treat as silence
    }

    float level = m_audioPeak.load(std::memory_order_relaxed);
    if (ageMs > 0) {
        // Decay since the last report so the meter falls smoothly between updates.
        level *= std::exp(-static_cast<float>(ageMs) / static_cast<float>(kAudioLevelDecayMs));
    }

    return std::min(1.f, std::max(0.f, level));
}

void BaseLayer::reportAudioLevel(float peak) {
    if (peak < 0.f) {
        peak = 0.f;
    } else if (peak > 1.f) {
        peak = 1.f;
    }

    const int64_t now = steadyClockMs();
    const int64_t lastReport = m_audioPeakTimeMs.load(std::memory_order_relaxed);

    float held = m_audioPeak.load(std::memory_order_relaxed);
    if (lastReport != 0) {
        const int64_t dtMs = now - lastReport;
        if (dtMs > 0 && dtMs <= kAudioLevelStaleMs) {
            // Decay the held peak for the time elapsed since the previous report so that
            // sustained signals hold their level while gaps between frames release it.
            held *= std::exp(-static_cast<float>(dtMs) / static_cast<float>(kAudioLevelDecayMs));
        } else if (dtMs > kAudioLevelStaleMs) {
            held = 0.f; // the stream was silent/stopped in between -> restart from zero
        }
    }

    if (peak > held) {
        held = peak;
    }

    m_audioPeak.store(held, std::memory_order_relaxed);
    m_audioPeakTimeMs.store(now, std::memory_order_release);
}

void BaseLayer::setAudioLevelsEnabled(bool enabled) {
    if (m_audioLevelsEnabled.load(std::memory_order_relaxed) == enabled) {
        return;
    }
    m_audioLevelsEnabled.store(enabled, std::memory_order_release);
    if (!enabled) {
        // Drop the held level so that re-enabling starts from silence.
        m_audioPeak.store(0.f, std::memory_order_relaxed);
        m_audioPeakTimeMs.store(0, std::memory_order_release);
    }
}

bool BaseLayer::audioLevelsEnabled() const {
    return m_audioLevelsEnabled.load(std::memory_order_acquire);
}

int BaseLayer::audioId() {
    return -1;
}

void BaseLayer::setAudioId(int) {
    // Overwrite in derived class
}

bool BaseLayer::isAudioEnabled() const {
    // Overwrite in derived class
    return isMaster(); // always support audio on master
}

void BaseLayer::enableAudio(bool) {
    // Overwrite in derived class
}

std::vector<Track>* BaseLayer::audioTracks() {
    // Overwrite in derived class
    return nullptr;
}

void BaseLayer::updateAudioOutput() {
    // Overwrite in derived class
}

void BaseLayer::setVolume(int, bool) {
    // Overwrite in derived class
}

void BaseLayer::setVolumeScaling(float vScale) {
    m_volumeScaling = vScale;
}

void BaseLayer::setVolumeMute(bool) {
    // Overwrite in derived class
}

bool BaseLayer::existOnMasterOnly() const {
    // Overwrite in derived class
    return m_existOnMasterOnly;
}

void BaseLayer::setExistOnMasterOnly(bool value) {
    if (m_existOnMasterOnly == value)
        return;

    m_existOnMasterOnly = value;
    setNeedSync();
}

int BaseLayer::eofMode() const {
    return -1;
}

void BaseLayer::setEOFMode(int) {
    // Overwrite in derived class
}

void BaseLayer::setTimePause(bool, bool) {
    // Overwrite in derived class
}

void BaseLayer::setTimePosition(double, bool) {
    // Overwrite in derived class
}

void BaseLayer::setLoopTime(double, double, bool) {
    // Overwrite in derived class
}

bool BaseLayer::loopTimeEnabled() const {
    return false;
}

double BaseLayer::loopTimeA() const {
    return 0.0;
}

double BaseLayer::loopTimeB() const {
    return 0.0;
}

void BaseLayer::setValue(std::string, int) {
    // Overwrite in derived class
}

bool BaseLayer::isQRCodeDetectionEnabled() const {
    return false;
}

void BaseLayer::setQRCodeDetectionEnabled(bool) {
    // Overwrite in derived class
}

int BaseLayer::textureDivisionMode() const {
    return 0;
}

void BaseLayer::setTextureDivisionMode(int) {
    // Overwrite in derived class
}

int BaseLayer::textureDivisionGrid() const {
    return 0;
}

void BaseLayer::setTextureDivisionGrid(int) {
    // Overwrite in derived class
}

void BaseLayer::encodeTypeCore(std::vector<std::byte>&) {
    // Overwrite in derived class
}

void BaseLayer::decodeTypeCore(const std::vector<std::byte>&, unsigned int&) {
    // Overwrite in derived class
}

void BaseLayer::encodeTypeAlways(std::vector<std::byte>&) {
    // Overwrite in derived class
}

void BaseLayer::decodeTypeAlways(const std::vector<std::byte>&, unsigned int&) {
    // Overwrite in derived class
}

void BaseLayer::encodeTypeProperties(std::vector<std::byte>&) {
    // Overwrite in derived class
}

void BaseLayer::decodeTypeProperties(const std::vector<std::byte>&, unsigned int&) {
    // Overwrite in derived class
}

bool BaseLayer::hasSubLayers() const {
    return false;
}

std::vector<std::shared_ptr<BaseLayer>>& BaseLayer::getSubLayers() const {
    static std::vector<std::shared_ptr<BaseLayer>> empty;
    return empty;
}

void BaseLayer::encodeBaseCore(std::vector<std::byte>& data) const {
    sgct::serializeObject(data, m_hierachy);
    sgct::serializeObject(data, m_filepath);
    sgct::serializeObject(data, renderData.flipY);
}

void BaseLayer::decodeBaseCore(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_hierachy);
    sgct::deserializeObject(data, pos, m_filepath);
    sgct::deserializeObject(data, pos, renderData.flipY);

    // Marking as needSync means we know update has occured, which we need to clear
    m_needSync = true;
}

void BaseLayer::encodeBaseAlways(std::vector<std::byte>& data) const {
    sgct::serializeObject(data, m_shouldUpdate);
    sgct::serializeObject(data, m_shouldPreLoad);
    sgct::serializeObject(data, renderData.alpha);
}

void BaseLayer::decodeBaseAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_shouldUpdate);
    sgct::deserializeObject(data, pos, m_shouldPreLoad);
    sgct::deserializeObject(data, pos, renderData.alpha);
}

void BaseLayer::encodeBaseProperties(std::vector<std::byte>& data) const {
    sgct::serializeObject(data, renderData.gridMode);
    sgct::serializeObject(data, renderData.stereoMode);
    sgct::serializeObject(data, renderData.eyeMode);

    sgct::serializeObject(data, renderData.roiEnabled);
    if (renderData.roiEnabled) {
        sgct::serializeObject(data, renderData.roi.x);
        sgct::serializeObject(data, renderData.roi.y);
        sgct::serializeObject(data, renderData.roi.z);
        sgct::serializeObject(data, renderData.roi.w);
    }

    if (renderData.gridMode == BaseLayer::GridMode::Plane) {
        sgct::serializeObject(data, planeData.azimuth);
        sgct::serializeObject(data, planeData.elevation);
        sgct::serializeObject(data, planeData.roll);
        sgct::serializeObject(data, planeData.distance);
        sgct::serializeObject(data, planeData.horizontal);
        sgct::serializeObject(data, planeData.vertical);
        sgct::serializeObject(data, planeData.specifiedSize.x);
        sgct::serializeObject(data, planeData.specifiedSize.y);
        sgct::serializeObject(data, planeData.aspectRatioConsideration);
    }
    else {
        sgct::serializeObject(data, renderData.rotate.x);
        sgct::serializeObject(data, renderData.rotate.y);
        sgct::serializeObject(data, renderData.rotate.z);
    }
}

void BaseLayer::decodeBaseProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, renderData.gridMode);
    sgct::deserializeObject(data, pos, renderData.stereoMode);
    sgct::deserializeObject(data, pos, renderData.eyeMode);

    sgct::deserializeObject(data, pos, renderData.roiEnabled);
    if (renderData.roiEnabled) {
        sgct::deserializeObject(data, pos, renderData.roi.x);
        sgct::deserializeObject(data, pos, renderData.roi.y);
        sgct::deserializeObject(data, pos, renderData.roi.z);
        sgct::deserializeObject(data, pos, renderData.roi.w);
    }

    if (renderData.gridMode == BaseLayer::GridMode::Plane) {
        sgct::deserializeObject(data, pos, planeData.azimuth);
        sgct::deserializeObject(data, pos, planeData.elevation);
        sgct::deserializeObject(data, pos, planeData.roll);
        sgct::deserializeObject(data, pos, planeData.distance);
        sgct::deserializeObject(data, pos, planeData.horizontal);
        sgct::deserializeObject(data, pos, planeData.vertical);
        sgct::deserializeObject(data, pos, planeData.specifiedSize.x);
        sgct::deserializeObject(data, pos, planeData.specifiedSize.y);
        sgct::deserializeObject(data, pos, planeData.aspectRatioConsideration);
    }
    else {
        sgct::deserializeObject(data, pos, renderData.rotate.x);
        sgct::deserializeObject(data, pos, renderData.rotate.y);
        sgct::deserializeObject(data, pos, renderData.rotate.z);
    }
}

void BaseLayer::encodeFull(std::vector<std::byte>& data) {
    encodeBaseCore(data);
    encodeBaseAlways(data);
    encodeBaseProperties(data);
    encodeTypeCore(data);
    encodeTypeAlways(data);
    encodeTypeProperties(data);
}
void BaseLayer::decodeFull(const std::vector<std::byte>& data, unsigned int& pos) {
    decodeBaseCore(data, pos);
    decodeBaseAlways(data, pos);
    decodeBaseProperties(data, pos);
    decodeTypeCore(data, pos);
    decodeTypeAlways(data, pos);
    decodeTypeProperties(data, pos);
}

void BaseLayer::encodeAlways(std::vector<std::byte>& data) {
    encodeBaseAlways(data);
    encodeTypeAlways(data);
}

void BaseLayer::decodeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    decodeBaseAlways(data, pos);
    decodeTypeAlways(data, pos);
}

bool BaseLayer::hasInitialized() const {
    return m_hasInitialized;
}

bool BaseLayer::isMaster() const {
    return m_isMaster;
}

uint32_t BaseLayer::identifier() const {
    return m_identifier;
}

bool BaseLayer::isLocked() const {
    return m_isLocked;
}

void BaseLayer::setIsLocked(bool locked) {
    m_isLocked = locked;
}

bool BaseLayer::isEnabled() const {
    return m_isEnabled;
}

void BaseLayer::setEnabled(bool enabled) {
    m_isEnabled = enabled;
}

bool BaseLayer::needSync() const {
    if (m_needSync)
        return true;
    // Check if any sublayer needs sync (division mode)
    if (hasSubLayers()) {
        for (const auto& sublayer : getSubLayers()) {
            if (sublayer && sublayer->needSync())
                return true;
        }
    }
    return false;
}

void BaseLayer::setHasSynced() {
    if (m_syncIteration > 0) {
        m_syncIteration--;
    }
    else {
        m_needSync = false;
    }
    // Also mark sublayers as synced
    if (hasSubLayers()) {
        for (const auto& sublayer : getSubLayers()) {
            if (sublayer)
                sublayer->setHasSynced();
        }
    }
}

BaseLayer::LayerType BaseLayer::type() const {
    return m_type;
}

void BaseLayer::setType(LayerType t) {
    m_type = t;
    setNeedSync();
}

BaseLayer::LayerHierarchy BaseLayer::hierarchy() const {
    return m_hierachy;
}

void BaseLayer::setHierarchy(LayerHierarchy h) {
    m_hierachy = h;
    setNeedSync();
}

std::string BaseLayer::typeName() const {
    return typeDescription(m_type);
}

std::string BaseLayer::title() const {
    return m_title;
}

void BaseLayer::setTitle(std::string t) {
    m_title = t;
}

std::string BaseLayer::filepath() const {
    return m_filepath;
}

void BaseLayer::setFilePath(std::string p) {
    m_filepath = p;
    setNeedSync();
}

std::string BaseLayer::loadStatusPath() const {
    const std::lock_guard<std::mutex> lock(m_loadStatusMutex);
    return m_loadStatusPath;
}

std::string BaseLayer::loadError() const {
    const std::lock_guard<std::mutex> lock(m_loadStatusMutex);
    return m_loadError;
}

bool BaseLayer::loadFailed() const {
    const std::lock_guard<std::mutex> lock(m_loadStatusMutex);
    return !m_loadError.empty();
}

void BaseLayer::setLoadError(const std::string &path, const std::string &err) {
    if (err.empty()) {
        clearLoadError();
        return;
    }
    const std::lock_guard<std::mutex> lock(m_loadStatusMutex);
    if (m_loadError == err && m_loadStatusPath == path)
        return; // idempotent - the verifier syncs this every tick
    m_loadStatusPath = path;
    m_loadError = err;
}

void BaseLayer::clearLoadError() {
    const std::lock_guard<std::mutex> lock(m_loadStatusMutex);
    if (m_loadError.empty())
        return; // idempotent
    m_loadStatusPath.clear();
    m_loadError.clear();
}

// Default: nothing to refresh - the layer's loader reports failures directly via setLoadError().
void BaseLayer::collectLoadStatus() {
}

int BaseLayer::volume() const {
    return m_volume;
}

int BaseLayer::keepVisibilityForNumSlides() const {
    return m_keepVisibilityForNumSlides;
}

void BaseLayer::setKeepVisibilityForNumSlides(int k) {
    m_keepVisibilityForNumSlides = k;
}

unsigned int BaseLayer::textureId() const {
    return renderData.texId;
}

unsigned int BaseLayer::textureInternalFormat() const {
    return GL_RGBA8;
}

int BaseLayer::width() const {
    return renderData.width;
}

int BaseLayer::height() const {
    return renderData.height;
}

float BaseLayer::alpha() const {
    return renderData.alpha;
}

void BaseLayer::setAlpha(float a) {
    if(a == alpha()) {
        return;
    }

    if (a > 0.f) {
        // Always set as should update if visible.
        // However might still be update if not visible.
        // See slide scheme.
        setShouldUpdate(true);
    }

    // Alpha starts/stop layer depending on visibility changes.
    if (isMaster()) {
        if (alpha() <= 0.f && a > 0.f) {
            if (ready()) {
                start();
            } else {
                m_pendingStart = true;
            }
        }
        else if (alpha() > 0.f && a <= 0.f) {
            m_pendingStart = false;
            stop();
        }

        // Check if a deferred start can now be fulfilled
        if (m_pendingStart && a > 0.f && ready()) {
            m_pendingStart = false;
            start();
        }

       //Alpha controls volume level as well, from 0 to desired value (100%)
       float volLevelF = static_cast<float>(volume()) * a;
       if (m_volumeScaling >= 0.f && m_volumeScaling < 1.f) {
           volLevelF *= m_volumeScaling;
       }
       setVolume(static_cast<int>(volLevelF), false);
    }

    renderData.alpha = a;

    // Apply alpha on sublayers as well, so they don't have to handle it themselves.
    if (hasSubLayers()) {
        for (const auto& sublayer : getSubLayers()) {
            if (sublayer) {
                sublayer->setAlpha(a);
            }
        }
    }

    // Is handled always right now anyway.
    // If slideModel or layerModel changed
    // setNeedSync();
}

bool BaseLayer::shouldUpdate() const {
    return m_shouldUpdate;
}

void BaseLayer::setShouldUpdate(bool value) {
    m_shouldUpdate = value;
    if (value && m_pendingStart && isMaster() && ready()) {
        m_pendingStart = false;
        start();
    }
}

bool BaseLayer::shouldUpdateFrame() const {
    return m_shouldUpdateFrame;
}

void BaseLayer::setShouldUpdateFrame(bool value) {
    m_shouldUpdateFrame = value;
}

bool BaseLayer::shouldPreLoad() const {
    return m_shouldPreLoad;
}

void BaseLayer::setShouldPreLoad(bool value) {
    m_shouldPreLoad = value;
}

bool BaseLayer::flipY() const {
    return renderData.flipY;
}

void BaseLayer::setFlipY(bool f) {
    renderData.flipY = f;
    setNeedSync();
}

uint8_t BaseLayer::gridMode() const {
    return renderData.gridMode;
}

void BaseLayer::setGridMode(uint8_t g) {
    renderData.gridMode = g;
    if (!isMaster() && gridMode() == GridMode::Plane) {
        updatePlane();
    }
    setNeedSync();
}

uint8_t BaseLayer::stereoMode() const {
    return renderData.stereoMode;
}

void BaseLayer::setStereoMode(uint8_t s) {
    renderData.stereoMode = s;
    if (!isMaster() && gridMode() == GridMode::Plane) {
        updatePlane();
    }
    setNeedSync();
}

uint8_t BaseLayer::eyeMode() const {
    return renderData.eyeMode;
}

void BaseLayer::setEyeMode(uint8_t e) {
    renderData.eyeMode = e;
    setNeedSync();
}

// Returns false if this layer should be skipped for the given frustum eye
 // (frustumEye: 0=Mono, 1=StereoLeft, 2=StereoRight - matches sgct::FrustumMode)
bool BaseLayer::shouldRenderForEye(int frustumEye) const {
    if (renderData.eyeMode == static_cast<uint8_t>(EyeMode::Left))
        return frustumEye != 2;
    if (renderData.eyeMode == static_cast<uint8_t>(EyeMode::Right))
        return frustumEye == 2;
    return true;
}

const glm::vec3 &BaseLayer::rotate() const {
    return renderData.rotate;
}

void BaseLayer::setRotate(const glm::vec3 &r) {
    renderData.rotate = r;
    setNeedSync();
}

const glm::vec3 &BaseLayer::translate() const {
    return renderData.translate;
}

void BaseLayer::setTranslate(const glm::vec3 &t) {
    renderData.translate = t;
    setNeedSync();
}

bool BaseLayer::roiEnabled() const {
    return renderData.roiEnabled;
}

void BaseLayer::setRoiEnabled(bool value) {
    renderData.roiEnabled = value;
    setNeedSync();
}

const glm::vec4 &BaseLayer::roi() const {
    return renderData.roi;
}

void BaseLayer::setRoi(const glm::vec4 &r) {
    renderData.roi = r;
    setNeedSync();
}

void BaseLayer::setRoi(float x, float y, float width, float height) {
    renderData.roi.x = x;
    renderData.roi.y = y;
    renderData.roi.z = width;
    renderData.roi.w = height;
    setNeedSync();
}

double BaseLayer::planeAzimuth() const {
    return planeData.azimuth;
}

void BaseLayer::setPlaneAzimuth(double pA) {
    planeData.azimuth = pA;
    setNeedSync();
}

double BaseLayer::planeElevation() const {
    return planeData.elevation;
}

void BaseLayer::setPlaneElevation(double pE) {
    planeData.elevation = pE;
    setNeedSync();
}

double BaseLayer::planeRoll() const {
    return planeData.roll;
}

void BaseLayer::setPlaneRoll(double pR) {
    planeData.roll = pR;
    setNeedSync();
}

double BaseLayer::planeDistance() const {
    return planeData.distance;
}

void BaseLayer::setPlaneDistance(double pD) {
    planeData.distance = pD;
    setNeedSync();
}

double BaseLayer::planeHorizontal() const {
    return planeData.horizontal;
}

void BaseLayer::setPlaneHorizontal(double pH) {
    planeData.horizontal = pH;
    setNeedSync();
}

double BaseLayer::planeVertical() const {
    return planeData.vertical;
}

void BaseLayer::setPlaneVertical(double pV) {
    planeData.vertical = pV;
    setNeedSync();
}

double BaseLayer::planeWidth() const {
    return planeData.specifiedSize.x;
}

void BaseLayer::setPlaneWidth(double pW) {
    planeData.specifiedSize.x = static_cast<float>(pW);
    if (!isMaster() && gridMode() == GridMode::Plane) {
        updatePlane();
    }
    setNeedSync();
}

double BaseLayer::planeHeight() const {
    return planeData.specifiedSize.y;
}

void BaseLayer::setPlaneHeight(double pH) {
    planeData.specifiedSize.y = static_cast<float>(pH);
    if (!isMaster() && gridMode() == GridMode::Plane) {
        updatePlane();
    }
    setNeedSync();
}

uint8_t BaseLayer::planeAspectRatio() const {
    return planeData.aspectRatioConsideration;
}

void BaseLayer::setPlaneAspectRatio(uint8_t parc) {
    planeData.aspectRatioConsideration = parc;
    if (!isMaster() && gridMode() == GridMode::Plane) {
        updatePlane();
    }
    setNeedSync();
}

void BaseLayer::setPlaneSize(glm::vec2 pS, uint8_t parc) {
    planeData.specifiedSize = pS;
    planeData.aspectRatioConsideration = parc;
    if (!isMaster() && gridMode() == GridMode::Plane) {
        updatePlane();
    }
    setNeedSync();
}

void BaseLayer::drawPlane() const {
    if (planeData.mesh) {
        planeData.mesh->draw();
    }
}

bool BaseLayer::hasPlane() const {
    return planeData.mesh != nullptr;
}

void BaseLayer::updatePlane() {
    if (renderData.width <= 0 || renderData.height <= 0)
        return;

    if (planeData.specifiedSize.x <= 0 || planeData.specifiedSize.y <= 0)
        return;

    float width = float(renderData.width);
    float height = float(renderData.height);
    if (renderData.roiEnabled) {
        width *= renderData.roi.z;
        height *= renderData.roi.w;
    }

    glm::vec2 calculatedPlaneSize = planeData.specifiedSize;
    int sm = renderData.stereoMode;

    float ratioMultiplier = 1.0f;
    if (sm == 1) { // Side-by-side
        //Check if we would should do FullSBS or HalfSBS based on aspect ratio
        if (width / height >= 2.f) {
            ratioMultiplier = 0.5f;
        }
    }
    else if (sm == 2) { // Top-bottom
        //Check if we would should do FullTP or HalfTP based on aspect ratio
        if (height / width >= 1.f) {
            ratioMultiplier = 2.0f;
        }
    }
    else if (sm == 3) { // Top-bottom-flip
        ratioMultiplier = 2.0f;
    }

    if (planeData.aspectRatioConsideration == 1) { // Calculate width from video
        float ratio = width / height;

        if (sm == 3) { // Top-bottom-flip
            ratio = height / width;
        }
        ratio *= ratioMultiplier;

        calculatedPlaneSize.x = ratio * planeData.specifiedSize.y;
    } else if (planeData.aspectRatioConsideration == 2) { // Calculate height from video
        float ratio = height / width;

        if (sm == 3) { // Top-bottom-flip
            ratio = width / height;
        }
        ratio *= ratioMultiplier;

        calculatedPlaneSize.y = ratio * planeData.specifiedSize.x;
    }

    // Re-create plane if it isn't correct size
    if (planeData.mesh == nullptr || calculatedPlaneSize.x != planeData.actualSize.x || calculatedPlaneSize.y != planeData.actualSize.y) {
        planeData.actualSize = calculatedPlaneSize;
        planeData.mesh = std::make_unique<PlaneGrid>(calculatedPlaneSize.x / 100.f, calculatedPlaneSize.y / 100.f);
        setNeedSync();
    }
}

void BaseLayer::setIsMaster(bool value) {
    m_isMaster = value;
}

void BaseLayer::setIdentifier(uint32_t id) {
    m_identifier = id;
}

void BaseLayer::updateIdentifierBasedOnCount() {
    m_identifier = m_id_gen++;
}

void BaseLayer::setNeedSync() {
    m_needSync = true;
#ifdef NETWORK_SYNC_SETTINGS
    m_syncIteration = PresentationSettings::networkSyncIterations();
#else
    m_syncIteration = 1;
#endif
}

bool BaseLayer::ndiOutputSupported() {
#ifdef NDI_SUPPORT
    return NdiSender::isSupported();
#else
    return false;
#endif
}

bool BaseLayer::ndiOutputEnabled() const {
    return m_ndiOutputEnabled;
}

void BaseLayer::setNdiOutputEnabled(bool enabled) {
    if (enabled == m_ndiOutputEnabled)
        return;

    if (enabled && m_nodeStreamOutputEnabled) {
        // Both NDI output and node streaming encode the layer texture on the master,
        // so they cannot be active at the same time.
        setNodeStreamOutputEnabled(false);
    }

    m_ndiOutputEnabled = enabled;

    if (m_ndiOutputEnabled) {
        if (m_ndiSenderName.empty()) {
            m_ndiSenderName = generateNdiSenderName();
        }
    }
    else {
        m_ndiSenderName.clear();
    }

#ifdef NDI_SUPPORT
    if (m_ndiSender) {
        // The sender is stopped here, but the OpenGL resources can only be
        // released from the render thread. updateNdiOutput() does that.
        m_ndiSender->stop();
    }
#endif
}

std::string BaseLayer::ndiSenderName() const {
    return m_ndiSenderName;
}

void BaseLayer::setNdiSenderName(std::string name) {
    if (name == m_ndiSenderName)
        return;

    m_ndiSenderName = name;

#ifdef NDI_SUPPORT
    // Force the sender to be re-created under the new name.
    if (m_ndiSender) {
        m_ndiSender->stop();
    }
#endif
}

std::string BaseLayer::generateNdiSenderName() const {
    std::string name = "C-Play Layer " + std::to_string(m_identifier);
    if (!m_title.empty()) {
        name += " - " + m_title;
    }
    return name;
}

bool BaseLayer::ndiOutputIsSending() const {
#ifdef NDI_SUPPORT
    return m_ndiSender && m_ndiSender->isSending();
#else
    return false;
#endif
}

void BaseLayer::updateNdiOutput() {
#ifdef NDI_SUPPORT
    if (!isMaster())
        return;

    if (!m_ndiOutputEnabled) {
        // Release the sender while we still have a current context.
        if (m_ndiSender) {
            m_ndiSender->cleanupGL();
            m_ndiSender.reset();
        }
        return;
    }

    if (!NdiSender::isSupported())
        return;

    if (!hasTexture() || textureId() == 0 || width() <= 0 || height() <= 0)
        return;

    if (!m_ndiSender) {
        m_ndiSender = std::make_unique<NdiSender>();
    }

    if (!m_ndiSender->isEnabled()) {
        if (m_ndiSenderName.empty()) {
            m_ndiSenderName = generateNdiSenderName();
        }
        m_ndiSender->setSource(NdiSender::sourceFromLayer(this));
        if (!m_ndiSender->start(m_ndiSenderName))
            return;
    }

    m_ndiSender->captureAndSend();
#endif
}

void BaseLayer::cleanupNdiOutput() {
#ifdef NDI_SUPPORT
    if (m_ndiSender) {
        m_ndiSender->cleanupGL();
        m_ndiSender.reset();
    }
#endif
}

#ifdef NODE_STREAM_SUPPORT
namespace {

struct NodeStreamSettings {
    std::string baseGroup = nodestream::kDefaultGroup;
    int basePort = nodestream::kDefaultPort;
    std::string interfaceAddress;
    int ttl = 1;
    int maxDatagram = 1472;
    int rateMbps = 0;
    int frameLockedWaitMs = 2;
    int latencyGuardMs = 1;
    bool allowPartialFrames = false;
    bool loopback = true;
    bool preferNdi = false;
};

std::mutex g_nodeStreamSettingsMutex;
NodeStreamSettings g_nodeStreamSettings;

NodeStreamSettings nodeStreamSettings() {
    std::lock_guard<std::mutex> lock(g_nodeStreamSettingsMutex);
    return g_nodeStreamSettings;
}

// Spreads the layers over consecutive groups so that the nodes only receive the streams they show.
std::string groupForLayer(const std::string &baseGroup, uint32_t identifier) {
    unsigned int a, b, c, d;
    if (std::sscanf(baseGroup.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
        return baseGroup;
    d = 1 + ((d + 253 + identifier % 254) % 254);
    return std::to_string(a) + "." + std::to_string(b) + "." + std::to_string(c) + "." + std::to_string(d);
}

std::string masterInterfaceAddress() {
    static const std::string address = [] {
        const std::string resolved = NodeStreamSocket::resolveIPv4(sgct::ClusterManager::instance().masterAddress());
        return NodeStreamSocket::isLoopbackAddress(resolved) ? std::string() : resolved;
    }();
    return address;
}

} // namespace
#endif

bool BaseLayer::nodeStreamOutputSupported() {
#ifdef NODE_STREAM_SUPPORT
    return true;
#else
    return false;
#endif
}

void BaseLayer::applyNodeStreamSettings() {
#if defined(NODE_STREAM_SUPPORT) && defined(NETWORK_SYNC_SETTINGS)
    NodeStreamSettings settings;
    settings.baseGroup = PresentationSettings::nodeStreamBaseGroup().trimmed().toStdString();
    if (settings.baseGroup.empty())
        settings.baseGroup = nodestream::kDefaultGroup;
    settings.basePort = PresentationSettings::nodeStreamBasePort();
    settings.interfaceAddress = PresentationSettings::nodeStreamInterface().trimmed().toStdString();
    settings.ttl = PresentationSettings::nodeStreamTTL();
    settings.maxDatagram = PresentationSettings::nodeStreamMaxDatagram();
    settings.rateMbps = PresentationSettings::nodeStreamRateMbps();
    settings.frameLockedWaitMs = PresentationSettings::nodeStreamFrameLockedWaitMs();
    settings.latencyGuardMs = PresentationSettings::nodeStreamLatencyGuardMs();
    settings.allowPartialFrames = PresentationSettings::nodeStreamAllowPartialFrames();
    settings.loopback = PresentationSettings::nodeStreamLoopback();
    settings.preferNdi = PresentationSettings::nodeStreamPreferNdi();
    std::lock_guard<std::mutex> lock(g_nodeStreamSettingsMutex);
    g_nodeStreamSettings = settings;
#endif
}

bool BaseLayer::nodeStreamOutputEnabled() const {
    return m_nodeStreamOutputEnabled;
}

void BaseLayer::setNodeStreamOutputEnabled(bool enabled) {
    if (!nodeStreamOutputSupported() || enabled == m_nodeStreamOutputEnabled)
        return;

    if (enabled && m_ndiOutputEnabled) {
        // Both NDI output and node streaming encode the layer texture on the master,
        // so they cannot be active at the same time.
        setNdiOutputEnabled(false);
    }

    m_nodeStreamOutputEnabled = enabled;
    // The nodes replace their layer with (or back from) a NodeStreamLayer.
    setNeedSync();
}

int BaseLayer::nodeStreamFormat() const {
    return m_nodeStreamFormat;
}

void BaseLayer::setNodeStreamFormat(int format) {
    m_nodeStreamFormat = std::clamp(format, 0, 3);
}

int BaseLayer::nodeStreamSyncMode() const {
    return m_nodeStreamSyncMode;
}

void BaseLayer::setNodeStreamSyncMode(int mode) {
    mode = std::clamp(mode, 0, 1);
    if (mode == m_nodeStreamSyncMode)
        return;
    m_nodeStreamSyncMode = mode;
    if (m_nodeStreamOutputEnabled)
        setNeedSync();
}

std::string BaseLayer::nodeStreamGroup() const {
    std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
    return m_nodeStreamGroup;
}

void BaseLayer::setNodeStreamGroup(std::string group) {
    {
        std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
        if (group == m_nodeStreamGroup)
            return;
        m_nodeStreamGroup = group;
    }
    if (m_nodeStreamOutputEnabled)
        setNeedSync();
}

int BaseLayer::nodeStreamPort() const {
    return m_nodeStreamPort;
}

void BaseLayer::setNodeStreamPort(int port) {
    port = std::clamp(port, 0, 65535);
    if (port == m_nodeStreamPort)
        return;
    m_nodeStreamPort = port;
    if (m_nodeStreamOutputEnabled)
        setNeedSync();
}

int BaseLayer::nodeStreamMaxFps() const {
    return m_nodeStreamMaxFps;
}

void BaseLayer::setNodeStreamMaxFps(int fps) {
    m_nodeStreamMaxFps = std::max(fps, 0);
}

int BaseLayer::nodeStreamUseNdi() const {
    return m_nodeStreamUseNdi;
}

void BaseLayer::setNodeStreamUseNdi(int value) {
    value = std::clamp(value, -1, 1);
    if (value == m_nodeStreamUseNdi)
        return;
    m_nodeStreamUseNdi = value;
    // The nodes may have to swap between a NodeStreamLayer and an NdiLayer.
    if (m_nodeStreamOutputEnabled)
        setNeedSync();
}

bool BaseLayer::nodeStreamUseNdiEffective() const {
#ifdef NODE_STREAM_SUPPORT
    if (!m_nodeStreamOutputEnabled)
        return false;
    const bool useNdi = m_nodeStreamUseNdi >= 0 ? m_nodeStreamUseNdi == 1 : nodeStreamSettings().preferNdi;
    if (!useNdi)
        return false;
#if defined(NDI_SUPPORT) && defined(NDI_LAYER)
    return NdiSender::isSupported();
#else
    return false;
#endif
#else
    return false;
#endif
}

std::string BaseLayer::nodeStreamNdiSenderName() const {
    return generateNdiSenderName();
}

bool BaseLayer::nodeStreamPreferNdi() {
#ifdef NODE_STREAM_SUPPORT
    return nodeStreamSettings().preferNdi;
#else
    return false;
#endif
}

void BaseLayer::markNodeStreamLayerForResync() {
    if (m_nodeStreamOutputEnabled)
        setNeedSync();
}

std::string BaseLayer::nodeStreamEffectiveGroup() const {
#ifdef NODE_STREAM_SUPPORT
    const std::string group = nodeStreamGroup();
    if (!group.empty())
        return group;
    return groupForLayer(nodeStreamSettings().baseGroup, m_identifier);
#else
    return std::string();
#endif
}

int BaseLayer::nodeStreamEffectivePort() const {
#ifdef NODE_STREAM_SUPPORT
    const int port = m_nodeStreamPort;
    return port > 0 ? port : nodeStreamSettings().basePort;
#else
    return 0;
#endif
}

bool BaseLayer::nodeStreamOutputIsSending() const {
#ifdef NODE_STREAM_SUPPORT
    std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
    if (m_nodeStreamSender && m_nodeStreamSender->isSending())
        return true;
    return m_nodeStreamNdiSender && m_nodeStreamNdiSender->isSending();
#else
    return false;
#endif
}

void BaseLayer::updateNodeStreamOutput() {
#ifdef NODE_STREAM_SUPPORT
    if (!isMaster())
        return;

    if (!m_nodeStreamOutputEnabled) {
        cleanupNodeStreamOutput();
        return;
    }

    if (!shouldUpdate() || !hasTexture() || textureId() == 0 || width() <= 0 || height() <= 0)
        return;

    if (nodeStreamUseNdiEffective()) {
        // NDI mode: the nodes receive this layer with an automatically created NDI layer.
        // The UDP path is fully bypassed, and the plain NDI output sender is separate.
        if (m_nodeStreamSender) {
            m_nodeStreamSender->cleanupGL();
            std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
            m_nodeStreamSender.reset();
        }

        const std::string name = nodeStreamNdiSenderName();
        if (!m_nodeStreamNdiSender || m_nodeStreamNdiStartedName != name) {
            if (m_nodeStreamNdiSender) {
                m_nodeStreamNdiSender->cleanupGL();
            }
            auto sender = std::make_unique<NdiSender>();
            sender->setSource(NdiSender::sourceFromLayer(this));
            if (!sender->start(name)) {
                std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
                m_nodeStreamNdiSender.reset();
                m_nodeStreamNdiStartedName.clear();
                m_nodeStreamNdiSyncedName.clear();
                return;
            }
            std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
            m_nodeStreamNdiSender = std::move(sender);
            m_nodeStreamNdiStartedName = name;
            m_nodeStreamNdiSyncedName.clear();
        }

        // Only the render thread replaces the sender, so it can be used without the lock here.
        m_nodeStreamNdiSender->captureAndSend();

        // The full NDI name (including the machine name) is only known once the sender
        // was created lazily. Re-sync the layer once it appears or changes, so the nodes
        // address the exact discoverable name.
        const std::string ndiName = m_nodeStreamNdiSender->ndiName();
        if (!ndiName.empty() && ndiName != m_nodeStreamNdiSyncedName) {
            std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
            m_nodeStreamNdiSyncedName = ndiName;
            setNeedSync();
        }
        return;
    }

    // UDP multicast mode: release the NDI sender, if any.
    if (m_nodeStreamNdiSender) {
        m_nodeStreamNdiSender->cleanupGL();
        std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
        m_nodeStreamNdiSender.reset();
        m_nodeStreamNdiStartedName.clear();
        m_nodeStreamNdiSyncedName.clear();
    }

    if (!m_nodeStreamSender) {
        auto sender = std::make_unique<NodeStreamSender>();
        sender->setSource(NodeStreamSender::sourceFromLayer(this));
        std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
        m_nodeStreamSender = std::move(sender);
    }

    const NodeStreamSettings settings = nodeStreamSettings();
    NodeStreamConfig config;
    config.group = nodeStreamEffectiveGroup();
    config.port = static_cast<uint16_t>(nodeStreamEffectivePort());
    config.interfaceAddress = settings.interfaceAddress.empty() ? masterInterfaceAddress() : settings.interfaceAddress;
    config.ttl = settings.ttl;
    config.loopback = settings.loopback;
    config.maxDatagram = settings.maxDatagram;
    config.rateMbps = settings.rateMbps;
    config.format = static_cast<nodestream::Format>(m_nodeStreamFormat.load());
    config.maxFps = m_nodeStreamMaxFps;
    config.streamId = m_identifier;

    // Only the render thread replaces the sender, so it can be used without the lock here.
    m_nodeStreamSender->captureAndSend(config);
#endif
}

void BaseLayer::cleanupNodeStreamOutput() {
#ifdef NODE_STREAM_SUPPORT
    if (m_nodeStreamNdiSender) {
        m_nodeStreamNdiSender->cleanupGL();
        std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
        m_nodeStreamNdiSender.reset();
        m_nodeStreamNdiStartedName.clear();
        m_nodeStreamNdiSyncedName.clear();
    }
    if (m_nodeStreamSender) {
        m_nodeStreamSender->cleanupGL();
        std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
        m_nodeStreamSender.reset();
    }
#endif
}

bool BaseLayer::syncToNodes() const {
    return !existOnMasterOnly() || m_nodeStreamOutputEnabled;
}

int BaseLayer::syncTypeForNodes() const {
#ifdef NODE_STREAM_SUPPORT
    if (m_nodeStreamOutputEnabled) {
        if (nodeStreamUseNdiEffective()) {
#if defined(NDI_LAYER)
            return static_cast<int>(NDI);
#else
            return static_cast<int>(INVALID);
#endif
        }
        return static_cast<int>(NODESTREAM);
    }
#endif
    return static_cast<int>(m_type);
}

void BaseLayer::encodeBaseCoreForNdiNodes(std::vector<std::byte>& data) const {
    // Like encodeBaseCore(), but the filepath slot carries the full NDI source name,
    // which is what an NdiLayer addresses its receiver by. The received image is
    // always top-down, so the node copy never flips.
    sgct::serializeObject(data, m_hierachy);
    std::string ndiName;
    {
        std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
        ndiName = m_nodeStreamNdiSyncedName;
    }
    if (ndiName.empty())
        ndiName = nodeStreamNdiSenderName();
    sgct::serializeObject(data, ndiName);
    const bool flipY = false;
    sgct::serializeObject(data, flipY);
}

void BaseLayer::encodeFullForNodes(std::vector<std::byte>& data) {
#ifdef NODE_STREAM_SUPPORT
    if (m_nodeStreamOutputEnabled && nodeStreamUseNdiEffective()) {
#if defined(NDI_LAYER)
        // Mirrors NdiLayer::decodeFull: the node creates an NdiLayer receiving this
        // layer as an NDI source. NdiLayer does not override decodeTypeCore, so the
        // type sections are written by NdiLayer::encodeNodeDefaultsForNodes().
        encodeBaseCoreForNdiNodes(data);
        encodeBaseAlways(data);
        encodeBaseProperties(data);
        NdiLayer::encodeNodeDefaultsForNodes(data, true);
        return;
#endif
    }
    if (m_nodeStreamOutputEnabled) {
        // Mirrors encodeFull with the NodeStreamLayer type sections.
        encodeBaseCore(data);
        encodeBaseAlways(data);
        encodeBaseProperties(data);

        const std::string group = nodeStreamEffectiveGroup();
        const int port = nodeStreamEffectivePort();
        const NodeStreamSettings settings = nodeStreamSettings();
        const uint8_t syncMode = static_cast<uint8_t>(m_nodeStreamSyncMode.load());
        const int maxWaitMs = settings.frameLockedWaitMs;
        const uint32_t streamId = m_identifier;
        sgct::serializeObject(data, group);
        sgct::serializeObject(data, port);
        sgct::serializeObject(data, syncMode);
        sgct::serializeObject(data, maxWaitMs);
        sgct::serializeObject(data, streamId);
        sgct::serializeObject(data, settings.allowPartialFrames);

        encodeNodeStreamTypeAlways(data);
        return;
    }
#endif
    encodeFull(data);
}

void BaseLayer::encodeNodeStreamTypeAlways(std::vector<std::byte>& data) const {
#ifdef NODE_STREAM_SUPPORT
    uint32_t sessionId = 0;
    uint32_t targetFrameId = 0;
    bool sending = false;
    const int64_t guardNs = static_cast<int64_t>(std::max(nodeStreamSettings().latencyGuardMs, 0)) * 1'000'000;
    {
        std::lock_guard<std::mutex> lock(m_nodeStreamMutex);
        if (m_nodeStreamSender) {
            sessionId = m_nodeStreamSender->sessionId();
            targetFrameId = m_nodeStreamSender->targetFrameId(guardNs);
            sending = m_nodeStreamSender->isSending();
        }
    }
    sgct::serializeObject(data, sessionId);
    sgct::serializeObject(data, targetFrameId);
    sgct::serializeObject(data, sending);
#else
    (void)data;
#endif
}

void BaseLayer::encodeAlwaysForNodes(std::vector<std::byte>& data) {
#ifdef NODE_STREAM_SUPPORT
    if (m_nodeStreamOutputEnabled && nodeStreamUseNdiEffective()) {
#if defined(NDI_LAYER)
        // Mirrors NdiLayer::decodeAlways.
        encodeBaseAlways(data);
        NdiLayer::encodeNodeDefaultsForNodes(data, false);
        return;
#endif
    }
    if (m_nodeStreamOutputEnabled) {
        encodeBaseAlways(data);
        encodeNodeStreamTypeAlways(data);
        return;
    }
#endif
    encodeAlways(data);
}
