/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "capturelayer.h"
#include "audiosettings.h"
#include <capture/capturepresets.h>
#include <capture/nodecaptureinventory.h>
#include <utils/texturesubdivision.h>
#include <sgct/opengl.h>
#include <sgct/sgct.h>
#include <format>

CaptureLayer::CaptureLayer() {
    setType(BaseLayer::LayerType::CAPTURE);
    enableTextureSubdivision();
    // Capture frames are top-down; set here since JSON decoding only applies flipY on load.
    renderData.flipY = true;
}

CaptureLayer::~CaptureLayer() {
    cleanup();
}

void CaptureLayer::cleanup() {
    // The backend is shared and owns the texture; the last owner closes it on its render thread.
    std::shared_ptr<CaptureBackend> released;
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        released.swap(m_backend);
    }
    released.reset();
    m_backendKey.clear();
    renderData.texId = 0;
    renderData.width = 0;
    renderData.height = 0;
}

void CaptureLayer::initialize() {
    m_hasInitialized = true;
}

std::string CaptureLayer::presetKey() const {
    return m_presetKey;
}

void CaptureLayer::setPresetKey(const std::string& key) {
    if (m_presetKey != key) {
        m_presetKey = key;
        if (isMaster())
            setNeedSync();
    }
}

std::string CaptureLayer::targetNode() const {
    std::lock_guard<std::mutex> lock(m_targetMutex);
    return m_targetNode;
}

void CaptureLayer::setTargetNode(const std::string& role) {
    {
        std::lock_guard<std::mutex> lock(m_targetMutex);
        if (m_targetNode == role)
            return;
        m_targetNode = role;
    }
    if (isMaster())
        setNeedSync();
}

namespace {
// Used until the node has reported the size of the capture.
constexpr int kDefaultPlaceholderWidth = 1920;
constexpr int kDefaultPlaceholderHeight = 1080;
}

bool CaptureLayer::nodeCaptureSize(int& width, int& height) const {
    const std::string target = targetNode();
    if (target.empty() || !NodeCaptureInventory::isReady())
        return false;
    const NodeCaptureInventory::LayerStatus s = NodeCaptureInventory::instance().layerStatus(target, identifier());
    if (s.reported && s.width > 0 && s.height > 0) {
        width = s.width;
        height = s.height;
        return true;
    }
    return NodeCaptureInventory::instance().captureSize(target, filepath(), width, height);
}

bool CaptureLayer::captureOk() const {
    std::lock_guard<std::mutex> lock(m_backendMutex);
    return m_backend && m_backend->error().empty() && m_backend->hasSignal() && m_backend->textureId() != 0;
}

void CaptureLayer::masterPlaceholderSize(int& width, int& height) const {
    if (!nodeCaptureSize(width, height)) {
        width = kDefaultPlaceholderWidth;
        height = kDefaultPlaceholderHeight;
    }
}

std::string CaptureLayer::statusText() const {
    const std::string target = targetNode();
    if (isMaster() && !target.empty()) {
        // The capture runs on the target node only: show what that node reported.
        if (!NodeCaptureInventory::isReady())
            return target + ": no report";
        const NodeCaptureInventory::LayerStatus s = NodeCaptureInventory::instance().layerStatus(target, identifier());
        if (!s.nodeOnline)
            return target + ": node offline";
        if (!s.reported)
            return target + ": waiting for report";
        return target + (s.ok ? ": ready, " : ": ") + s.status;
    }
    std::shared_ptr<CaptureBackend> backend;
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        backend = m_backend;
    }
    if (!backend) {
        const CaptureSource src = resolveSource();
        if (isMaster() && src.placeholder) return "Master placeholder";
        if (!target.empty() && !src.valid()) return "Captured on " + target + " only";
        return src.valid() ? std::string("Starting") : std::string("No capture source on this machine");
    }
    const std::string err = backend->error();
    if (!err.empty())
        return err;
    if (!backend->hasSignal())
        return noSignalImageEnabled() ? "No signal (showing no-signal image)" : "No signal";
    if (backend->textureId() == 0)
        return "Waiting for frames";
    return std::format("{}x{}, {}", backend->width(), backend->height(), CaptureBackend::transferName(backend->transfer()));
}

std::string CaptureLayer::sourceText() const {
    const CaptureSource src = resolveSource();
    return src.valid() ? src.toString() : std::string();
}

void CaptureLayer::encodeTypeCore(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_presetKey);
    sgct::serializeObject(data, m_useNoSignalImage.load());
    sgct::serializeObject(data, targetNode());
}

void CaptureLayer::decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_presetKey);
    bool useNoSignalImage = false;
    sgct::deserializeObject(data, pos, useNoSignalImage);
    m_useNoSignalImage.store(useNoSignalImage);
    std::string target;
    sgct::deserializeObject(data, pos, target);
    std::lock_guard<std::mutex> lock(m_targetMutex);
    m_targetNode = std::move(target);
}

void CaptureLayer::encodeTypeAlways(std::vector<std::byte>& data) {
    m_subdivision->encodeAlways(data);
}

void CaptureLayer::decodeTypeAlways(const std::vector<std::byte>& data, unsigned int& pos) {
    m_subdivision->decodeAlways(data, pos);
}

void CaptureLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_volume);
    sgct::serializeObject(data, m_volumeMute.load());
    m_subdivision->encodeProperties(data);
}

void CaptureLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    bool mute = false;
    sgct::deserializeObject(data, pos, m_volume);
    sgct::deserializeObject(data, pos, mute);
    m_volumeMute.store(mute);
    m_subdivision->decodeProperties(data, pos);
}

bool CaptureLayer::hasAudio() const {
    return (m_sourceHasAudio.load() && isAudioEnabled()) || (isMaster() && AudioSettings::enableAudioOnNodes());
}

bool CaptureLayer::isAudioEnabled() const {
    return m_isAudioEnabled.load();
}

void CaptureLayer::enableAudio(bool enabled) {
    m_isAudioEnabled.store(enabled);
}

void CaptureLayer::updateAudioOutput() {
    if (isMaster())
        enableAudio(AudioSettings::enableAudioOnMaster());
    std::shared_ptr<CaptureBackend> backend;
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        backend = m_backend;
    }
    if (backend)
        backend->restartAudio();
}

void CaptureLayer::setVolume(int v, bool storeLevel) {
    if (storeLevel)
        m_volume = v;
    if (isMaster() && AudioSettings::enableAudioOnNodes())
        setNeedSync();
}

void CaptureLayer::setVolumeMute(bool v) {
    m_volumeMute.store(v);
}

bool CaptureLayer::ready() const {
    if (masterPlaceholderEnabled())
        return masterPlaceholderReady();
    return noSignalImageEnabled() || (renderData.texId > 0 && !noSignalHidden());
}

bool CaptureLayer::hasTexture() const {
    if (masterPlaceholderEnabled())
        return masterPlaceholderReady();
    return noSignalImageEnabled() || (renderData.texId > 0 && !noSignalHidden());
}

unsigned int CaptureLayer::textureInternalFormat() const {
    return masterPlaceholderEnabled() || noSignalImageEnabled() ? GL_RGBA8 : GL_RGB8;
}

void CaptureLayer::processPendingGLCleanup() {
    CaptureBackend::processPendingClose();
}

CaptureSource CaptureLayer::resolveSource() const {
    const std::string target = targetNode();
    if (!target.empty()) {
        if (isMaster()) {
            CaptureSource placeholder;
            placeholder.placeholder = true;
            return placeholder;
        }
        if (!CapturePresets::isThisMachine(target, false))
            return CaptureSource{"", 0, "", true};
        return CaptureSource::fromString(filepath());
    }
    if (!m_presetKey.empty()) {
        // A master-only layer has no nodes to show the capture: it never shows the placeholder.
        CaptureSource resolved;
        if (CapturePresets::resolve(m_presetKey, isMaster(), resolved, !existOnMasterOnly()))
            return resolved;
    }
    // No local entry for the preset (or a custom selection): use the synced default source.
    return CaptureSource::fromString(filepath());
}

void CaptureLayer::update(bool) {
    // Called with update(false) on the master and while !ready() on the nodes; the capture must
    // run in both cases since ready() depends on a captured frame.
    updateCapture();
}

void CaptureLayer::updateFrame() {
    updateCapture();
}

void CaptureLayer::updateCapture() {
    CaptureBackend::processPendingClose();

    const CaptureSource src = resolveSource();
    setMasterPlaceholder(src.placeholder);
    const std::string key = src.valid() ? src.toString() : std::string();
    if (key != m_backendKey) {
        std::shared_ptr<CaptureBackend> previous;
        {
            std::lock_guard<std::mutex> lock(m_backendMutex);
            previous.swap(m_backend);
        }
        // Release first, so a new source on the same input can reopen it.
        previous.reset();
        std::shared_ptr<CaptureBackend> next = key.empty() ? nullptr : CaptureBackend::acquire(src);
        {
            std::lock_guard<std::mutex> lock(m_backendMutex);
            m_backend = next;
        }
        m_backendKey = key;
        if (key.empty())
            clearLoadError();
        else if (!m_backend)
            setLoadError(key, "capture backend '" + src.backend + "' is not available in this build");
    }

    if (updateMasterPlaceholder())
        return;

    if (!m_backend) {
        updateNoSignalImage(false, {});
        m_sourceHasAudio.store(false);
        renderData.texId = 0;
        renderData.width = 0;
        renderData.height = 0;
        return;
    }

    m_backend->update();
    setLoadError(key, m_backend->error());

    m_sourceHasAudio.store(src.audio);
    const float volume = m_volumeMute.load() ? 0.f : static_cast<float>(m_volume) / 100.f * m_volumeScaling;
    m_backend->updateAudio(isAudioEnabled() && alpha() > 0.f, volume, audioLevelsEnabled(), audioSpatialSource());
    const float peak = m_backend->takeAudioPeak();
    if (peak >= 0.f)
        reportAudioLevel(peak);

    renderData.width = m_backend->width();
    renderData.height = m_backend->height();
    // The capture texture is shared by all layers showing this input: a QR control frame is
    // shown as this layer's copy of the last clean frame instead.
    renderData.texId = m_subdivision->filterControlFrame(m_backend->textureId(), renderData.width, renderData.height);
    m_subdivision->updateSubLayers(renderData.texId, renderData.width, renderData.height);
    // Not setFlipY(): that would mark the layer for re-sync whenever the transfer mode changes.
    renderData.flipY = m_backend->flipY();

    updateNoSignalImage(!m_backend->hasSignal() && m_backend->error().empty(), noSignalImageFile(LayerType::CAPTURE));
}
