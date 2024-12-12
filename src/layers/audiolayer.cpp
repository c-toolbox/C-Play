#include "audiolayer.h"

AudioLayer::AudioLayer(gl_adress_func_v1 opa,
    bool allowDirectRendering,
    bool loggingOn,
    std::string logLevel) : MpvLayer(opa, allowDirectRendering, loggingOn, logLevel) {
    m_data.supportVideo = false;
    m_existOnMasterOnly = true;
    setType(BaseLayer::LayerType::AUDIO);
}

AudioLayer::~AudioLayer() {
    cleanup();
}

bool AudioLayer::ready() const {
    return !m_data.loadedFile.empty();
}
