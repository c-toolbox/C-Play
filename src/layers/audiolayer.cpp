#include "audiolayer.h"

AudioLayer::AudioLayer(opengl_func_adress_ptr opa,
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

bool AudioLayer::ready() {
    return !m_data.loadedFile.empty();
}
