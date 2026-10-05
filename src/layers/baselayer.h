/*
 * SPDX-FileCopyrightText:
 * 2024-2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef BASELAYER_H
#define BASELAYER_H

#include <glm/glm.hpp>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "track.h"
#include <utils/planegrid.h>

class NdiSender;
class NodeStreamSender;

class BaseLayer {
public:
    enum LayerType {
        BASE,
#ifdef IMAGE_LAYER
        IMAGE,
#endif
#ifdef VIDEO_LAYER
        VIDEO,
#endif
#ifdef MULTI_VIDEO_LAYER
        MULTIVIDEO,
#endif
#ifdef AUDIO_LAYER
        AUDIO,
#endif
#ifdef PDF_SUPPORT
        PDF,
#endif
#ifdef NDI_SUPPORT
        NDI,
#endif
#ifdef OMT_SUPPORT
        OMT,
#endif
#ifdef DIRECTSHOW_SUPPORT
        DIRECTSHOW,
#endif
#ifdef SPOUT_SUPPORT
        SPOUT,
#endif
#ifdef STREAM_LAYER
        STREAM,
#endif
#ifdef YOUTUBE_LAYER
        YOUTUBE,
#endif
#ifdef WEBRTC_LAYER
        WEBRTC,
#endif
#ifdef TEXT_LAYER
        TEXT,
#endif
#ifdef CONTROL_LAYER
        CONTROL,
#endif
#ifdef REST_LAYER
        REST,
#endif
#ifdef NODE_STREAM_SUPPORT
        NODESTREAM,
#endif
        TCP,
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

    enum EyeMode {
        Both,
        Left,
        Right
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
        uint8_t eyeMode = 0;
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
        std::unique_ptr<PlaneGrid> mesh = nullptr;
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
    virtual bool ready() const = 0;

    // Called after a frame has been presented to inform the layer about swap.
    // Override in derived classes (e.g. MpvLayer) to report swap to the underlying renderer.
    virtual void reportSwap();
    virtual bool hasTexture() const = 0;

    // Loader failure verification (node side): lets a layer refresh its load status before the
    // NodeLoaderVerifier reads it. Default is a no-op - layers whose loader reports failures
    // directly via setLoadError() do not override it. Overridden by layers whose loader state
    // lives outside BaseLayer's members (MPV event thread, MDK player polling, DirectShow worker).
    virtual void collectLoadStatus();

    virtual void start();
    virtual void stop();

    // Request playback, deferring it when the layer is not ready yet. A slow load
    // (e.g. a network stream) that completes after the request is fulfilled by the
    // render loop calling fulfillPendingStart() each frame, so a play signal that
    // arrives before the load finished is not lost.
    void requestStart();
    // Called from the render loop: starts a layer whose start() was deferred by
    // requestStart() (or setAlpha() on a visibility change) until ready() turned
    // true. Only fires for a visible, updating master layer. Returns true if started.
    bool fulfillPendingStart();

    virtual bool pause();
    virtual void setPause(bool paused);

    virtual double position();
    virtual void setPosition(double pos);

    virtual double duration();
    virtual double remaining();

    virtual bool hasAudio() const;
    // True when the layer reports live audio levels from its decoded audio path,
    // e.g. for a level meter in the LayerView (NDI, OMT, DirectShow and WebRTC do).
    virtual bool hasAudioLevels() const { return false; }
    // Current audio level in [0..1] for visualization purposes. The value decays
    // smoothly between sample reports and falls to zero when no new samples arrive.
    float audioLevel() const;
    // Report the peak of a freshly decoded/mixed audio frame (clamped to [0..1]).
    // Called from the layer's audio path, which may run on any thread. Callers should only
    // invoke this while audioLevelsEnabled() is true so the per-sample scan can be skipped.
    void reportAudioLevel(float peak);
    // Enable or disable live audio level reporting. While disabled the layers skip their
    // per-sample peak computation in the audio path and audioLevel() reports zero.
    void setAudioLevelsEnabled(bool enabled);
    bool audioLevelsEnabled() const;
    virtual int audioId();
    virtual void setAudioId(int id);
    virtual bool isAudioEnabled() const;
    virtual void enableAudio(bool enabled = true);
    virtual std::vector<Track>* audioTracks();
    virtual void updateAudioOutput();
    virtual void setVolume(int v, bool storeLevel = true);
    virtual void setVolumeScaling(float vScale);
    virtual void setVolumeMute(bool v);

    virtual bool existOnMasterOnly() const;
    // User controlled flag deciding if the layer should be synced to the nodes.
    void setExistOnMasterOnly(bool value);

    virtual int eofMode() const;
    virtual void setEOFMode(int eofMode);
    virtual void setTimePause(bool paused, bool updateTime = true);
    virtual void setTimePosition(double timePos, bool updateTime = true);
    virtual bool loopTimeEnabled() const;
    virtual double loopTimeA() const;
    virtual double loopTimeB() const;
    virtual void setLoopTime(double A, double B, bool enabled);
    virtual void setValue(std::string param, int val);

    virtual bool isQRCodeDetectionEnabled() const;
    virtual void setQRCodeDetectionEnabled(bool enabled);

    // Texture division mode: 0=None, 1=ImPres(QR), 2=Division
    virtual int textureDivisionMode() const;
    virtual void setTextureDivisionMode(int mode);

    // Texture division grid index: 0=1x1, 1=1x2, 2=2x1, 3=2x2, 4=2x3, 5=3x2, 6=3x3
    virtual int textureDivisionGrid() const;
    virtual void setTextureDivisionGrid(int grid);
    
    virtual void encodeTypeCore(std::vector<std::byte>& data);
    virtual void decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos);

    virtual void encodeTypeAlways(std::vector<std::byte>& data);
    virtual void decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    virtual void encodeTypeProperties(std::vector<std::byte>& data);
    virtual void decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos);

    virtual bool hasSubLayers() const;
    virtual std::vector<std::shared_ptr<BaseLayer>>& getSubLayers() const;

    // End virtual methods to use in derived classes

    void encodeBaseCore(std::vector<std::byte>& data) const;
    void decodeBaseCore(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeBaseAlways(std::vector<std::byte>& data) const;
    void decodeBaseAlways(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeBaseProperties(std::vector<std::byte>& data) const;
    void decodeBaseProperties(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeFull(std::vector<std::byte>& data);
    void decodeFull(const std::vector<std::byte>& data, unsigned int& pos);

    void encodeAlways(std::vector<std::byte>& data);
    void decodeAlways(const std::vector<std::byte>& data, unsigned int& pos);

    bool hasInitialized() const;

    bool isMaster() const;
    uint32_t identifier() const;

    bool isLocked() const;
    void setIsLocked(bool locked);

    bool isEnabled() const;
    void setEnabled(bool enabled);

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

    // Loader failure state for the NodeLoaderVerifier: the source path that failed to load and
    // a short human-readable reason. An empty error means no failure. Writers may run on loader
    // or worker threads, so all access is mutex-protected (see m_loadStatusMutex).
    std::string loadStatusPath() const;
    std::string loadError() const;
    bool loadFailed() const;

    int keepVisibilityForNumSlides() const;
    void setKeepVisibilityForNumSlides(int k);

    int volume() const;
    virtual unsigned int textureId() const;
    virtual unsigned int textureInternalFormat() const;
    virtual int width() const;
    virtual int height() const;

    float alpha() const;
    void setAlpha(float a);

    bool shouldUpdate() const;
    void setShouldUpdate(bool value);

    bool shouldUpdateFrame() const;
    void setShouldUpdateFrame(bool value);

    bool shouldPreLoad() const;
    void setShouldPreLoad(bool value);

    bool flipY() const;
    void setFlipY(bool f);

    uint8_t gridMode() const;
    void setGridMode(uint8_t g);

    uint8_t stereoMode() const;
    void setStereoMode(uint8_t s);

    uint8_t eyeMode() const;
    void setEyeMode(uint8_t e);
    bool shouldRenderForEye(int frustumEye) const;

    const glm::vec3 &rotate() const;
    void setRotate(const glm::vec3 &r);

    const glm::vec3 &translate() const;
    void setTranslate(const glm::vec3 &t);

    bool roiEnabled() const;
    void setRoiEnabled(bool value);

    const glm::vec4 &roi() const;
    void setRoi(const glm::vec4 &r);
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

    void drawPlane() const;
    bool hasPlane() const;
    void updatePlane();

    void setIsMaster(bool value);
    void setIdentifier(uint32_t id);
    void updateIdentifierBasedOnCount();

    bool shouldRenderForEye() const;

    // NDI output on the master. The nodes are not affected by this.
    // Mutually exclusive with node stream output: enabling one disables the other,
    // since both encode the layer texture on the master.
    static bool ndiOutputSupported();
    bool ndiOutputEnabled() const;
    void setNdiOutputEnabled(bool enabled);
    std::string ndiSenderName() const;
    void setNdiSenderName(std::string name);
    std::string generateNdiSenderName() const;
    bool ndiOutputIsSending() const;
    // Captures the layer texture and sends one NDI frame. Must be called with
    // the OpenGL context owning the layer texture current.
    void updateNdiOutput();
    // Releases the NDI sender and its OpenGL resources. Requires a current context.
    void cleanupNdiOutput();

    // Streams the layer texture to the nodes (GPU block compression + UDP multicast).
    // While enabled, the nodes show a NodeStreamLayer instead of their own copy of this layer.
    // Mutually exclusive with NDI output: enabling one disables the other.
    static bool nodeStreamOutputSupported();
    // Reloads the global node stream settings. Call on the GUI thread.
    static void applyNodeStreamSettings();
    bool nodeStreamOutputEnabled() const;
    void setNodeStreamOutputEnabled(bool enabled);
    // nodestream::Format (0 = Auto, 1 = BC1, 2 = BC3, 3 = BC7).
    int nodeStreamFormat() const;
    void setNodeStreamFormat(int format);
    // nodestream::SyncMode (0 = frame-locked, 1 = immediate).
    int nodeStreamSyncMode() const;
    void setNodeStreamSyncMode(int mode);
    // Empty / 0 means derived from the global settings.
    std::string nodeStreamGroup() const;
    void setNodeStreamGroup(std::string group);
    int nodeStreamPort() const;
    void setNodeStreamPort(int port);
    int nodeStreamMaxFps() const;
    void setNodeStreamMaxFps(int fps);
    std::string nodeStreamEffectiveGroup() const;
    int nodeStreamEffectivePort() const;
    bool nodeStreamOutputIsSending() const;
    // NDI mode for node streaming: -1 follows the global preference, 0 forces UDP
    // multicast, 1 forces NDI.
    int nodeStreamUseNdi() const;
    void setNodeStreamUseNdi(int value);
    // True when node streaming for this layer should use NDI, taking the per-layer
    // override, the global preference and NDI availability into account.
    bool nodeStreamUseNdiEffective() const;
    // The NDI source name the nodes receive this layer as in NDI mode.
    std::string nodeStreamNdiSenderName() const;
    // The cached global "prefer NDI" preference.
    static bool nodeStreamPreferNdi();
    // Marks a streaming layer for a full re-sync, e.g. when the global NDI preference
    // changed and the nodes may have to swap between NodeStreamLayer and NdiLayer.
    void markNodeStreamLayerForResync();
    // Encodes and queues the layer texture. Must be called on the render thread.
    void updateNodeStreamOutput();
    // Stops the stream and releases its OpenGL resources. Requires a current context.
    void cleanupNodeStreamOutput();

    // How the layer is synced to the nodes, taking node streaming into account.
    bool syncToNodes() const;
    int syncTypeForNodes() const;
    void encodeFullForNodes(std::vector<std::byte>& data);
    void encodeAlwaysForNodes(std::vector<std::byte>& data);

protected:
    void setNeedSync();
    void encodeNodeStreamTypeAlways(std::vector<std::byte>& data) const;
    // Base-core section for the NDI node-stream mode: like encodeBaseCore(), but the
    // filepath slot carries the NDI source name the node's NdiLayer receives.
    void encodeBaseCoreForNdiNodes(std::vector<std::byte>& data) const;

    // Loader failure state mutators. Idempotent (no-op when the state is unchanged) so the
    // verifier's per-tick collectLoadStatus() sync stays cheap and chatter-free.
    void setLoadError(const std::string &path, const std::string &err);
    void clearLoadError();

    mutable std::mutex m_loadStatusMutex;
    std::string m_loadStatusPath;
    std::string m_loadError;

    LayerType m_type;
    LayerHierarchy m_hierachy;
    std::string m_title;
    std::string m_filepath;
    int m_volume;
    float m_volumeScaling;
    // Live audio level state (see audioLevel()/reportAudioLevel()). The writer is always
    // the layer's own single audio path, readers are lock-free.
    std::atomic<float> m_audioPeak{0.f};
    std::atomic<int64_t> m_audioPeakTimeMs{0}; // steady_clock ms of the last report (0 = never)
    std::atomic<bool> m_audioLevelsEnabled{false}; // gate for the per-sample peak scan in the audio path
    int m_keepVisibilityForNumSlides;
    bool m_isLocked;
    bool m_isEnabled;
    bool m_isMaster;
    bool m_existOnMasterOnly;
    bool m_shouldUpdate;
    bool m_shouldUpdateFrame;
    bool m_shouldPreLoad;
    bool m_hasInitialized;
    bool m_needSync;
    // Written by the GUI thread (requestStart/setAlpha/setShouldUpdate), read and
    // consumed by the render thread (fulfillPendingStart) - hence atomic.
    std::atomic_bool m_pendingStart{false};
    int m_syncIteration;

    mutable std::mutex m_updateMutex;
    mutable std::mutex m_updateFrameMutex;

    RenderParams renderData;
    PlaneParams planeData;

    uint32_t m_identifier;
    static std::atomic_uint32_t m_id_gen;

    bool m_ndiOutputEnabled;
    std::string m_ndiSenderName;
#ifdef NDI_SUPPORT
    std::unique_ptr<NdiSender> m_ndiSender;
#endif

    std::atomic_bool m_nodeStreamOutputEnabled{false};
    std::atomic<int> m_nodeStreamFormat{0};
    std::atomic<int> m_nodeStreamSyncMode{0};
    std::atomic<int> m_nodeStreamPort{0};
    std::atomic<int> m_nodeStreamMaxFps{0};
    std::atomic<int> m_nodeStreamUseNdi{-1};
    std::string m_nodeStreamGroup;
    mutable std::mutex m_nodeStreamMutex;
#ifdef NODE_STREAM_SUPPORT
    // Created and released on the render thread, read under m_nodeStreamMutex by the sync encoder.
    std::unique_ptr<NodeStreamSender> m_nodeStreamSender;
    // The NDI sender used in NDI mode (kept separate from the plain NDI output sender).
    // Created and released on the render thread, read under m_nodeStreamMutex by the sync encoder.
    std::unique_ptr<NdiSender> m_nodeStreamNdiSender;
    // Render-thread only: the name the sender was started with, and the full NDI name
    // last reported to the nodes (the full name is only known once the sender runs).
    std::string m_nodeStreamNdiStartedName;
    std::string m_nodeStreamNdiSyncedName;
#endif
};

#endif // BASELAYER_H
