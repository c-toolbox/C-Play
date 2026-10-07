/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "capturelayer.h"
#include "audiosettings.h"
#include <capture/capturepresets.h>
#include <sgct/opengl.h>
#include <sgct/sgct.h>
#include <format>

CaptureLayer::CaptureLayer() {
    setType(BaseLayer::LayerType::CAPTURE);
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

std::string CaptureLayer::statusText() const {
    std::shared_ptr<CaptureBackend> backend;
    {
        std::lock_guard<std::mutex> lock(m_backendMutex);
        backend = m_backend;
    }
    if (!backend) {
        const CaptureSource src = resolveSource();
        return src.valid() ? std::string("Starting") : std::string("No capture source on this machine");
    }
    const std::string err = backend->error();
    if (!err.empty())
        return err;
    if (!backend->hasSignal())
        return "No signal";
    if (backend->textureId() == 0)
        return "Waiting for frames";
    return std::format("{}x{}, {}", backend->width(), backend->height(), CaptureBackend::transferName(backend->transfer()));
}

void CaptureLayer::encodeTypeCore(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_presetKey);
}

void CaptureLayer::decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) {
    sgct::deserializeObject(data, pos, m_presetKey);
}

void CaptureLayer::encodeTypeProperties(std::vector<std::byte>& data) {
    sgct::serializeObject(data, m_volume);
    sgct::serializeObject(data, m_volumeMute.load());
}

void CaptureLayer::decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) {
    bool mute = false;
    sgct::deserializeObject(data, pos, m_volume);
    sgct::deserializeObject(data, pos, mute);
    m_volumeMute.store(mute);
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
    return renderData.texId > 0;
}

bool CaptureLayer::hasTexture() const {
    return renderData.texId > 0;
}

unsigned int CaptureLayer::textureInternalFormat() const {
    return GL_RGB8;
}

void CaptureLayer::processPendingGLCleanup() {
    CaptureBackend::processPendingClose();
}

CaptureSource CaptureLayer::resolveSource() const {
    if (!m_presetKey.empty()) {
        CaptureSource resolved;
        if (CapturePresets::resolve(m_presetKey, isMaster(), resolved))
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

    if (!m_backend) {
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
    m_backend->updateAudio(isAudioEnabled() && alpha() > 0.f, volume, audioLevelsEnabled());
    const float peak = m_backend->takeAudioPeak();
    if (peak >= 0.f)
        reportAudioLevel(peak);

    renderData.texId = m_backend->textureId();
    renderData.width = m_backend->width();
    renderData.height = m_backend->height();
    // Not setFlipY(): that would mark the layer for re-sync whenever the transfer mode changes.
    renderData.flipY = m_backend->flipY();
}
