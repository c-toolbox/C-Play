/*
 * SPDX-FileCopyrightText:
 * 2026 Erik Sunden <eriksunden85@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef CAPTURELAYER_H
#define CAPTURELAYER_H

#include <layers/baselayer.h>
#include <capture/capturebackend.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>

// Shows a capture card input read directly through the vendor SDK (see CaptureBackend), with GPU
// direct transfer on professional GPUs.
//
// The file path holds the default source (CaptureSource::toString()). A layer created from a
// predefined setup also carries its title as preset key, which each machine resolves against its
// local data/predefined-captures.json - so the master and the nodes may capture different inputs,
// or only some machines capture at all. Layers showing the same input share one capture.
class CaptureLayer : public BaseLayer {
public:
    CaptureLayer();
    ~CaptureLayer();

    void cleanup() override;
    void initialize() override;
    void update(bool updateRendering = true) override;
    void updateFrame() override;
    bool ready() const override;
    bool hasTexture() const override;
    unsigned int textureInternalFormat() const override;

    std::string presetKey() const;
    void setPresetKey(const std::string& key);

    // Short human readable state, e.g. "1920x1080, NVIDIA GPUDirect for Video".
    std::string statusText() const;

    void encodeTypeCore(std::vector<std::byte>& data) override;
    void decodeTypeCore(const std::vector<std::byte>& data, unsigned int& pos) override;
    void encodeTypeProperties(std::vector<std::byte>& data) override;
    void decodeTypeProperties(const std::vector<std::byte>& data, unsigned int& pos) override;

    // Audio plays on the machines whose resolved source has audio=true (see CapturePresets).
    bool hasAudio() const override;
    bool hasAudioLevels() const override { return hasAudio(); }
    bool isAudioEnabled() const override;
    void enableAudio(bool enabled = true) override;
    void updateAudioOutput() override;
    void setVolume(int v, bool storeLevel = true) override;
    void setVolumeMute(bool v) override;

    // Render thread: closes captures that were released on another thread.
    static void processPendingGLCleanup();

private:
    CaptureSource resolveSource() const;
    void updateCapture();

    std::string m_presetKey;
    mutable std::mutex m_backendMutex; // m_backend is swapped on the render thread, read by statusText()
    std::shared_ptr<CaptureBackend> m_backend;
    std::string m_backendKey;
    std::atomic<bool> m_isAudioEnabled{false};
    std::atomic<bool> m_sourceHasAudio{false};
    std::atomic<bool> m_volumeMute{false};
};

#endif // CAPTURELAYER_H
