#ifndef STREAMLAYER_H
#define STREAMLAYER_H

#include <layers/videolayer.h>

class StreamLayer : public VideoLayer {
public:
    StreamLayer(gl_adress_func_v1 opa,
        bool allowDirectRendering = false,
        bool loggingOn = false,
        std::string logLevel = "info",
        MpvLayer::onFileLoadedCallback flc = nullptr);
    ~StreamLayer();

    void initialize();
    bool ready() const;

private:
};

#endif // STREAMLAYER_H